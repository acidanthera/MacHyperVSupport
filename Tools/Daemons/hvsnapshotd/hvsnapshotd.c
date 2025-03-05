//
//  hvsnapshotd.c
//  Hyper-V VSS snapshot userspace daemon
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "HyperVSnapshotUserClient.h"
#include "hvdebug.h"
#include "hviokit.h"

#define HYPERV_SNAPSHOT_KERNEL_SERVICE "HyperVSnapshot"

HVDeclareLogFunctionsUser("hvsnapshotd");

void hvSnapshotCallUserClientMethod(io_connect_t connection, UInt32 selector, IOReturn status) {
  IOReturn callStatus;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
  //
  // Call into user client with standard API.
  //
  UInt64 input64[2];
  input64[0] = kHyperVSnapshotMagic;
  input64[1] = status;
  callStatus = IOConnectCallScalarMethod(connection, selector, input64, 2, NULL, NULL);
#else
  //
  // Call into user client with legacy API.
  //
  callStatus = IOConnectMethodScalarIScalarO(connection, selector, 2, 0, kHyperVSnapshotMagic, status);
#endif

  if (callStatus != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failed to call user client method %u with status 0x%X", selector, status);
  }
}

void hvSnapshotDoSnapshotCheck(io_connect_t connection) {
  IOReturn status = kIOReturnSuccess;

  hvSnapshotCallUserClientMethod(connection, kHyperVSnapshotUserClientMethodReportSnapshotAbility, status);
}

void hvSnapshotDoFreeze(io_connect_t connection) {
  HVDBGLOG(stdout, "Starting freeze of filesystems");
  
  IOReturn status = kIOReturnSuccess;

  HVDBGLOG(stdout, "Freeze of filesystems completed with status: 0x%X", status);
  hvSnapshotCallUserClientMethod(connection, kHyperVSnapshotUserClientMethodCompleteFreeze, status);
}

void hvSnapshotDoThaw(io_connect_t connection) {
  HVDBGLOG(stdout, "Starting thaw of filesystems");
  
  IOReturn status = kIOReturnSuccess;

  HVDBGLOG(stdout, "Thaw of filesystems completed with status: 0x%X", status);
  hvSnapshotCallUserClientMethod(connection, kHyperVSnapshotUserClientMethodCompleteThaw, status);
}

void hvIOKitNotificationHandler(io_connect_t connection, CFMachPortRef port, void *msg, CFIndex size, void *info) {
  HyperVSnapshotUserClientNotificationMessage *snapshotMsg = (HyperVSnapshotUserClientNotificationMessage *) msg;

  if (size < __offsetof(HyperVSnapshotUserClientNotificationMessage, type)) {
    HVSYSLOG(stderr, "Invalid message size %u received, should be at least %u",
             size, __offsetof(HyperVSnapshotUserClientNotificationMessage, type));
    return;
  }

  HVDBGLOG(stdout, "Received notification of type 0x%X", snapshotMsg->type);
  switch (snapshotMsg->type) {
    //
    // Always returns magic value, means daemon is alive and can handle a snapshot request.
    //
    case kHyperVSnapshotUserClientNotificationTypeCheck:
      hvSnapshotDoSnapshotCheck(connection);
      break;

    case kHyperVSnapshotUserClientNotificationTypeFreeze:
      hvSnapshotDoFreeze(connection);
      break;

    case kHyperVSnapshotUserClientNotificationTypeThaw:
      hvSnapshotDoThaw(connection);
      break;

    default:
      HVDBGLOG(stdout, "Unknown notification type 0x%X", snapshotMsg->type);
      break;
  }
}

int main(int argc, const char * argv[]) {
 /* HVSYSLOG(stdout, "started");
  
  // Get /.
  
  int fd = open("/System/Volumes/Data", O_RDONLY);
  
  HVSYSLOG(stdout, "Open /System/Volumes/Data 0x%X", fd);
  
  int ret = fcntl(fd, F_FREEZE_FS, 0);
  
  HVSYSLOG(stdout, "Is frozen: 0x%X", ret);
  
  sleep(10);
  
  ret = fcntl(fd, F_THAW_FS, 0);
  
  HVSYSLOG(stdout, "Is thawed: 0x%X", ret);*/
  
  //
  // Setup I/O Kit notifications.
  //
  if (hvIOKitSetupIOKitNotifications(HYPERV_SNAPSHOT_KERNEL_SERVICE) != kIOReturnSuccess) {
    return -1;
  }

  //
  // Run main loop, this should not return.
  //
  CFRunLoopRun();
  return 0;
}
