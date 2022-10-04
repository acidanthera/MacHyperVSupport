//
//  hvutil.c
//  Hyper-V guest utility daemon
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include <mach/mach_port.h>
#include <IOKit/IOKitLib.h>

#include <sys/time.h>

#include <iconv.h>

#include "hvdebug.h"
#include "HyperVUserClient.h"

#define HYPERV_CONTROLLER_KERNEL_SERVICE   "HyperVController"
#define SHUTDOWN_BIN_PATH                 "/sbin/shutdown"

HVDeclareLogFunctionsUser("util");

static IONotificationPortRef  sIOKitNotificationPort                  = NULL;
static CFRunLoopSourceRef     sIOKitNotificationCFRunLoopSource       = NULL;

static io_service_t           sDevice                                 = 0;
static io_connect_t           sConnection                             = 0;
static io_iterator_t          shvUtilAppearedIterator             = 0;
static io_iterator_t          shvUtilRemovedIterator              = 0;

static mach_port_t            shvUtilNotificationPort             = MACH_PORT_NULL;
static CFMachPortRef          shvUtilNotificationCFMachPort       = NULL;
static CFRunLoopSourceRef     shvUtilNotificationCFRunLoopSource  = NULL;

static void hvUtilDoShutdown(bool restart) {
  HVSYSLOG(stdout, "Shutdown request received, performing shutdown (restart %u)", restart);

  //
  // Shutdown/restart has been requested, invoke /sbin/shutdown.
  //
  char *shutdownArgs[] = {
    SHUTDOWN_BIN_PATH,
    restart ? "-r" : "-h",
    "now",
    "Hyper-V Guest Shutdown initiated",
    NULL
  };

  //
  // This should not return.
  //
  int ret = execv(shutdownArgs[0], shutdownArgs);
  if (ret == -1) {
    HVSYSLOG(stderr, "Failed to execute %s", shutdownArgs[0]);
  }
}

static void hvUtilDoTimeSync(void *data, UInt32 dataLength) {
  HyperVUserClientTimeData *timeData;
  struct timeval timeValData;

  //
  // Get received epoch.
  //
  if (dataLength != sizeof (*timeData)) {
    HVSYSLOG(stderr, "Incorrect data size for time sync data");
    return;
  }
  timeData = (HyperVUserClientTimeData *) data;
  HVDBGLOG(stdout, "Got new time data (seconds: %llu, microseconds: %llu)", timeData->seconds, timeData->microseconds);

  timeValData.tv_sec = timeData->seconds;
  timeValData.tv_usec = timeData->microseconds;
  settimeofday(&timeValData, NULL);
}

static void hvUtilFileCopyStartCopy(HyperVUserClientFileCopyStartCopy *startCopyMsg) {
  UInt16 *origFileName;
  UInt16 *origFilePath;
  UInt8 *utf8FileName;
  UInt8 *utf8FilePath;
  UInt8 *utf8FileNameOrig;
  UInt8 *utf8FilePathOrig;
  size_t origFileNameSize = sizeof (startCopyMsg->fileName);
  size_t utf8FileNameSize = sizeof (startCopyMsg->fileName) / 2;
  size_t origFilePathSize = sizeof (startCopyMsg->filePath);
  size_t utf8FilePathSize = sizeof (startCopyMsg->filePath) / 2;
  
  iconv_t iconvFcopy = iconv_open("UTF-8", "UTF-16LE");
  if ((int) iconvFcopy == -1) {
    if (errno == EINVAL) { HVDBGLOG(stderr, "Unsupported conversion"); }
    else { HVDBGLOG(stderr, "Initialization failure"); }
  }
  
  origFileName = &startCopyMsg->fileName;
  origFilePath = &startCopyMsg->filePath;
  utf8FileName = malloc(utf8FileNameSize);
  utf8FilePath = malloc(utf8FilePathSize);
  memset(utf8FileName, 0, utf8FileNameSize);
  memset(utf8FilePath, 0, utf8FilePathSize);
  utf8FileNameOrig = utf8FileName;
  utf8FilePathOrig = utf8FilePath;
  
  
  int ret1 = iconv(iconvFcopy, &origFileName, &origFileNameSize, &utf8FileName, &utf8FileNameSize);
  int ret2 = iconv(iconvFcopy, NULL, NULL, NULL, NULL);
  int ret3 = iconv(iconvFcopy, &origFilePath, &origFilePathSize, &utf8FilePath, &utf8FilePathSize);
  
  if (ret1 == (iconv_t) -1 || ret2 == (iconv_t) -1 || ret3 == (iconv_t) -1) {
    HVSYSLOG(stderr, "Failed to convert with iconv");
    free(utf8FileNameOrig);
    free(utf8FilePathOrig);
    iconv_close(iconvFcopy);
    return;
  }
  
  HVSYSLOG(stdout, "%s | %s", utf8FileNameOrig, utf8FilePathOrig);
  free(utf8FileNameOrig);
  free(utf8FilePathOrig);
  iconv_close(iconvFcopy);
}

static void hvUtilFileCopy(void *data, UInt32 dataLength) {
  HyperVUserClientFileCopy *fcopyMsg;
  if (dataLength > sizeof (*fcopyMsg)) {
    HVSYSLOG(stderr, "File copy packet larger than expected");
    return;
  }
  fcopyMsg = (HyperVUserClientFileCopy *) data;
  
  switch (fcopyMsg->operation) {
    case kHyperVUserClientFileCopyOperationStartFileCopy:
      hvUtilFileCopyStartCopy(&fcopyMsg->operationData.startCopy);
    default:
      HVDBGLOG(stdout, "Unknown file copy operation type 0x%X", fcopyMsg->operation);
      break;
  }
}

static void hvUtilNotification(CFMachPortRef port, void *msg, CFIndex size, void *info) {
  HyperVUserClientNotificationMessage *hvMsg = (HyperVUserClientNotificationMessage *) msg;
  HVDBGLOG(stdout, "Received notification of type 0x%X", hvMsg->standard.type);

  switch (hvMsg->standard.type) {
    case kHyperVUserClientNotificationTypePerformShutdown:
    case kHyperVUserClientNotificationTypePerformRestart:
      hvUtilDoShutdown(hvMsg->standard.type == kHyperVUserClientNotificationTypePerformRestart);
      break;
      
    case kHyperVUserClientNotificationTypeTimeSync:
      hvUtilDoTimeSync(hvMsg->standard.data, hvMsg->standard.dataLength);
      break;
    
    case kHyperVUserClientNotificationTypeFileCopy:
      hvUtilFileCopy(hvMsg->large.data, hvMsg->large.dataLength);
      break;

    default:
      HVDBGLOG(stdout, "Unknown notification type 0x%X", hvMsg->standard.type);
      break;
  }
}

static void hvUtilTeardownNotification() {
  if (sConnection) {
    IOServiceClose(sConnection);
    sConnection = 0;
  }

  if (sDevice != 0) {
    IOObjectRelease(sDevice);
    sDevice = 0;
  }

  if (shvUtilNotificationCFRunLoopSource != NULL) {
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), shvUtilNotificationCFRunLoopSource, kCFRunLoopCommonModes);
    CFRelease(shvUtilNotificationCFRunLoopSource);
    shvUtilNotificationCFRunLoopSource = NULL;
  }

  if (shvUtilNotificationCFMachPort != NULL) {
    CFMachPortInvalidate(shvUtilNotificationCFMachPort);
    CFRelease(shvUtilNotificationCFMachPort);
    shvUtilNotificationCFMachPort = NULL;
  }

  if (shvUtilNotificationPort != MACH_PORT_NULL) {
    mach_port_destroy(mach_task_self(), shvUtilNotificationPort);
    shvUtilNotificationPort = MACH_PORT_NULL;
  }

  HVDBGLOG(stdout, "Service closed");
}

static IOReturn hvUtilSetupNotification(io_service_t device) {
  IOReturn          result;
  CFMachPortContext context;

  //
  // Setup notification for userspace requests.
  //
  context.version = 1;
  context.info = &context;
  context.retain = NULL;
  context.release = NULL;
  context.copyDescription = NULL;

  result = IOCreateReceivePort(kOSAsyncCompleteMessageID, &shvUtilNotificationPort);
  if (result != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failure while creating notification port: 0x%X", result);
    return result;
  }

  shvUtilNotificationCFMachPort = CFMachPortCreateWithPort(kCFAllocatorDefault, shvUtilNotificationPort, hvUtilNotification, &context, NULL);
  if (shvUtilNotificationCFMachPort == NULL) {
    HVSYSLOG(stderr, "Failed while creating notification CFMachPort");
    hvUtilTeardownNotification();
    return kIOReturnNoResources;
  }

  shvUtilNotificationCFRunLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, shvUtilNotificationCFMachPort, 0);
  if (shvUtilNotificationCFRunLoopSource == NULL) {
    HVSYSLOG(stderr, "Failed while creating notification run loop source");
    hvUtilTeardownNotification();
    return kIOReturnNoResources;
  }
  CFRunLoopAddSource(CFRunLoopGetCurrent(), shvUtilNotificationCFRunLoopSource, kCFRunLoopDefaultMode);

  //
  // Connect to service.
  //
  sDevice = device;
  result = IOServiceOpen(sDevice, mach_task_self(), 0, &sConnection);
  if (result != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failure while opening service: 0x%X\n", result);
    hvUtilTeardownNotification();
    return result;
  }
  HVDBGLOG(stdout, "Opened connection to service");

  result = IOConnectSetNotificationPort(sConnection, 0, shvUtilNotificationPort, 0);
  if (result != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failed to set notification port: 0x%X", result);
    hvUtilTeardownNotification();
    return result;
  }
  HVDBGLOG(stdout, "Port 0x%p setup for notifications", shvUtilNotificationPort);

  return kIOReturnSuccess;
}

static void hvUtilAppeared(void *refCon, io_iterator_t iterator) {
  IOReturn      result;
  io_service_t  obj;
  io_service_t  device = 0;

  while ((obj = IOIteratorNext(iterator))) {
    if (!device) {
      device = obj;
    } else {
      IOObjectRelease(obj);
    }
  }

  if (device) {
    result = hvUtilSetupNotification(device);
    if (result != kIOReturnSuccess) {
      return;
    }
  }
}

static void hvUtilRemoved(void *refCon, io_iterator_t iterator) {
  io_service_t  obj;
  bool          deviceIsRemoved = false;

  while ((obj = IOIteratorNext(iterator))) {
    if (sDevice && IOObjectIsEqualTo(obj, sDevice)) {
      deviceIsRemoved = true;
    }

    IOObjectRelease(obj);
  }

  //
  // Close connection to service.
  //
  if (deviceIsRemoved) {
    hvUtilTeardownNotification();
  }
}

static IOReturn hvUtilSetupIOKitNotifications() {
  CFMutableDictionaryRef  matching;
  IOReturn                result;

  //
  // Create notification port for I/O Kit notifications.
  //
  sIOKitNotificationPort = IONotificationPortCreate(kIOMasterPortDefault);
  if (sIOKitNotificationPort == NULL) {
    HVSYSLOG(stderr, "Failure while creating notification port");
    return kIOReturnNoResources;
  }

  //
  // Configure matching notifications.
  // An extra retain call is required as IOServiceAddMatchingNotification
  // will release the dictionary (called twice).
  //
  matching = IOServiceMatching(HYPERV_CONTROLLER_KERNEL_SERVICE);
  if (matching == NULL) {
    HVSYSLOG(stderr, "Failure while creating matching dictionary");
    return kIOReturnNoResources;
  }
  matching = (CFMutableDictionaryRef)CFRetain(matching);

  result = IOServiceAddMatchingNotification(sIOKitNotificationPort,
                                            kIOPublishNotification,
                                            matching,
                                            hvUtilAppeared,
                                            NULL,
                                            &shvUtilAppearedIterator);
  if (result != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failure while adding matching notification (published): 0x%X", result);
    return result;
  }
  hvUtilAppeared(NULL, shvUtilAppearedIterator);

  result = IOServiceAddMatchingNotification(sIOKitNotificationPort,
                                            kIOTerminatedNotification,
                                            matching,
                                            hvUtilRemoved,
                                            NULL,
                                            &shvUtilRemovedIterator);
  if (result != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failure while adding matching notification (terminated): 0x%X", result);
    return result;
  }
  hvUtilRemoved(NULL, shvUtilRemovedIterator);

  sIOKitNotificationCFRunLoopSource = IONotificationPortGetRunLoopSource(sIOKitNotificationPort);
  if (sIOKitNotificationCFRunLoopSource == NULL) {
    HVSYSLOG(stderr, "Failed while creating matching notification run loop source");
    return kIOReturnNoResources;
  }
  CFRunLoopAddSource(CFRunLoopGetCurrent(), sIOKitNotificationCFRunLoopSource, kCFRunLoopDefaultMode);

  return kIOReturnSuccess;
}

int main(int argc, const char * argv[]) {
  //
  // Setup I/O Kit notifications.
  //
  if (hvUtilSetupIOKitNotifications() != kIOReturnSuccess) {
    return -1;
  }

  //
  // Run main loop, this should not return.
  //
  CFRunLoopRun();
  return 0;
}
