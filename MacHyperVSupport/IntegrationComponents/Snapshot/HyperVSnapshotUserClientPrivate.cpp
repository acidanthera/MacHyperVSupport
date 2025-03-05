//
//  HyperVSnapshotUserClientPrivate.cpp
//  Hyper-V snapshot (VSS) user client
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "HyperVSnapshotUserClientInternal.hpp"

IOReturn HyperVSnapshotUserClient::notifySnapshotClient(HyperVSnapshotUserClientNotificationType type) {
  HyperVSnapshotUserClientNotificationMessage notificationMsg = { };

  if (_notificationPort == MACH_PORT_NULL) {
    HVDBGLOG("Notification port is not open");
    return kIOReturnNotFound;
  }

  HVDBGLOG("Sending snapshot notification type %u", type);
  notificationMsg.header.msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
  notificationMsg.header.msgh_size        = sizeof (notificationMsg);
  notificationMsg.header.msgh_remote_port = _notificationPort;
  notificationMsg.header.msgh_local_port  = MACH_PORT_NULL;
  notificationMsg.header.msgh_reserved    = 0;
  notificationMsg.header.msgh_id          = 0;
  notificationMsg.type                    = type;

  return mach_msg_send_from_kernel(&notificationMsg.header, notificationMsg.header.msgh_size);
}

IOReturn HyperVSnapshotUserClient::reportSnapshotAbility(UInt32 magic, IOReturn snapshotStatus) {
  IOReturn status = (magic == kHyperVSnapshotMagic) ? snapshotStatus : kIOReturnUnsupported;
  wakeThread(status);
  return status;
}

IOReturn HyperVSnapshotUserClient::completeFreeze(UInt32 magic, IOReturn freezeStatus) {
  IOReturn status = (magic == kHyperVSnapshotMagic) ? freezeStatus : kIOReturnUnsupported;
  wakeThread(status);
  return status;
}

IOReturn HyperVSnapshotUserClient::completeThaw(UInt32 magic, IOReturn thawStatus) {
  IOReturn status = (magic == kHyperVSnapshotMagic) ? thawStatus : kIOReturnUnsupported;
  wakeThread(status);
  return status;
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
IOReturn HyperVSnapshotUserClient::sDispatchMethodReportSnapshotAbility(HyperVSnapshotUserClient *target, void *ref, IOExternalMethodArguments *args) {
  return target->reportSnapshotAbility(static_cast<UInt32>(args->scalarInput[0]), static_cast<IOReturn>(args->scalarInput[1]));
}

IOReturn HyperVSnapshotUserClient::sDispatchMethodCompleteFreeze(HyperVSnapshotUserClient *target, void *ref, IOExternalMethodArguments *args) {
  return target->completeFreeze(static_cast<UInt32>(args->scalarInput[0]), static_cast<IOReturn>(args->scalarInput[1]));
}

IOReturn HyperVSnapshotUserClient::sDispatchMethodCompleteThaw(HyperVSnapshotUserClient *target, void *ref, IOExternalMethodArguments *args) {
  return target->completeThaw(static_cast<UInt32>(args->scalarInput[0]), static_cast<IOReturn>(args->scalarInput[1]));
}
#else
#if (defined(__i386__) && defined(__clang__))
IOReturn HyperVSnapshotUserClient::sMethodReportSnapshotAbility(HyperVSnapshotUserClient* that, UInt32 magic, IOReturn snapshotStatus) {
  return that->reportSnapshotAbility(magic, snapshotStatus);
}

IOReturn HyperVSnapshotUserClient::sMethodCompleteFreeze(HyperVSnapshotUserClient* that, UInt32 magic, IOReturn freezeStatus) {
  return that->completeFreeze(magic, freezeStatus);
}

IOReturn HyperVSnapshotUserClient::sMethodCompleteThaw(HyperVSnapshotUserClient* that, UInt32 magic, IOReturn thawStatus) {
  return that->completeThaw(magic, thawStatus);
}
#else
IOReturn HyperVSnapshotUserClient::methodReportSnapshotAbility(UInt32 magic, IOReturn snapshotStatus) {
  return reportSnapshotAbility(magic, snapshotStatus);
}

IOReturn HyperVSnapshotUserClient::methodCompleteFreeze(UInt32 magic, IOReturn freezeStatus) {
  return completeFreeze(magic, freezeStatus);
}

IOReturn HyperVSnapshotUserClient::methodCompleteThaw(UInt32 magic, IOReturn thawStatus) {
  return completeThaw(magic, thawStatus);
}
#endif
#endif
