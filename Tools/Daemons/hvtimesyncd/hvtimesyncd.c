//
//  hvtimesyncd.c
//  Hyper-V time synchronization userspace daemon
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVTimeSyncUserClient.h"
#include "hvdebug.h"
#include "hviokit.h"

#include <sys/time.h>

#define HYPERV_TIME_SYNC_KERNEL_SERVICE "HyperVTimeSync"

HVDeclareLogFunctionsUser("hvtimesyncd");

void hvIOKitNotificationHandler(io_connect_t connection, CFMachPortRef port, void *msg, CFIndex size, void *info) {
  HyperVTimeSyncUserClientNotificationMessage *timeSyncMsg = (HyperVTimeSyncUserClientNotificationMessage *) msg;
  struct timeval timeValData;

  if (size < __offsetof(HyperVTimeSyncUserClientNotificationMessage, microseconds)) {
    HVSYSLOG(stderr, "Invalid message size %u received, should be at least %u",
             size, __offsetof(HyperVTimeSyncUserClientNotificationMessage, microseconds));
    return;
  }

  HVDBGLOG(stdout, "Got new time data (seconds: %llu, microseconds: %u)", timeSyncMsg->seconds, timeSyncMsg->microseconds);

  timeValData.tv_sec  = timeSyncMsg->seconds;
  timeValData.tv_usec = timeSyncMsg->microseconds;
  settimeofday(&timeValData, NULL);
}

int main(int argc, const char * argv[]) {
  //
  // Setup I/O Kit notifications.
  //
  if (hvIOKitSetupIOKitNotifications(HYPERV_TIME_SYNC_KERNEL_SERVICE) != kIOReturnSuccess) {
    return -1;
  }

  //
  // Run main loop, this should not return.
  //
  CFRunLoopRun();
  return 0;
}
