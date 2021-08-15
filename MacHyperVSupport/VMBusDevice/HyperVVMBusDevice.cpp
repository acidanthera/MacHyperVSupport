//
//  HyperVVMBusDevice.cpp
//  Hyper-V VMBus device nub
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVVMBusDevice.hpp"
#include "HyperVVMBusDeviceInternal.hpp"

OSDefineMetaClassAndStructors(HyperVVMBusDevice, super);

bool HyperVVMBusDevice::attach(IOService *provider) {
  char channelLocation[10];
  
  if (!super::attach(provider)) {
    return false;
  }
  
  channelIsOpen = false;
  
  //
  // Get channel number.
  //
  OSNumber *channelNumber = OSDynamicCast(OSNumber, getProperty(kHyperVVMBusDeviceChannelIDKey));
  vmbusProvider = OSDynamicCast(HyperVVMBusController, getProvider());
  if (channelNumber == NULL || vmbusProvider == NULL) {
    return false;
  }
  channelId = channelNumber->unsigned32BitValue();
  DBGLOG("Attaching nub for channel %u", channelId);
  
  //
  // Set location to ensure unique names in I/O Registry.
  //
  snprintf(channelLocation, sizeof (channelLocation), "%x", channelId);
  setLocation(channelLocation);
  
  return true;
}

void HyperVVMBusDevice::detach(IOService *provider) {
  //
  // Close and free channel.
  //
  if (channelIsOpen) {
    closeChannel();
  }
  vmbusProvider->freeVMBusChannel(channelId);
  
  super::detach(provider);
}

bool HyperVVMBusDevice::openChannel(UInt32 txSize, UInt32 rxSize, OSObject *owner, IOInterruptEventAction intAction) {
  if (channelIsOpen) {
    return true;
  }
  
  DBGLOG("Opening channel for %u", channelId);
  txBufferSize = txSize;
  rxBufferSize = rxSize;
  
  if (!setupInterrupt()) {
    return false;
  }
  
  if (owner != NULL && intAction != NULL) {
    childInterruptSource = IOInterruptEventSource::interruptEventSource(owner, intAction);
    if (childInterruptSource == NULL) {
      return kIOReturnError;
    }

    workLoop->addEventSource(childInterruptSource);
    childInterruptSource->enable();
  }
  
  //
  // Open channel.
  //
  if (!vmbusProvider->initVMBusChannel(channelId, txBufferSize, &txBuffer, rxBufferSize, &rxBuffer)) {
    teardownInterrupt();
    return false;
  }
  
  if (!vmbusProvider->openVMBusChannel(channelId)) {
    vmbusProvider->closeVMBusChannel(channelId);
    teardownInterrupt();
    return false;
  }
  
  channelIsOpen = true;
  DBGLOG("Opened channel for %u", channelId);
  return true;
}

void HyperVVMBusDevice::closeChannel() {
  //
  // Close channel and stop interrupts.
  //
  vmbusProvider->closeVMBusChannel(channelId);
  teardownInterrupt();
  channelIsOpen = false;
}

bool HyperVVMBusDevice::createGpadlBuffer(UInt32 bufferSize, UInt32 *gpadlHandle, void **buffer) {
  return vmbusProvider->initVMBusChannelGpadl(channelId, bufferSize, gpadlHandle, buffer);
}

IOReturn HyperVVMBusDevice::doRequest(HyperVVMBusDeviceRequest *request) {
  return commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBusDevice::doRequestGated), request);
}
