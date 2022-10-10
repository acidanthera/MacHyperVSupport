//
//  HyperVICUserClient.cpp
//  Hyper-V IC user client base class
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVICUserClient.hpp"

OSDefineMetaClassAndAbstractStructors(HyperVICUserClient, super);

bool HyperVICUserClient::start(IOService *provider) {
  //
  // Get parent HyperVICService object.
  //
  _hvICProvider = OSDynamicCast(HyperVICService, provider);
  if (_hvICProvider == nullptr) {
    HVSYSLOG("Provider is not HyperVICService");
    return false;
  }
  _hvICProvider->retain();

  HVCheckDebugArgs();
  HVDBGLOG("Initializing Hyper-V user client");

  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    OSSafeReleaseNULL(_hvICProvider);
    return false;
  }

  //
  // Should only be one user client active at a time.
  //
  if (_hvICProvider->isOpen() || !_hvICProvider->open(this)) {
    HVSYSLOG("Unable to open additional user clients, only one at a time is allowed");
    stop(provider);
    return false;
  }

  _sleepLock = IOLockAlloc();
  if (_sleepLock == nullptr) {
    HVSYSLOG("Failed to allocate sleeping lock");
    stop(provider);
    return false;
  }

  //
  // User client needs to be registered for daemons to connect.
  //
  registerService();
  HVDBGLOG("Initialized Hyper-V user client");
  return true;
}

void HyperVICUserClient::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V user client");

  if (_sleepLock != nullptr) {
    IOLockFree(_sleepLock);
  }

  if (_hvICProvider != nullptr) {
    _hvICProvider->close(this);
    OSSafeReleaseNULL(_hvICProvider);
  }

  super::stop(provider);
}

IOReturn HyperVICUserClient::message(UInt32 type, IOService *provider, void *argument) {
  if (OSDynamicCast(HyperVICService, provider) == _hvICProvider) {
    HVDBGLOG("Message from HyperVICService of type 0x%X received", type);
    switch (type) {
      case kIOMessageServiceIsTerminated:
        _hvICProvider->close(this);
        break;

      default:
        break;
    }
  }

  return super::message(type, provider, argument);
}

IOReturn HyperVICUserClient::clientClose() {
  HVDBGLOG("Hyper-V user client is closing");
  terminate();
  return kIOReturnSuccess;
}

bool HyperVICUserClient::initWithTask(task_t owningTask, void *securityToken, UInt32 type, OSDictionary *properties) {
  if (owningTask == nullptr) {
    return false;
  }

  //
  // Clients must be administrator level.
  //
  if (clientHasPrivilege(securityToken, kIOClientPrivilegeAdministrator) != kIOReturnSuccess) {
    return false;
  };

  if (!super::initWithTask(owningTask, securityToken, type))
    return false;

  _task = owningTask;
  return true;
}

IOReturn HyperVICUserClient::registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon) {
  if (_hvICProvider == nullptr) {
    return kIOReturnNotReady;
  }

  HVDBGLOG("Registering notification port 0x%p", port);
  _notificationPort = port;
  return kIOReturnSuccess;
}

bool HyperVICUserClient::sleepThread() {
  int          result = THREAD_AWAKENED;
  AbsoluteTime deadline;

  clock_interval_to_deadline(15, kSecondScale, &deadline);

  IOLockLock(_sleepLock);
  while (_isSleeping) {
    result = IOLockSleepDeadline(_sleepLock, &_isSleeping, deadline, THREAD_UNINT);
  }
  IOLockUnlock(_sleepLock);

  return result != THREAD_TIMED_OUT;
}

void HyperVICUserClient::wakeThread() {
  IOLockLock(_sleepLock);
  _isSleeping = false;
  IOLockUnlock(_sleepLock);
  IOLockWakeup(_sleepLock, &_isSleeping, true);
}
