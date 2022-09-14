//
//  HyperVMouse.cpp
//  Hyper-V mouse driver
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#include "HyperVMouse.hpp"

OSDefineMetaClassAndStructors(HyperVMouse, super);

bool HyperVMouse::handleStart(IOService *provider) {
  bool     result = false;
  IOReturn status;

  //
  // Get parent VMBus device object.
  //
  _hvDevice = OSDynamicCast(HyperVVMBusDevice, provider);
  if (_hvDevice == nullptr) {
    HVSYSLOG("Provider is not HyperVVMBusDevice");
    return false;
  }
  _hvDevice->retain();

  HVCheckDebugArgs();
  HVDBGLOG("Initializing Hyper-V Synthetic Mouse");

  if (HVCheckOffArg()) {
    HVSYSLOG("Disabling Hyper-V Synthetic Mouse due to boot arg");
    return false;
  }

  if (!super::handleStart(provider)) {
    HVSYSLOG("super::handleStart() returned false");
    return false;
  }

  //
  // HIDDefaultBehavior needs to be set to Mouse for the device to
  // get exposed as a mouse to userspace.
  //
  setProperty("HIDDefaultBehavior", "Mouse");

  do {
    //
    // Install packet handler.
    //
    status = _hvDevice->installPacketActions(this, OSMemberFunctionCast(HyperVVMBusDevice::PacketReadyAction, this, &HyperVMouse::handlePacket),
                                             nullptr, kHyperVMouseResponsePacketSize);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to install packet handler with status 0x%X", status);
      break;
    }

    //
    // Open VMBus channel.
    //
    status = _hvDevice->openVMBusChannel(kHyperVMouseRingBufferSize, kHyperVMouseRingBufferSize);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to open VMBus channel with status 0x%X", status);
      break;
    }

    //
    // Configure Hyper-V mouse device.
    //
    if (!setupMouse()) {
      HVSYSLOG("Unable to setup mouse device");
      break;
    }
    HVDBGLOG("Initialized Hyper-V Synthetic Mouse");

    result = true;
  } while (false);

  if (!result) {
    _hvDevice->closeVMBusChannel();
    _hvDevice->uninstallPacketActions();
    OSSafeReleaseNULL(_hvDevice);
  }

  return result;
}

void HyperVMouse::handleStop(IOService *provider) {
  HVDBGLOG("Hyper-V Synthetic Mouse is stopping");

  if (_hvDevice != nullptr) {
    _hvDevice->closeVMBusChannel();
    _hvDevice->uninstallPacketActions();
    OSSafeReleaseNULL(_hvDevice);
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
  return OSNumber::withNumber(_mouseInfo.vendor, 16);
}

OSNumber* HyperVMouse::newProductIDNumber() const {
  return OSNumber::withNumber(_mouseInfo.product, 16);
}

OSNumber* HyperVMouse::newVersionNumber() const {
  return OSNumber::withNumber(_mouseInfo.version, 16);
}

IOReturn HyperVMouse::newReportDescriptor(IOMemoryDescriptor **descriptor) const {
  IOBufferMemoryDescriptor *bufferDesc = IOBufferMemoryDescriptor::withBytes(_hidDescriptor, _hidDescriptorLength, kIODirectionNone);
  if (bufferDesc == nullptr) {
    HVSYSLOG("Failed to allocate report descriptor buffer descriptor");
    return kIOReturnNoResources;
  }

  *descriptor = bufferDesc;
  return kIOReturnSuccess;
}
