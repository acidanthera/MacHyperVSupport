//
//  HyperVMouse.cpp
//  Hyper-V mouse driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVMouse.hpp"

OSDefineMetaClassAndStructors(HyperVMouse, super);

bool HyperVMouse::handleStart(IOService *provider) {
  if (!super::handleStart(provider)) {
    return false;
  }
  
  //
  // HIDDefaultBehavior needs to be set to Mouse for the device to
  // get exposed as a mouse to userspace.
  //
  DBGLOG("Initializing Hyper-V Synthetic Mouse");
  setProperty("HIDDefaultBehavior", "Mouse");
  
  //
  // Get parent VMBus device object.
  //
  hvDevice = OSDynamicCast(HyperVVMBusDevice, provider);
  if (hvDevice == NULL) {
    return false;
  }
  hvDevice->retain();
  
  //
  // Configure the channel.
  //
  if (!hvDevice->openChannel(kHyperVMouseRingBufferSize, kHyperVMouseRingBufferSize,
                             this, OSMemberFunctionCast(IOInterruptEventAction, this, &HyperVMouse::handleInterrupt))) {
    return false;
  }
  
  if (!setupMouse()) {
    SYSLOG("Failed to set up device");
    return false;
  }
  
  for (int i = 0; i < kHyperVMouseInitTimeout; i++) {
    if (hidDescriptorValid) {
      DBGLOG("Device info packet is now valid");
      break;
    }
    
    IODelay(10);
  }
  
  if (hidDescriptorValid) {
    SYSLOG("Initialized Hyper-V Synthetic Mouse");
  } else {
    SYSLOG("Timed out getting device info");
  }
  
  return hidDescriptorValid;
}

void HyperVMouse::handleStop(IOService *provider) {
  DBGLOG("Hyper-V Mouse is stopping");
  
  if (hidDescriptor != NULL) {
    IOFree(hidDescriptor, hidDescriptorLength);
    hidDescriptor = NULL;
  }
  
  //
  // Close channel and remove interrupt sources.
  //
  if (hvDevice != NULL) {
    hvDevice->closeChannel();
    hvDevice->release();
  }
  
  super::handleStop(provider);
}

OSString* HyperVMouse::newTransportString() const {
  return OSString::withCStringNoCopy("VMBus");
}

OSString* HyperVMouse::newManufacturerString() const {
  return OSString::withCStringNoCopy("Microsoft");
}

OSString* HyperVMouse::newProductString() const {
  return OSString::withCStringNoCopy("Hyper-V Mouse");
}

OSNumber* HyperVMouse::newVendorIDNumber() const {
  return OSNumber::withNumber(mouseInfo.vendor, 16);
}

OSNumber* HyperVMouse::newProductIDNumber() const {
  return OSNumber::withNumber(mouseInfo.product, 16);
}

OSNumber* HyperVMouse::newVersionNumber() const {
  return OSNumber::withNumber(mouseInfo.version, 16);
}

IOReturn HyperVMouse::newReportDescriptor(IOMemoryDescriptor **descriptor) const {
  IOBufferMemoryDescriptor *bufferDesc = IOBufferMemoryDescriptor::withBytes(hidDescriptor, hidDescriptorLength, kIODirectionNone);
  if (bufferDesc == NULL) {
    SYSLOG("Failed to allocate report descriptor buffer descriptor");
    return kIOReturnNoResources;
  }

  *descriptor = bufferDesc;
  return kIOReturnSuccess;
}

