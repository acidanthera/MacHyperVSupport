//
//  HyperVShutdownUserClient.cpp
//  Hyper-V guest shutdown userspace client
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVShutdownUserClientInternal.hpp"

OSDefineMetaClassAndStructors(HyperVShutdownUserClient, super);

bool HyperVShutdownUserClient::start(IOService *provider) {
  bool     result = false;
  IOReturn status;

  //
  // Get parent HyperVshutdown object.
  //
  _hvShutdown = OSDynamicCast(HyperVShutdown, provider);
  if (_hvShutdown == nullptr) {
    HVSYSLOG("Provider is not HyperVShutdown");
    return false;
  }
  _hvShutdown->retain();

  HVCheckDebugArgs();
  HVDBGLOG("Initializing Hyper-V Guest Shutdown userclient");

  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    OSSafeReleaseNULL(_hvShutdown);
    return false;
  }

  //
  // Should only be one user client active at a time.
  //
  if (_hvShutdown->isOpen() || !_hvShutdown->open(this)) {
    HVSYSLOG("Unable to open additional user clients, only one at a time is allowed");
    stop(provider);
    return false;
  }
  
  //
  // Populate notification message info.
  //
  _notificationMsg.header.msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
  _notificationMsg.header.msgh_size        = sizeof (_notificationMsg);
  _notificationMsg.header.msgh_remote_port = MACH_PORT_NULL;
  _notificationMsg.header.msgh_local_port  = MACH_PORT_NULL;
  _notificationMsg.header.msgh_reserved    = 0;
  _notificationMsg.header.msgh_id          = 0;

  HVDBGLOG("Initialized Hyper-V Guest Shutdown user client");
  return true;
}

void HyperVShutdownUserClient::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V Guest Shutdown user client");

  if (_hvShutdown != nullptr) {
    _hvShutdown->close(this);
    OSSafeReleaseNULL(_hvShutdown);
  }

  super::stop(provider);
}

IOReturn HyperVShutdownUserClient::message(UInt32 type, IOService *provider, void *argument) {
  if (OSDynamicCast(HyperVShutdown, provider) == _hvShutdown) {
    HVDBGLOG("Message from provider of type 0x%X received", type);
    switch (type) {
      //
      // Indicates to provider we are ready to shutdown.
      //
      case kHyperVShutdownMessageTypeShutdownRequested:
        *(static_cast<bool*>(argument)) = true;
        return kIOReturnSuccess;

      //
      // Send notification to userspace client application.
      //
      case kHyperVShutdownMessageTypePerformShutdown:
        return notifyShutdown();

      case kHyperVShutdownMessageTypePerformRestart:
        return notifyRestart();

      case kIOMessageServiceIsTerminated:
        _hvShutdown->close(this);
        break;

      default:
        break;
    }
  }
  
  return super::message(type, provider, argument);
}

IOReturn HyperVShutdownUserClient::clientClose() {
  HVDBGLOG("Hyper-V Guest Shutdown user client is closing");
  terminate();
  return kIOReturnSuccess;
}

IOReturn HyperVShutdownUserClient::registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon) {
  if (_hvShutdown == nullptr) {
    return kIOReturnNotReady;
  }

  HVDBGLOG("Registering notification port 0x%p", port);
  _notificationMsg.header.msgh_remote_port = port;
  return kIOReturnSuccess;
}

IOReturn HyperVShutdownUserClient::notifyShutdown() {
  HVDBGLOG("Sending notification for shutdown");
  _notificationMsg.type = kHyperVShutdownNotificationTypePerformShutdown;
  return mach_msg_send_from_kernel(&_notificationMsg.header, _notificationMsg.header.msgh_size);
}

IOReturn HyperVShutdownUserClient::notifyRestart() {
  HVDBGLOG("Sending notification for restart");
  _notificationMsg.type = kHyperVShutdownNotificationTypePerformRestart;
  return mach_msg_send_from_kernel(&_notificationMsg.header, _notificationMsg.header.msgh_size);
}
