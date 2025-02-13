//
//  hvshutdownd.c
//  Hyper-V guest shutdown userspace daemon
//
//  Copyright © 2022-2025 Goldfish64. All rights reserved.
//

#include "HyperVShutdownUserClient.h"
#include "hvdebug.h"
#include "hviokit.h"

#define HYPERV_SHUTDOWN_KERNEL_SERVICE "HyperVShutdown"
#define SHUTDOWN_BIN_PATH              "/sbin/shutdown"

HVDeclareLogFunctionsUser("hvshutdownd");

void hvShutdownDoShutdownCheck(io_connect_t connection) {
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
  //
  // Call into user client with standard API.
  //
  UInt64 input64 = kHyperVShutdownMagic;
  IOConnectCallScalarMethod(connection, kHyperVShutdownUserClientMethodReportShutdownAbility, &input64, 1, NULL, NULL);
#else
  //
  // Call into user client with legacy API.
  //
  IOConnectMethodScalarIScalarO(connection, kHyperVShutdownUserClientMethodReportShutdownAbility, 1, 0, kHyperVShutdownMagic);
#endif
}

void hvShutdownDoShutdown(bool restart) {
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
  HyperVShutdownUserClientNotificationMessage *shutdownMsg = (HyperVShutdownUserClientNotificationMessage *) msg;

  if (size < __offsetof(HyperVShutdownUserClientNotificationMessage, type)) {
    HVSYSLOG(stderr, "Invalid message size %u received, should be at least %u",
             size, __offsetof(HyperVShutdownUserClientNotificationMessage, type));
    return;
  }

  HVDBGLOG(stdout, "Received notification of type 0x%X", shutdownMsg->type);
  switch (shutdownMsg->type) {
    //
    // Always returns magic value, means daemon is alive and can handle a shutdown request.
    //
    case kHyperVShutdownUserClientNotificationTypeCheck:
      hvShutdownDoShutdownCheck(connection);
      break;

    case kHyperVShutdownUserClientNotificationTypePerformShutdown:
    case kHyperVShutdownUserClientNotificationTypePerformRestart:
      hvShutdownDoShutdown(shutdownMsg->type == kHyperVShutdownUserClientNotificationTypePerformRestart);
      break;

    default:
      HVDBGLOG(stdout, "Unknown notification type 0x%X", shutdownMsg->type);
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
