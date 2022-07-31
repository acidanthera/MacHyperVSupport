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

static io_connect_t hvShutdownConnect(void) {
  io_connect_t  connection = 0;
  io_iterator_t iterator = 0;
  kern_return_t result;
  
  //
  // Locate HyperVShutdown service and open connection.
  //
  CFMutableDictionaryRef dict = IOServiceMatching(HVSHUTDOWN_KERNEL_SERVICE);
  if (dict) {
    result = IOServiceGetMatchingServices(kIOMasterPortDefault, dict, &iterator);
    if (result == kIOReturnSuccess && IOIteratorIsValid(iterator)) {
      io_object_t device = IOIteratorNext(iterator);
      if (device) {
        result = IOServiceOpen(device, mach_task_self(), 0, &connection);
        if (result != kIOReturnSuccess)
          HVSYSLOG(stderr, "Failure while opening %s service: 0x%X\n", HVSHUTDOWN_KERNEL_SERVICE, result);
        IOObjectRelease(device);
      } else {
        HVSYSLOG(stderr, "Unable to locate %s service: 0x%X\n", HVSHUTDOWN_KERNEL_SERVICE, result);
      }
      IOObjectRelease(iterator);
    } else {
      HVSYSLOG(stderr, "Unable to locate %s service: 0x%X\n", HVSHUTDOWN_KERNEL_SERVICE, result);
    }
  } else {
    HVSYSLOG(stderr, "Unable to create %s service matching dictionary\n", HVSHUTDOWN_KERNEL_SERVICE);
  }
  
  return connection;
}

static mach_port_t hvShutdownSetupPort(io_connect_t connection) {
  kern_return_t result;
  mach_port_t   port = MACH_PORT_NULL;
  
  //
  // Create port used for shutdown request notifications.
  //
  result = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
  if (result != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failed to allocate notification port: 0x%X", result);
    return MACH_PORT_NULL;
  }
  result = mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
  if (result != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failed to insert notification port: 0x%X", result);
    mach_port_deallocate(mach_task_self(), port);
    return MACH_PORT_NULL;
  }
  HVDBGLOG(stdout, "Port 0x%llX created for shutdown notifications", port);
  
  //
  // Setup notification for shutdown requests.
  //
  result = IOConnectSetNotificationPort(connection, kHyperVShutdownNotificationTypePerformShutdown, port, 0);
  if (result != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failed to set notification port: 0x%X", result);
    mach_port_deallocate(mach_task_self(), port);
    return MACH_PORT_NULL;
  }
  HVDBGLOG(stdout, "Port 0x%llX setup for shutdown notfications", port);
  
  return port;
}

static void hvShutdownTeardown(io_connect_t connection, mach_port_t port) {
  //
  // Cleanup connection.
  //
  IOServiceClose(connection);
  mach_port_deallocate(mach_task_self(), port);
}

static kern_return_t hvShutdownWaitForNotification(mach_port_t port) {
  kern_return_t                     result;
  HyperVShutdownNotificationMessage msg;
  
  //
  // Wait for notification message to come in on port.
  //
  result = mach_msg(&msg.header, MACH_RCV_MSG, 0, sizeof (msg), port, 0, MACH_PORT_NULL);
  if (result != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failure while waiting for notification: 0x%X", result);
  }
  return result;
}

int main(int argc, const char * argv[]) {
  kern_return_t result;
  io_connect_t  hvConnection;
  mach_port_t   shutdownPort;
  
  //
  // Connect to HyperVShutdown.
  //
  hvConnection = hvShutdownConnect();
  if (!hvConnection) {
    return -1;
  }
  HVDBGLOG(stdout, "Connected to %s", HVSHUTDOWN_KERNEL_SERVICE);
  
  //
  // Setup notification and wait for port to be triggered.
  // The port being triggered indicates a shutdown request.
  //
  shutdownPort = hvShutdownSetupPort(hvConnection);
  if (shutdownPort == MACH_PORT_NULL) {
    hvShutdownTeardown(hvConnection, shutdownPort);
    return -1;
  }
  result = hvShutdownWaitForNotification(shutdownPort);
  if (result != kIOReturnSuccess) {
    hvShutdownTeardown(hvConnection, shutdownPort);
    return -1;
  }
  
  HVSYSLOG(stdout, "Shutdown request received, performing shutdown");
  hvShutdownTeardown(hvConnection, shutdownPort);
  
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
  
  return 0;
}
