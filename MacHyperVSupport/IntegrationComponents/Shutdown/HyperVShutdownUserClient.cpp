//
//  HyperVShutdownUserClient.cpp
//  Hyper-V guest shutdown user client
//
//  Copyright © 2022-2025 Goldfish64. All rights reserved.
//

#include "HyperVShutdownUserClientInternal.hpp"

OSDefineMetaClassAndStructors(HyperVShutdownUserClient, super);

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
  static const IOExternalMethodDispatch methods[kHyperVShutdownUserClientMethodNumberOfMethods] = {
    { // kHyperVShutdownUserClientMethodReportShutdownAbility
      reinterpret_cast<IOExternalMethodAction>(&HyperVShutdownUserClient::sMethodReportShutdownAbility),  // Method pointer
      1,                                                                                                  // Num of scalar input values
      0,                                                                                                  // Size of struct input
      0,                                                                                                  // Num of scalar output values
      0                                                                                                   // Size of struct output
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
      NULL,                                                               // Target pointer
#if (defined(__i386__) && defined(__clang__))
      // Required to match GCC behavior on 32-bit when building with clang
      kIOExternalMethodACID32Padding,
      (IOMethodACID32) &HyperVShutdownUserClient::sReportShutdownAbility, // Static method pointer
#else
      (IOMethod) &HyperVShutdownUserClient::reportShutdownAbility,        // Instance method pointer
#endif
      kIOUCScalarIScalarO,                                                // Method type
      1,                                                                  // Num of scalar input values or size of struct input
      0                                                                   // Num of scalar output values or size of struct output
    }
  };

  if (index >= kHyperVShutdownUserClientMethodNumberOfMethods) {
    return nullptr;
  }

  *target = this;
  return (IOExternalMethod *) &methods[index];
}
#endif

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
