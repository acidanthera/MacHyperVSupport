//
//  hviokit.c
//  Hyper-V userspace I/O Kit support
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "hviokit.h"
#include "hvdebug.h"

static IONotificationPortRef  sIOKitNotificationPort            = NULL;
static CFRunLoopSourceRef     sIOKitNotificationCFRunLoopSource = NULL;

static io_service_t           sDevice                           = 0;
static io_connect_t           sConnection                       = 0;
static io_iterator_t          sHvIOKitAppearedIterator          = 0;
static io_iterator_t          sHvIOKitRemovedIterator           = 0;

static mach_port_t            sHvNotificationPort               = MACH_PORT_NULL;
static CFMachPortRef          sHvNotificationCFMachPort         = NULL;
static CFRunLoopSourceRef     sHvNotificationCFRunLoopSource    = NULL;

static void hvIOKitNotification(CFMachPortRef port, void *msg, CFIndex size, void *info) {
  hvIOKitNotificationHandler(sConnection, port, msg, size, info);
}

static void hvIOKitTeardownNotification() {
  if (sConnection) {
    IOServiceClose(sConnection);
    sConnection = 0;
  }

  if (sDevice != 0) {
    IOObjectRelease(sDevice);
    sDevice = 0;
  }

  if (sHvNotificationCFRunLoopSource != NULL) {
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), sHvNotificationCFRunLoopSource, kCFRunLoopCommonModes);
    CFRelease(sHvNotificationCFRunLoopSource);
    sHvNotificationCFRunLoopSource = NULL;
  }

  if (sHvNotificationCFMachPort != NULL) {
    CFMachPortInvalidate(sHvNotificationCFMachPort);
    CFRelease(sHvNotificationCFMachPort);
    sHvNotificationCFMachPort = NULL;
  }

  if (sHvNotificationPort != MACH_PORT_NULL) {
    mach_port_destroy(mach_task_self(), sHvNotificationPort);
    sHvNotificationPort = MACH_PORT_NULL;
  }

  HVDBGLOG(stdout, "Service closed");
}

static IOReturn hvIOKitSetupNotification(io_service_t device) {
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

  result = IOCreateReceivePort(kOSAsyncCompleteMessageID, &sHvNotificationPort);
  if (result != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failure while creating notification port: 0x%X", result);
    return result;
  }

  sHvNotificationCFMachPort = CFMachPortCreateWithPort(kCFAllocatorDefault, sHvNotificationPort, hvIOKitNotification, &context, NULL);
  if (sHvNotificationCFMachPort == NULL) {
    HVSYSLOG(stderr, "Failed while creating notification CFMachPort");
    hvIOKitTeardownNotification();
    return kIOReturnNoResources;
  }

  sHvNotificationCFRunLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, sHvNotificationCFMachPort, 0);
  if (sHvNotificationCFRunLoopSource == NULL) {
    HVSYSLOG(stderr, "Failed while creating notification run loop source");
    hvIOKitTeardownNotification();
    return kIOReturnNoResources;
  }
  CFRunLoopAddSource(CFRunLoopGetCurrent(), sHvNotificationCFRunLoopSource, kCFRunLoopDefaultMode);

  //
  // Connect to service.
  //
  sDevice = device;
  result = IOServiceOpen(sDevice, mach_task_self(), 0, &sConnection);
  if (result != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failure while opening service: 0x%X\n", result);
    hvIOKitTeardownNotification();
    return result;
  }
  HVDBGLOG(stdout, "Opened connection to service");

  result = IOConnectSetNotificationPort(sConnection, 0, sHvNotificationPort, 0);
  if (result != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failed to set notification port: 0x%X", result);
    hvIOKitTeardownNotification();
    return result;
  }
  HVDBGLOG(stdout, "Port 0x%p setup for notifications", sHvNotificationPort);

  return kIOReturnSuccess;
}

static void hvIOKitAppeared(void *refCon, io_iterator_t iterator) {
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
    result = hvIOKitSetupNotification(device);
    if (result != kIOReturnSuccess) {
      return;
    }
  }
}

static void hvIOKitRemoved(void *refCon, io_iterator_t iterator) {
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
    hvIOKitTeardownNotification();
  }
}

IOReturn hvIOKitSetupIOKitNotifications(const char *name) {
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
  matching = IOServiceMatching(name);
  if (matching == NULL) {
    HVSYSLOG(stderr, "Failure while creating matching dictionary");
    return kIOReturnNoResources;
  }
  matching = (CFMutableDictionaryRef)CFRetain(matching);

  result = IOServiceAddMatchingNotification(sIOKitNotificationPort,
                                            kIOPublishNotification,
                                            matching,
                                            hvIOKitAppeared,
                                            NULL,
                                            &sHvIOKitAppearedIterator);
  if (result != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failure while adding matching notification (published): 0x%X", result);
    return result;
  }
  hvIOKitAppeared(NULL, sHvIOKitAppearedIterator);

  result = IOServiceAddMatchingNotification(sIOKitNotificationPort,
                                            kIOTerminatedNotification,
                                            matching,
                                            hvIOKitRemoved,
                                            NULL,
                                            &sHvIOKitRemovedIterator);
  if (result != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failure while adding matching notification (terminated): 0x%X", result);
    return result;
  }
  hvIOKitRemoved(NULL, sHvIOKitRemovedIterator);

  sIOKitNotificationCFRunLoopSource = IONotificationPortGetRunLoopSource(sIOKitNotificationPort);
  if (sIOKitNotificationCFRunLoopSource == NULL) {
    HVSYSLOG(stderr, "Failed while creating matching notification run loop source");
    return kIOReturnNoResources;
  }
  CFRunLoopAddSource(CFRunLoopGetCurrent(), sIOKitNotificationCFRunLoopSource, kCFRunLoopDefaultMode);

  return kIOReturnSuccess;
}
