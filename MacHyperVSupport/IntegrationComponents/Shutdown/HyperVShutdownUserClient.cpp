//
//  HyperVShutdownUserClient.cpp
//  Hyper-V guest shutdown user client
//
//  Copyright © 2022-2025 Goldfish64. All rights reserved.
//

#include "HyperVShutdownUserClientInternal.hpp"

OSDefineMetaClassAndStructors(HyperVShutdownUserClient, super);

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
IOReturn HyperVShutdownUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *arguments, IOExternalMethodDispatch *dispatch,
                                                  OSObject *target, void *reference) {
  static const IOExternalMethodDispatch methods[kHyperVShutdownUserClientMethodNumberOfMethods] = {
    { // kHyperVShutdownUserClientMethodReportShutdownAbility
      reinterpret_cast<IOExternalMethodAction>(&HyperVShutdownUserClient::sMethodReportShutdownAbility),  // Method pointer
      1,                                        // Num of scalar input values
      0,                                        // Size of struct input
      0,                                        // Num of scalar output values
      0                                         // Size of struct output
    }
  };
  
  if (selector >= kHyperVShutdownUserClientMethodNumberOfMethods) {
    return kIOReturnUnsupported;
  }
  dispatch = const_cast<IOExternalMethodDispatch*>(&methods[selector]);

  target = this;
  reference = nullptr;

  return super::externalMethod(selector, arguments, dispatch, target, reference);
}

#else
IOExternalMethod* HyperVShutdownUserClient::getTargetAndMethodForIndex(IOService **target, UInt32 index) {
  static const IOExternalMethod methods[kHyperVShutdownUserClientMethodNumberOfMethods] = {
    { // kHyperVShutdownUserClientMethodReportShutdownAbility
      NULL,
#if (defined(__i386__) && defined(__clang__))
      // Required to match GCC behavior on 32-bit when building with clang
      kIOExternalMethodACID32Padding,
      (IOMethodACID32) &HyperVShutdownUserClient::sReportShutdownAbility,
#else
      (IOMethod) &HyperVShutdownUserClient::reportShutdownAbility,
#endif
      kIOUCScalarIScalarO,
      1,
      0
    }
  };

  if (index >= kHyperVShutdownUserClientMethodNumberOfMethods) {
    return nullptr;
  }

  *target = this;
  return (IOExternalMethod *) &methods[index];
}
#endif

bool HyperVShutdownUserClient::start(IOService *provider) {
  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    return false;
  }

  HVCheckDebugArgs();
  setICDebug(debugEnabled);

  HVDBGLOG("Initialized Hyper-V Guest Shutdown user client");
  return true;
}

void HyperVShutdownUserClient::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V Guest Shutdown user client");
  super::stop(provider);
}

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

IOReturn HyperVShutdownUserClient::reportShutdownAbility(UInt32 arg) {
  wakeThread((arg == kHyperVShutdownMagic) ? kIOReturnSuccess : kIOReturnUnsupported);
  return kIOReturnSuccess;
}

bool HyperVShutdownUserClient::canShutdown() {
  IOReturn status;

  //
  // Check if userspace daemon is running and responsive.
  //
  _isSleeping = true;
  status = notifyShutdownClient(kHyperVShutdownUserClientNotificationTypeCheck);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to notify shutdown daemon with status 0x%X", status);
    return false;
  }
  status = sleepThread();
  if (status == kIOReturnTimeout) {
    HVSYSLOG("Timed out while waiting for shutdown daemon");
    return false;
  }

  HVDBGLOG("Shutdown supported: %u", status == kIOReturnSuccess);
  return status == kIOReturnSuccess;
}

void HyperVShutdownUserClient::doShutdown(bool restart) {
  IOReturn status = notifyShutdownClient(restart ?
                                         kHyperVShutdownUserClientNotificationTypePerformRestart :
                                         kHyperVShutdownUserClientNotificationTypePerformShutdown);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to notify shutdown daemon with status 0x%X", status);
  }
}
