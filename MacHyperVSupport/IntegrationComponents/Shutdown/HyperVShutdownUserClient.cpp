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
  
  if (!super::start(provider)) {
    return false;
  }
  
  debugEnabled = true;// checkKernelArgument("-hvshutucdbg");
  HVDBGLOG("Initialized Hyper-V Guest Shutdown user client");
  
  return true;
}

IOReturn HyperVShutdownUserClient::message(UInt32 type, IOService *provider, void *argument) {
  if (OSDynamicCast(HyperVShutdown, provider) == hvShutdown) {
    HVDBGLOG("Message from provider of type 0x%X received", type);
    switch (type) {
      // Indicates to provider we are ready to shutdown.
      case kHyperVShutdownMessageTypeShutdownRequested:
        *(static_cast<bool*>(argument)) = true;
        return kIOReturnSuccess;
        
      // Send notification to userspace client application.
      case kHyperVShutdownMessageTypePerformShutdown:
        return performShutdown();
        
      default:
        break;
    }
  }
  
  return super::message(type, provider, argument);
}

IOReturn HyperVShutdownUserClient::clientClose() {
  detach(hvShutdown);
  HVDBGLOG("Hyper-V Guest Shutdown user client is closed");
  return kIOReturnSuccess;
}

IOReturn HyperVShutdownUserClient::registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon) {
  if (type != kHyperVShutdownNotificationTypePerformShutdown) {
    return kIOReturnBadArgument;
  }
  if (hvShutdown == nullptr) {
    return kIOReturnNotReady;
  }
  if (hvShutdown->isOpen()) {
    return kIOReturnExclusiveAccess;
  }
  if (isPortRegistered) {
    return kIOReturnPortExists;
  }
  
  // Should only be one userclient active at a time.
  if (!hvShutdown->open(this)) {
    return kIOReturnNotReady;
  }
  
  HVDBGLOG("Registering notification port 0x%p type 0x%X", port, type);

  isPortRegistered                        = true;
  notificationMsg.header.msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
  notificationMsg.header.msgh_size        = sizeof (notificationMsg);
  notificationMsg.header.msgh_remote_port = port;
  notificationMsg.header.msgh_local_port  = MACH_PORT_NULL;
  notificationMsg.header.msgh_reserved    = 0;
  notificationMsg.header.msgh_id          = 0;
  notificationMsg.type                    = kHyperVShutdownNotificationTypePerformShutdown;
  
  return kIOReturnSuccess;
}

IOReturn HyperVShutdownUserClient::performShutdown() {
  if (!isPortRegistered) {
    return kIOReturnNotReady;
  }
  
  HVDBGLOG("Sending notification for shutdown");
  return mach_msg_send_from_kernel(&notificationMsg.header, notificationMsg.header.msgh_size);
}
