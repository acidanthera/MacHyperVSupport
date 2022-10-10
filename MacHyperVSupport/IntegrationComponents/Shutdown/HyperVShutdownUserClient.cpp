//
//  HyperVShutdownUserClient.cpp
//  Hyper-V guest shutdown user client
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVShutdownUserClientInternal.hpp"

OSDefineMetaClassAndStructors(HyperVShutdownUserClient, super);

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
const IOExternalMethodDispatch HyperVShutdownUserClient::sShutdownMethods[kHyperVShutdownUserClientMethodNumberOfMethods] = {
  { // kHyperVShutdownUserClientMethodReportShutdownAbility
    reinterpret_cast<IOExternalMethodAction>(&HyperVShutdownUserClient::methodReportShutdownAbility),  // Method pointer
    1,                                        // Num of scalar input values
    0,                                        // Num of struct input values
    0,                                        // Num of scalar output values
    0                                         // Num of struct output values
  }
};

/*IOExternalMethod* HyperVShutdownUserClient::getTargetAndMethodForIndex(IOService **targetP, UInt32 index) {
  static IOExternalMethod sMethods[kHyperVShutdownUserClientMethodNumberOfMethods] = {
    { // kHyperVShutdownUserClientMethodReportShutdownAbility, 0
      0, (IOMethod)&HyperVShutdownUserClient::reportShutdownAbility, kIOUCScalarIScalarO, 1, 0
    },
  };

  if (index >= kHyperVShutdownUserClientMethodNumberOfMethods) {
    return nullptr;
  } else {
    *targetP = this;
    return &sMethods[index];
  }
}*/

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

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
IOReturn HyperVShutdownUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *arguments, IOExternalMethodDispatch *dispatch,
                                                  OSObject *target, void *reference) {
  if (selector >= kHyperVShutdownUserClientMethodNumberOfMethods) {
    return kIOReturnUnsupported;
  }
  dispatch = const_cast<IOExternalMethodDispatch*>(&sShutdownMethods[selector]);

  target = this;
  reference = nullptr;

  return super::externalMethod(selector, arguments, dispatch, target, reference);
}
#endif

IOReturn HyperVShutdownUserClient::notifyClientApplication(HyperVShutdownUserClientNotificationType type) {
  HyperVShutdownUserClientNotificationMessage notificationMsg = { };

  HVDBGLOG("Sending shutdown notification type %u", type);

  notificationMsg.header.msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
  notificationMsg.header.msgh_size        = sizeof (notificationMsg);
  notificationMsg.header.msgh_remote_port = _notificationPort;
  notificationMsg.header.msgh_local_port  = MACH_PORT_NULL;
  notificationMsg.header.msgh_reserved    = 0;
  notificationMsg.header.msgh_id          = 0;

  notificationMsg.type = type;

  return mach_msg_send_from_kernel(&notificationMsg.header, notificationMsg.header.msgh_size);
}

IOReturn HyperVShutdownUserClient::methodReportShutdownAbility(HyperVShutdownUserClient *target, void *ref, IOExternalMethodArguments *args) {
  target->_isShutdownSupported = static_cast<bool>(args->scalarInput[0]);
  target->wakeThread();
  return kIOReturnSuccess;
}

bool HyperVShutdownUserClient::canShutdown() {
  IOReturn status;

  _isShutdownSupported = false;
  _isSleeping = true;
  status = notifyClientApplication(kHyperVShutdownUserClientNotificationTypeCheck);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to notify shutdown daemon with status 0x%X", status);
    return false;
  }
  if (!sleepThread()) {
    HVSYSLOG("Timed out while waiting for shutdown daemon");
    return false;
  }

  HVDBGLOG("Shutdown supported: %u", _isShutdownSupported);
  return _isShutdownSupported;
}

void HyperVShutdownUserClient::doShutdown(bool restart) {
  IOReturn status = notifyClientApplication(restart ?
                                            kHyperVShutdownUserClientNotificationTypePerformRestart :
                                            kHyperVShutdownUserClientNotificationTypePerformShutdown);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to notify shutdown daemon with status 0x%X", status);
  }
}
