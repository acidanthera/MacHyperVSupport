//
//  HyperVMouse.cpp
//  Hyper-V mouse driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVMouse.hpp"

#include <Headers/kern_api.hpp>

OSDefineMetaClassAndStructors(HyperVMouse, super);

bool HyperVMouse::handleStart(IOService *provider) {
  if (!super::handleStart(provider)) {
    return false;
  }

  //
  // Get parent VMBus device object.
  //
  hvDevice = OSDynamicCast(HyperVVMBusDevice, provider);
  if (hvDevice == NULL) {
    return false;
  }
  hvDevice->retain();
  
  debugEnabled = checkKernelArgument("-hvmousdbg");
  hvDevice->setDebugMessagePrinting(checkKernelArgument("-hvmousmsgdbg"));
  
  //
  // HIDDefaultBehavior needs to be set to Mouse for the device to
  // get exposed as a mouse to userspace.
  //
  HVDBGLOG("Initializing Hyper-V Synthetic Mouse");
  setProperty("HIDDefaultBehavior", "Mouse");
  
  //
  // Configure interrupt.
  //
  interruptSource =
    IOInterruptEventSource::interruptEventSource(this, OSMemberFunctionCast(IOInterruptEventAction, this, &HyperVMouse::handleInterrupt), provider, 0);
  getWorkLoop()->addEventSource(interruptSource);
  interruptSource->enable();

  //
  // Configure the channel.
  //
  if (!hvDevice->openChannel(kHyperVMouseRingBufferSize, kHyperVMouseRingBufferSize)) {
    return false;
  }

  if (!setupMouse()) {
    HVSYSLOG("Failed to set up device");
    return false;
  }
  
  HVSYSLOG("Initialized Hyper-V Synthetic Mouse");
  return true;
}

void HyperVMouse::handleStop(IOService *provider) {
  HVDBGLOG("Hyper-V Mouse is stopping");

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
    HVSYSLOG("Failed to allocate report descriptor buffer descriptor");
    return kIOReturnNoResources;
  }

  *descriptor = bufferDesc;
  return kIOReturnSuccess;
}
