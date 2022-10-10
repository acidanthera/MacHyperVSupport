//
//  hvshutdownd.c
//  Hyper-V Guest Shutdown userspace daemon
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVShutdownUserClient.h"
#include "hvdebug.h"
#include "hviokit.h"

#define HYPERV_SHUTDOWN_KERNEL_SERVICE   "HyperVShutdown"
#define SHUTDOWN_BIN_PATH                "/sbin/shutdown"

HVDeclareLogFunctionsUser("hvshutdownd");

static void hvShutdownDoShutdown(bool restart) {
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

void hvIOKitNotificationHandler(io_connect_t connection, CFMachPortRef port, void *msg, CFIndex size, void *info) {
  uint64_t input[1];

  HyperVShutdownUserClientNotificationMessage *hvMsg = (HyperVShutdownUserClientNotificationMessage *) msg;
  HVDBGLOG(stdout, "Received notification of type 0x%X", hvMsg->type);

  switch (hvMsg->type) {
    case kHyperVShutdownUserClientNotificationTypeCheck:
      input[0] = true;
      IOConnectCallScalarMethod(connection, kHyperVShutdownUserClientMethodReportShutdownAbility, input, 1, NULL, NULL);
      break;

    case kHyperVShutdownUserClientNotificationTypePerformShutdown:
    case kHyperVShutdownUserClientNotificationTypePerformRestart:
      hvShutdownDoShutdown(hvMsg->type == kHyperVShutdownUserClientNotificationTypePerformRestart);
      break;

    default:
      HVDBGLOG(stdout, "Unknown notification type 0x%X", hvMsg->type);
      break;
  }
}

int main(int argc, const char * argv[]) {
  //
  // Setup I/O Kit notifications.
  //
  if (hvIOKitSetupIOKitNotifications(HYPERV_SHUTDOWN_KERNEL_SERVICE) != kIOReturnSuccess) {
    return -1;
  }

  //
  // Run main loop, this should not return.
  //
  CFRunLoopRun();
  return 0;
}
