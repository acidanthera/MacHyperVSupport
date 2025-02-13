//
//  HyperVShutdownUserClientPrivate.cpp
//  Hyper-V guest shutdown user client
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "HyperVShutdownUserClientInternal.hpp"

IOReturn HyperVShutdownUserClient::notifyShutdownClient(HyperVShutdownUserClientNotificationType type) {
  HyperVShutdownUserClientNotificationMessage notificationMsg = { };

  if (_notificationPort == MACH_PORT_NULL) {
    HVDBGLOG("Notification port is not open");
    return kIOReturnNotFound;
  }

  HVDBGLOG("Sending shutdown notification type %u", type);
  notificationMsg.header.msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
  notificationMsg.header.msgh_size        = sizeof (notificationMsg);
  notificationMsg.header.msgh_remote_port = _notificationPort;
  notificationMsg.header.msgh_local_port  = MACH_PORT_NULL;
  notificationMsg.header.msgh_reserved    = 0;
  notificationMsg.header.msgh_id          = 0;
  notificationMsg.type                    = type;

  return mach_msg_send_from_kernel(&notificationMsg.header, notificationMsg.header.msgh_size);
}

IOReturn HyperVShutdownUserClient::reportShutdownAbility(UInt32 arg) {
  wakeThread((arg == kHyperVShutdownMagic) ? kIOReturnSuccess : kIOReturnUnsupported);
  return kIOReturnSuccess;
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
IOReturn HyperVShutdownUserClient::sMethodReportShutdownAbility(HyperVShutdownUserClient *target, void *ref, IOExternalMethodArguments *args) {
  return target->reportShutdownAbility((UInt32)args->scalarInput[0]);
}
#else
#if (defined(__i386__) && defined(__clang__))
IOReturn HyperVShutdownUserClient::sReportShutdownAbility(HyperVShutdownUserClient* that, UInt32 arg) {
  that->wakeThread((arg == kHyperVShutdownMagic) ? kIOReturnSuccess : kIOReturnUnsupported);
  return that->reportShutdownAbility(arg);
}
#endif
#endif
