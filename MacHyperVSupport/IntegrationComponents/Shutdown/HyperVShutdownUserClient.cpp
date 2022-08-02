//
//  HyperVShutdownUserClient.cpp
//  Hyper-V guest shutdown userspace client
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVShutdownUserClientInternal.hpp"

#include <Headers/kern_api.hpp>

OSDefineMetaClassAndStructors(HyperVShutdownUserClient, super);

bool HyperVShutdownUserClient::start(IOService *provider) {
  hvShutdown = OSDynamicCast(HyperVShutdown, provider);
  if (hvShutdown == nullptr) {
    HVSYSLOG("Provider is not HyperVShutdown, aborting");
    return false;
  }
  hvShutdown->retain();
  
  if (!super::start(provider)) {
    return false;
  }
  
  // Should only be one user client active at a time.
  if (hvShutdown->isOpen() || !hvShutdown->open(this)) {
    super::stop(provider);
    return false;
  }
  
  // Populate notification message info.
  notificationMsg.header.msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
  notificationMsg.header.msgh_size        = sizeof (notificationMsg);
  notificationMsg.header.msgh_remote_port = MACH_PORT_NULL;
  notificationMsg.header.msgh_local_port  = MACH_PORT_NULL;
  notificationMsg.header.msgh_reserved    = 0;
  notificationMsg.header.msgh_id          = 0;
  
  debugEnabled = checkKernelArgument("-hvshutdbg");
  HVDBGLOG("Initialized Hyper-V Guest Shutdown user client");
  return true;
}

void HyperVShutdownUserClient::stop(IOService *provider) {
  releaseNotificationPort(notificationMsg.header.msgh_remote_port);
  hvShutdown->close(this);
  hvShutdown->release();
  super::stop(provider);
  HVDBGLOG("Stopped Hyper-V Guest Shutdown user client");
}

IOReturn HyperVShutdownUserClient::message(UInt32 type, IOService *provider, void *argument) {
  if (OSDynamicCast(HyperVShutdown, provider) == hvShutdown) {
    HVDBGLOG("Message from provider of type 0x%X received", type);
    switch (type) {
      // Indicates from provider we are ready to shutdown.
      case kHyperVShutdownMessageTypeShutdownRequested:
        *(static_cast<bool*>(argument)) = true;
        return kIOReturnSuccess;
        
      // Send notification to userspace client application.
      case kHyperVShutdownMessageTypePerformShutdown:
        return notifyShutdown();
        
      case kIOMessageServiceIsTerminated:
        notifyClosure();
        hvShutdown->close(this);
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
  if (hvShutdown == nullptr) {
    return kIOReturnNotReady;
  }
  releaseNotificationPort(port);
  
  HVDBGLOG("Registering notification port 0x%p", port);
  notificationMsg.header.msgh_remote_port = port;
  return kIOReturnSuccess;
}

IOReturn HyperVShutdownUserClient::notifyShutdown() {
  HVDBGLOG("Sending notification for shutdown");
  notificationMsg.type = kHyperVShutdownNotificationTypePerformShutdown;
  return mach_msg_send_from_kernel(&notificationMsg.header, notificationMsg.header.msgh_size);
}

IOReturn HyperVShutdownUserClient::notifyClosure() {
  HVDBGLOG("Sending notification for service closing");
  notificationMsg.type = kHyperVShutdownNotificationTypeClosed;
  return mach_msg_send_from_kernel(&notificationMsg.header, notificationMsg.header.msgh_size);
}
