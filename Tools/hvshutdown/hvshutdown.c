//
//  hvshutdown.c
//  Hyper-V guest shutdown daemon
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include <mach/mach_port.h>
#include <IOKit/IOKitLib.h>

#include "hvdebug.h"
#include "HyperVShutdownUserClient.h"

#define HVSHUTDOWN_KERNEL_SERVICE   "HyperVShutdown"
#define SHUTDOWN_BIN_PATH           "/sbin/shutdown"

HVDeclareLogFunctionsUser("hvshutdown");

static IONotificationPortRef  sIOKitNotificationPort                  = NULL;
static CFRunLoopSourceRef     sIOKitNotificationCFRunLoopSource       = NULL;

static io_service_t           sDevice                                 = 0;
static io_connect_t           sConnection                             = 0;
static io_iterator_t          sHVShutdownAppearedIterator             = 0;
static io_iterator_t          sHVShutdownRemovedIterator              = 0;

static mach_port_t            sHVShutdownNotificationPort             = MACH_PORT_NULL;
static CFMachPortRef          sHVShutdownNotificationCFMachPort       = NULL;
static CFRunLoopSourceRef     sHVShutdownNotificationCFRunLoopSource  = NULL;

static void hvShutdownNotification(CFMachPortRef port, void *msg, CFIndex size, void *info) {
  HyperVShutdownNotificationMessage *hvMsg = (HyperVShutdownNotificationMessage *) msg;
  HVDBGLOG(stdout, "Received notification of type 0x%X", hvMsg->type);

  if (hvMsg->type == kHyperVShutdownNotificationTypePerformShutdown) {
    HVSYSLOG(stdout, "Shutdown request received, performing shutdown");

    //
    // Shutdown has been requested, invoke /sbin/shutdown.
    //
    char *shutdownArgs[] = {
      SHUTDOWN_BIN_PATH,
      "-h",
      "now",
      NULL
    };

    //
    // This should not return.
    //
    int ret = execv(shutdownArgs[0], shutdownArgs);
    if (ret == -1) {
      HVSYSLOG(stderr, "Failed to execute %s", shutdownArgs[0]);
    }

  } else if (hvMsg->type == kHyperVShutdownNotificationTypePerformRestart) {
    HVSYSLOG(stdout, "Restart request received, performing restart");

    //
    // Restart has been requested, invoke /sbin/shutdown.
    //
    char *restartArgs[] = {
      SHUTDOWN_BIN_PATH,
      "-r",
      "now",
      NULL
    };

    //
    // This should not return.
    //
    int ret = execv(restartArgs[0], restartArgs);
    if (ret == -1) {
      HVSYSLOG(stderr, "Failed to execute %s", restartArgs[0]);
    }
  }
}

static void hvShutdownTeardownNotification() {
  if (sConnection) {
    IOServiceClose(sConnection);
    sConnection = 0;
  }

  if (sDevice != 0) {
    IOObjectRelease(sDevice);
    sDevice = 0;
  }

  if (sHVShutdownNotificationCFRunLoopSource != NULL) {
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), sHVShutdownNotificationCFRunLoopSource, kCFRunLoopCommonModes);
    CFRelease(sHVShutdownNotificationCFRunLoopSource);
    sHVShutdownNotificationCFRunLoopSource = NULL;
  }

  if (sHVShutdownNotificationCFMachPort != NULL) {
    CFMachPortInvalidate(sHVShutdownNotificationCFMachPort);
    CFRelease(sHVShutdownNotificationCFMachPort);
    sHVShutdownNotificationCFMachPort = NULL;
  }

  if (sHVShutdownNotificationPort != MACH_PORT_NULL) {
    mach_port_destroy(mach_task_self(), sHVShutdownNotificationPort);
    sHVShutdownNotificationPort = MACH_PORT_NULL;
  }

  HVDBGLOG(stdout, "Service closed");
}

static IOReturn hvShutdownSetupNotification(io_service_t device) {
  IOReturn          result;
  CFMachPortContext context;

  //
  // Setup notification for shutdown requests.
  //
  context.version = 1;
  context.info = &context;
  context.retain = NULL;
  context.release = NULL;
  context.copyDescription = NULL;

  result = IOCreateReceivePort(kOSAsyncCompleteMessageID, &sHVShutdownNotificationPort);
  if (result != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failure while creating notification port: 0x%X", result);
    return result;
  }

  sHVShutdownNotificationCFMachPort = CFMachPortCreateWithPort(kCFAllocatorDefault, sHVShutdownNotificationPort, hvShutdownNotification, &context, NULL);
  if (sHVShutdownNotificationCFMachPort == NULL) {
    HVSYSLOG(stderr, "Failed while creating notification CFMachPort");
    hvShutdownTeardownNotification();
    return kIOReturnNoResources;
  }

  sHVShutdownNotificationCFRunLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, sHVShutdownNotificationCFMachPort, 0);
  if (sHVShutdownNotificationCFRunLoopSource == NULL) {
    HVSYSLOG(stderr, "Failed while creating notification run loop source");
    hvShutdownTeardownNotification();
    return kIOReturnNoResources;
  }
  CFRunLoopAddSource(CFRunLoopGetCurrent(), sHVShutdownNotificationCFRunLoopSource, kCFRunLoopDefaultMode);

  //
  // Connect to service.
  //
  sDevice = device;
  result = IOServiceOpen(sDevice, mach_task_self(), 0, &sConnection);
  if (result != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failure while opening service: 0x%X\n", result);
    hvShutdownTeardownNotification();
    return result;
  }
  HVDBGLOG(stdout, "Opened connection to service");

  result = IOConnectSetNotificationPort(sConnection, 0, sHVShutdownNotificationPort, 0);
  if (result != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failed to set notification port: 0x%X", result);
    hvShutdownTeardownNotification();
    return result;
  }
  HVDBGLOG(stdout, "Port 0x%p setup for notifications", sHVShutdownNotificationPort);

  return kIOReturnSuccess;
}

static void hvShutdownAppeared(void *refCon, io_iterator_t iterator) {
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
    result = hvShutdownSetupNotification(device);
    if (result != kIOReturnSuccess) {
      return;
    }
  }
}

static void hvShutdownRemoved(void *refCon, io_iterator_t iterator) {
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
    hvShutdownTeardownNotification();
  }
}

static IOReturn hvShutdownSetupIOKitNotifications() {
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
  matching = IOServiceMatching(HVSHUTDOWN_KERNEL_SERVICE);
  if (matching == NULL) {
    HVSYSLOG(stderr, "Failure while creating matching dictionary");
    return kIOReturnNoResources;
  }
  matching = (CFMutableDictionaryRef)CFRetain(matching);

  result = IOServiceAddMatchingNotification(sIOKitNotificationPort,
                                            kIOPublishNotification,
                                            matching,
                                            hvShutdownAppeared,
                                            NULL,
                                            &sHVShutdownAppearedIterator);
  if (result != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failure while adding matching notification (published): 0x%X", result);
    return result;
  }
  hvShutdownAppeared(NULL, sHVShutdownAppearedIterator);

  result = IOServiceAddMatchingNotification(sIOKitNotificationPort,
                                            kIOTerminatedNotification,
                                            matching,
                                            hvShutdownRemoved,
                                            NULL,
                                            &sHVShutdownRemovedIterator);
  if (result != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failure while adding matching notification (terminated): 0x%X", result);
    return result;
  }
  hvShutdownRemoved(NULL, sHVShutdownRemovedIterator);

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
  if (hvShutdownSetupIOKitNotifications() != kIOReturnSuccess) {
    return -1;
  }

  //
  // Run main loop, this should not return.
  //
  CFRunLoopRun();
  return 0;
}
