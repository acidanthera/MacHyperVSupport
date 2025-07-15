//
//  HyperVSnapshotUserClient.cpp
//  Hyper-V snapshot (VSS) user client
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "HyperVSnapshotUserClientInternal.hpp"

OSDefineMetaClassAndStructors(HyperVSnapshotUserClient, super);

bool HyperVSnapshotUserClient::start(IOService *provider) {
  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    return false;
  }

  HVCheckDebugArgs();
  setICDebug(debugEnabled);

  HVDBGLOG("Initialized Hyper-V Snapshot user client");
  return true;
}

void HyperVSnapshotUserClient::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V Snapshot user client");
  super::stop(provider);
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
IOReturn HyperVSnapshotUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *arguments, IOExternalMethodDispatch *dispatch,
                                                  OSObject *target, void *reference) {
  static const IOExternalMethodDispatch methods[kHyperVSnapshotUserClientMethodNumberOfMethods] = {
    { // kHyperVSnapshotUserClientMethodReportSnapshotAbility
      reinterpret_cast<IOExternalMethodAction>(&HyperVSnapshotUserClient::sDispatchMethodReportSnapshotAbility),  // Method pointer
      2,  // Num of scalar input values
      0,  // Size of struct input
      0,  // Num of scalar output values
      0   // Size of struct output
    },
    { // kHyperVSnapshotUserClientMethodCompleteFreeze
      reinterpret_cast<IOExternalMethodAction>(&HyperVSnapshotUserClient::sDispatchMethodCompleteFreeze),         // Method pointer
      2,  // Num of scalar input values
      0,  // Size of struct input
      0,  // Num of scalar output values
      0   // Size of struct output
    },
    { // kHyperVSnapshotUserClientMethodCompleteThaw
      reinterpret_cast<IOExternalMethodAction>(&HyperVSnapshotUserClient::sDispatchMethodCompleteThaw),           // Method pointer
      2,  // Num of scalar input values
      0,  // Size of struct input
      0,  // Num of scalar output values
      0   // Size of struct output
    }
  };

  if (selector >= kHyperVSnapshotUserClientMethodNumberOfMethods) {
    return kIOReturnUnsupported;
  }
  dispatch = const_cast<IOExternalMethodDispatch*>(&methods[selector]);

  target = this;
  reference = nullptr;

  return super::externalMethod(selector, arguments, dispatch, target, reference);
}
#else
IOExternalMethod* HyperVSnapshotUserClient::getTargetAndMethodForIndex(IOService **target, UInt32 index) {
  static const IOExternalMethod methods[kHyperVSnapshotUserClientMethodNumberOfMethods] = {
    { // kHyperVSnapshotUserClientMethodReportSnapshotAbility
      NULL,                 // Target pointer
#if (defined(__i386__) && defined(__clang__))
      // Required to match GCC behavior on 32-bit when building with clang
      kIOExternalMethodACID32Padding,
      reinterpret_cast<IOMethodACID32>(&HyperVSnapshotUserClient::sMethodReportSnapshotAbility),  // Static method pointer
#else
      reinterpret_cast<IOMethod>(&HyperVSnapshotUserClient::methodReportSnapshotAbility),         // Instance method pointer
#endif
      kIOUCScalarIScalarO,  // Method type
      2,                    // Num of scalar input values or size of struct input
      0                     // Num of scalar output values or size of struct output
    },
    { // kHyperVSnapshotUserClientMethodCompleteFreeze
      NULL,                 // Target pointer
#if (defined(__i386__) && defined(__clang__))
      // Required to match GCC behavior on 32-bit when building with clang
      kIOExternalMethodACID32Padding,
      reinterpret_cast<IOMethodACID32>(&HyperVSnapshotUserClient::sMethodCompleteFreeze),         // Static method pointer
#else
      reinterpret_cast<IOMethod>(&HyperVSnapshotUserClient::methodCompleteFreeze),                // Instance method pointer
#endif
      kIOUCScalarIScalarO,  // Method type
      2,                    // Num of scalar input values or size of struct input
      0                     // Num of scalar output values or size of struct output
    },
    { // kHyperVSnapshotUserClientMethodCompleteThaw
      NULL,                 // Target pointer
#if (defined(__i386__) && defined(__clang__))
      // Required to match GCC behavior on 32-bit when building with clang
      kIOExternalMethodACID32Padding,
      reinterpret_cast<IOMethodACID32>(&HyperVSnapshotUserClient::sMethodCompleteThaw),           // Static method pointer
#else
      reinterpret_cast<IOMethod>(&HyperVSnapshotUserClient::methodCompleteThaw),                  // Instance method pointer
#endif
      kIOUCScalarIScalarO,  // Method type
      2,                    // Num of scalar input values or size of struct input
      0                     // Num of scalar output values or size of struct output
    }
  };

  if (index >= kHyperVSnapshotUserClientMethodNumberOfMethods) {
    return nullptr;
  }

  *target = this;
  return const_cast<IOExternalMethod*>(&methods[index]);
}
#endif

IOReturn HyperVSnapshotUserClient::checkSnapshotAbility() {
  IOReturn status;

  //
  // Check if userspace daemon is running and responsive.
  //
  _isSleeping = true;
  status = notifySnapshotClient(kHyperVSnapshotUserClientNotificationTypeCheck);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to notify snapshot daemon with status 0x%X", status);
    return status;
  }
  status = sleepThread();
  if (status == kIOReturnTimeout) {
    HVSYSLOG("Timed out while waiting for snapshot daemon");
    return status;
  }

  HVDBGLOG("Snapshot supported status: 0x%X", status);
  return status;
}

IOReturn HyperVSnapshotUserClient::doFreeze() {
  IOReturn status;

  //
  // Notify userspace daemon to begin freeze.
  //
  _isSleeping = true;
  status = notifySnapshotClient(kHyperVSnapshotUserClientNotificationTypeFreeze);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to notify snapshot daemon with status 0x%X", status);
    return status;
  }
  status = sleepThread();
  if (status == kIOReturnTimeout) {
    HVSYSLOG("Timed out while waiting for snapshot daemon");
    return status;
  }

  HVDBGLOG("Freeze status: 0x%X", status);
  return status;
}

IOReturn HyperVSnapshotUserClient::doThaw() {
  IOReturn status;

  //
  // Notify userspace daemon to begin thaw.
  //
  _isSleeping = true;
  status = notifySnapshotClient(kHyperVSnapshotUserClientNotificationTypeThaw);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to notify snapshot daemon with status 0x%X", status);
    return status;
  }
  status = sleepThread();
  if (status == kIOReturnTimeout) {
    HVSYSLOG("Timed out while waiting for snapshot daemon");
    return status;
  }

  HVDBGLOG("Thaw status: 0x%X", status);
  return status;
}
