//
//  HyperVMouse.cpp
//  Hyper-V mouse driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVMouse.hpp"

OSDefineMetaClassAndStructors(HyperVMouse, super);

bool HyperVMouse::handleStart(IOService *provider) {
  bool      result  = false;
  IOReturn  status;
  
  //
  // Get parent VMBus device object.
  //
  hvDevice = OSDynamicCast(HyperVVMBusDevice, provider);
  if (hvDevice == nullptr) {
    HVSYSLOG("Unable to get parent VMBus device nub");
    return false;
  }
  hvDevice->retain();
  HVCheckDebugArgs();
  
  do {
    HVDBGLOG("Initializing Hyper-V Synthetic Mouse");
    
    if (HVCheckOffArg()) {
      HVSYSLOG("Disabling Hyper-V Synthetic Mouse due to boot arg");
      break;
    }
    
    if (!super::handleStart(provider)) {
      HVSYSLOG("Superclass start function failed");
      break;
    }
    
    //
    // HIDDefaultBehavior needs to be set to Mouse for the device to
    // get exposed as a mouse to userspace.
    //
    setProperty("HIDDefaultBehavior", "Mouse");
    
    //
    // Configure interrupt.
    //
    interruptSource =
      IOInterruptEventSource::interruptEventSource(this, OSMemberFunctionCast(IOInterruptEventAction, this, &HyperVMouse::handleInterrupt), provider, 0);
    if (interruptSource == nullptr) {
      HVSYSLOG("Unable to initialize interrupt");
      break;
    }
    
    status = getWorkLoop()->addEventSource(interruptSource);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Unable to add interrupt event source: 0x%X", status);
      break;
    }
    interruptSource->enable();
    
    //
    // Configure the channel.
    //
    if (hvDevice->openVMBusChannel(kHyperVMouseRingBufferSize, kHyperVMouseRingBufferSize) != kIOReturnSuccess) {
      HVSYSLOG("Unable to configure VMBus channel");
      break;
    }
    
    if (!setupMouse()) {
      HVSYSLOG("Unable to setup mouse device");
      return false;
    }
    
    result = true;
    HVDBGLOG("Initialized Hyper-V Synthetic Mouse");
  } while (false);

  if (!result) {
    freeStructures();
  }
  
  return result;
}

void HyperVMouse::handleStop(IOService *provider) {
  HVDBGLOG("Hyper-V Synthetic Mouse is stopping");

  freeStructures();
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
    HVSYSLOG("Failed to allocate report descriptor buffer descriptor");
    return kIOReturnNoResources;
  }

  *descriptor = bufferDesc;
  return kIOReturnSuccess;
}
