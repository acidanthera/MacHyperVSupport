//
//  HyperVUserClient.cpp
//  Hyper-V userspace client
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVUserClientInternal.hpp"

OSDefineMetaClassAndStructors(HyperVUserClient, super);

bool HyperVUserClient::start(IOService *provider) {
  //
  // Get parent HyperVController object.
  //
  _hvController = OSDynamicCast(HyperVController, provider);
  if (_hvController == nullptr) {
    HVSYSLOG("Provider is not HyperVController");
    return false;
  }
  _hvController->retain();

  HVCheckDebugArgs();
  HVDBGLOG("Initializing Hyper-V user client");

  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    OSSafeReleaseNULL(_hvController);
    return false;
  }

  //
  // Should only be one user client active at a time.
  //
  if (_hvController->isOpen() || !_hvController->open(this)) {
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

  _drivers = OSDictionary::withCapacity(1);
  if (_drivers == nullptr) {
    HVSYSLOG("Failed to create driver dictionary");
    return false;
  }
  registerService();
  HVDBGLOG("Initialized Hyper-V user client");
  return true;
}

void HyperVUserClient::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V user client");

  if (_hvController != nullptr) {
    _hvController->close(this);
    OSSafeReleaseNULL(_hvController);
  }

  super::stop(provider);
}

IOReturn HyperVUserClient::message(UInt32 type, IOService *provider, void *argument) {
  if (OSDynamicCast(HyperVController, provider) == _hvController) {
    HVDBGLOG("Message from HyperVController of type 0x%X received", type);
    switch (type) {
      case kIOMessageServiceIsTerminated:
        _hvController->close(this);
        break;

      default:
        break;
    }
  }

  return super::message(type, provider, argument);
}

bool HyperVUserClient::initWithTask(task_t owningTask, void *securityToken, UInt32 type, OSDictionary *properties) {
    if (!owningTask)
        return false;
    
    if (!super::initWithTask(owningTask, securityToken, type))
        return false;
    
    mTask = owningTask;
    
    return true;
}

IOReturn HyperVUserClient::clientClose() {
  HVDBGLOG("Hyper-V user client is closing");
  terminate();
  return kIOReturnSuccess;
}

IOReturn HyperVUserClient::registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon) {
  if (_hvController == nullptr) {
    return kIOReturnNotReady;
  }

  HVDBGLOG("Registering notification port 0x%p", port);
  _notificationMsg.header.msgh_remote_port = port;
  return kIOReturnSuccess;
}

bool HyperVUserClient::registerDriver(IOService *driver) {
  HVDBGLOG("Registering driver (%s) to receive callbacks", driver->getMetaClass()->getClassName());
  return _drivers->setObject(driver->getMetaClass()->getClassName(), driver);
}

void HyperVUserClient::deregisterDriver(IOService *driver) {
  HVDBGLOG("Deregistering callback-recipient driver (%s)", driver->getMetaClass()->getClassName());
  _drivers->removeObject(driver->getMetaClass()->getClassName());
}


IOReturn HyperVUserClient::notifyClientApplication(HyperVUserClientNotificationType type, void *data, UInt32 dataLength) {
  if (dataLength > sizeof (_notificationMsg.data)) {
    return kIOReturnMessageTooLarge;
  }

  HVDBGLOG("Sending notification type %u with %u bytes of data", type, dataLength);
  _notificationMsg.type = type;
  memcpy(_notificationMsg.data, data, dataLength);
  _notificationMsg.dataLength = dataLength;

  return mach_msg_send_from_kernel(&_notificationMsg.header, _notificationMsg.header.msgh_size);
}

IOReturn HyperVUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *arguments, IOExternalMethodDispatch *dispatch, OSObject *target, void *reference) {
  if (selector >= kNumberOfMethods)
    return kIOReturnUnsupported;
  
  dispatch = (IOExternalMethodDispatch*) &sMethods[selector];
  target = this;
  reference = NULL;
  
  return super::externalMethod(selector, arguments, dispatch, target, reference);
}

// User client dispatch table
const IOExternalMethodDispatch HyperVUserClient::sMethods[kNumberOfMethods] = {
  { // kMethodFileCopyReturnGeneric
    (IOExternalMethodAction) &HyperVUserClient::sMethodFileCopyReturnGeneric, // Method pointer
    1,                                                                 // Num of scalar input values
    0,                                                                 // Size of struct input
    0,                                                                 // Num of scalar output values
    0                                                                  // Size of struct output
  },
  { // kMethodFileCopyGetStartCopyData
    (IOExternalMethodAction) &HyperVUserClient::sMethodFileCopyGetStartCopyData,
    0,
    0,
    0,
    sizeof (HyperVUserClientFileCopyStartCopyData)
  },
  { // kMethodFileCopyGetDoCopyData
    (IOExternalMethodAction) &HyperVUserClient::sMethodFileCopyGetDoCopyData,
    0,
    0,
    0,
    sizeof (HyperVUserClientFileCopyDoCopyData)
  }
};

IOReturn HyperVUserClient::sMethodFileCopyReturnGeneric(HyperVUserClient *target, void *ref, IOExternalMethodArguments *args) {
  IOService *fCopy;
  target->HVDBGLOG("Userspace called sMethodFileCopyReturnGeneric in userclient");
  
  fCopy = OSDynamicCast(IOService, target->_drivers->getObject("HyperVFileCopy"));
  if (!fCopy)
    return kIOReturnNotReady;
  
  fCopy->callPlatformFunction("returnCodeFromUserspace", true, (void *) args->scalarInput, NULL, NULL, NULL);
  
  return kIOReturnSuccess;
}

IOReturn HyperVUserClient::sMethodFileCopyGetStartCopyData(HyperVUserClient *target, void *ref, IOExternalMethodArguments *args) {
  IOService *fCopy;
  target->HVDBGLOG("Userspace called sMethodFileCopyGetStartCopyData in userclient");
  
  fCopy = OSDynamicCast(IOService, target->_drivers->getObject("HyperVFileCopy"));
  if (!fCopy)
    return kIOReturnNotReady;
  
  fCopy->callPlatformFunction("getStartCopyData", true, (void *) args->structureOutput, NULL, NULL, NULL);
  
  return kIOReturnSuccess;
}

IOReturn HyperVUserClient::sMethodFileCopyGetDoCopyData(HyperVUserClient *target, void *ref, IOExternalMethodArguments *args) {
  IOService *fCopy;
  IOMemoryMap *map = nullptr;
  const HyperVUserClientFileCopyDoCopyData *doCopyData;
  size_t doCopyDataSize;
  target->HVDBGLOG("Userspace called sMethodFileCopyGetDoCopyData in userclient");
  
  fCopy = OSDynamicCast(IOService, target->_drivers->getObject("HyperVFileCopy"));
  if (!fCopy)
    return kIOReturnNotReady;
  
  if (args->structureOutputDescriptor != nullptr) {
    map = args->structureOutputDescriptor->createMappingInTask(kernel_task, 0, kIOMapAnywhere);
    
    if (map == nullptr)
      return kIOReturnError;
    
    doCopyData = reinterpret_cast<const HyperVUserClientFileCopyDoCopyData *>(map->getAddress());
    doCopyDataSize = map->getLength();
  } else {
    doCopyData = reinterpret_cast<const HyperVUserClientFileCopyDoCopyData *>(args->structureInput);
    doCopyDataSize = args->structureInputSize;
  }
  
  if (doCopyDataSize < sizeof (HyperVUserClientFileCopyDoCopyData)) {
    OSSafeReleaseNULL(map);
    target->HVSYSLOG("doCopyDataSize smaller than expected");
    return kIOReturnError;
  }
  
  fCopy->callPlatformFunction("getDoCopyData", true, (void *) doCopyData, NULL, NULL, NULL);
  
  OSSafeReleaseNULL(map);
  return kIOReturnSuccess;
}
