//
//  HyperVTimeSyncUserClient.cpp
//  Hyper-V time synchronization user client
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVTimeSyncUserClientInternal.hpp"

OSDefineMetaClassAndStructors(HyperVTimeSyncUserClient, super);

bool HyperVTimeSyncUserClient::start(IOService *provider) {
  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    return false;
  }

  HVCheckDebugArgs();
  setICDebug(debugEnabled);

  HVDBGLOG("Initialized Hyper-V Time Synchronization user client");
  return true;
}

void HyperVTimeSyncUserClient::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V Time Synchronization user client");
  super::stop(provider);
}

IOReturn HyperVTimeSyncUserClient::doTimeSync(UInt64 seconds, UInt32 microseconds) {
  HyperVTimeSyncUserClientNotificationMessage notificationMsg = { };

  if (_notificationPort == MACH_PORT_NULL) {
    HVDBGLOG("Notification port is not open");
    return kIOReturnNotFound;
  }

  HVDBGLOG("Sending time sync notification with %llu seconds and %u microseconds", seconds, microseconds);

  notificationMsg.header.msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
  notificationMsg.header.msgh_size        = sizeof (notificationMsg);
  notificationMsg.header.msgh_remote_port = _notificationPort;
  notificationMsg.header.msgh_local_port  = MACH_PORT_NULL;
  notificationMsg.header.msgh_reserved    = 0;
  notificationMsg.header.msgh_id          = 0;

  notificationMsg.seconds      = seconds;
  notificationMsg.microseconds = microseconds;

  return mach_msg_send_from_kernel(&notificationMsg.header, notificationMsg.header.msgh_size);
}
