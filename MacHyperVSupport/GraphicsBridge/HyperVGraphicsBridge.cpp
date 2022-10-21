//
//  HyperVGraphicsBridge.cpp
//  Hyper-V synthetic graphics bridge
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#include "HyperVGraphicsBridge.hpp"
#include "HyperVPCIRoot.hpp"

#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>

OSDefineMetaClassAndStructors(HyperVGraphicsBridge, super);

void HyperVGraphicsBridge::fillFakePCIDeviceSpace() {
  //
  // Fill PCI device config space.
  //
  // PCI bridge will contain a single PCI graphics device
  // with the framebuffer memory at BAR0. The vendor/device ID is
  // the same as what a generation 1 Hyper-V VM uses for the
  // emulated graphics.
  //
  bzero(_fakePCIDeviceSpace, sizeof (_fakePCIDeviceSpace));

  OSWriteLittleInt16(_fakePCIDeviceSpace, kIOPCIConfigVendorID, kHyperVPCIVendorMicrosoft);
  OSWriteLittleInt16(_fakePCIDeviceSpace, kIOPCIConfigDeviceID, kHyperVPCIDeviceHyperVVideo);
  OSWriteLittleInt32(_fakePCIDeviceSpace, kIOPCIConfigRevisionID, 0x3000000);
  OSWriteLittleInt16(_fakePCIDeviceSpace, kIOPCIConfigSubSystemVendorID, kHyperVPCIVendorMicrosoft);
  OSWriteLittleInt16(_fakePCIDeviceSpace, kIOPCIConfigSubSystemID, kHyperVPCIDeviceHyperVVideo);

  OSWriteLittleInt32(_fakePCIDeviceSpace, kIOPCIConfigBaseAddress0, (UInt32)_consoleInfo.v_baseAddr);
}

bool HyperVGraphicsBridge::configure(IOService *provider) {
  //
  // Add framebuffer memory range to bridge.
  //
  UInt32 fbSize = (UInt32)(_consoleInfo.v_height * _consoleInfo.v_rowBytes);
  addBridgeMemoryRange(_consoleInfo.v_baseAddr, fbSize, true);
  return super::configure(provider);
}

bool HyperVGraphicsBridge::start(IOService *provider) {
  if (HVCheckOffArg()) {
    return false;
  }
  
  //
  // Get parent VMBus device object.
  //
  _hvDevice = OSDynamicCast(HyperVVMBusDevice, provider);
  if (_hvDevice == NULL) {
    return false;
  }
  _hvDevice->retain();
  HVCheckDebugArgs();
  
  //
  // Do not start on Gen1 VMs.
  //
  IORegistryEntry *pciEntry = IORegistryEntry::fromPath("/PCI0@0", gIODTPlane);
  if (pciEntry != NULL) {
    HVDBGLOG("Existing PCI bus found (Gen1 VM), will not start");
    
    pciEntry->release();
    _hvDevice->release();
    return false;
  }
  
  //
  // Locate root PCI bus instance and register ourselves.
  //
  if (!HyperVPCIRoot::registerChildPCIBridge(this)) {
    HVSYSLOG("Failed to register with root PCI bus instance");
    _hvDevice->release();
    return false;
  }
  
  //
  // Pull console info.
  // TODO: Use actual info from Hyper-V VMBus device for this.
  //
  if (getPlatform()->getConsoleInfo(&_consoleInfo) != kIOReturnSuccess) {
    HVSYSLOG("Failed to get console info");
    _hvDevice->release();
    return false;
  }
  HVDBGLOG("Console is at 0x%X (%ux%u, bpp: %u, bytes/row: %u)",
         _consoleInfo.v_baseAddr, _consoleInfo.v_height, _consoleInfo.v_width, _consoleInfo.v_depth, _consoleInfo.v_rowBytes);
  
  _pciLock = IOSimpleLockAlloc();
  fillFakePCIDeviceSpace();
  
  HVDBGLOG("PCI bridge started");
  
  if (!super::start(provider)) {
    return false;
  }

  //
  // Add a friendly name to the child device produced.
  //
  OSIterator *childIterator = getChildIterator(gIOServicePlane);
  if (childIterator != NULL) {
    childIterator->reset();
    
    IOService *childService = OSDynamicCast(IOService, childIterator->getNextObject());
    if (childService != NULL) {
      HVDBGLOG("Found child %s", childService->getName());
      childService->setProperty("model", "Hyper-V Graphics");
    }

    childIterator->release();
  }

  HVSYSLOG("Hyper-V Synthetic Video initialized");
  return true;
}

UInt32 HyperVGraphicsBridge::configRead32(IOPCIAddressSpace space, UInt8 offset) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  UInt32 data;
  IOInterruptState ints;
  
  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return 0xFFFFFFFF;
  }
  
  ints = IOSimpleLockLockDisableInterrupt(_pciLock);
  data = OSReadLittleInt32(_fakePCIDeviceSpace, offset);
  
  if (offset == kIOPCIConfigurationOffsetBaseAddress0) {
    HVDBGLOG("gonna read %X", data);
  }
  
  IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);
  return data;
}

void HyperVGraphicsBridge::configWrite32(IOPCIAddressSpace space, UInt8 offset, UInt32 data) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  IOInterruptState ints;
  
  if (space.es.deviceNum != 0 || space.es.functionNum != 0 || (offset > kIOPCIConfigurationOffsetBaseAddress0 && offset <= kIOPCIConfigurationOffsetBaseAddress5) || offset == kIOPCIConfigurationOffsetExpansionROMBase) {
    HVDBGLOG("ignoring offset %X", offset);
    return;
  }
  
  if (offset == kIOPCIConfigurationOffsetBaseAddress0) {
    HVDBGLOG("gonna write %X", data);
  }
  
  if (offset == kIOPCIConfigurationOffsetBaseAddress0 && data == 0xFFFFFFFF) {
    HVDBGLOG("Got bar size request");
    UInt32 fbSize = (UInt32)(_consoleInfo.v_height * _consoleInfo.v_rowBytes);
    OSWriteLittleInt32(_fakePCIDeviceSpace, offset, (0xFFFFFFFF - fbSize) + 1);
    return;
  }
  
  ints = IOSimpleLockLockDisableInterrupt(_pciLock);
  OSWriteLittleInt32(_fakePCIDeviceSpace, offset, data);
  
  IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);
}

UInt16 HyperVGraphicsBridge::configRead16(IOPCIAddressSpace space, UInt8 offset) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  UInt16 data;
  IOInterruptState ints;
  
  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return 0xFFFF;
  }
  
  ints = IOSimpleLockLockDisableInterrupt(_pciLock);
  data = OSReadLittleInt16(_fakePCIDeviceSpace, offset);
  
  IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);
  return data;
}

void HyperVGraphicsBridge::configWrite16(IOPCIAddressSpace space, UInt8 offset, UInt16 data) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  IOInterruptState ints;
  
  if (space.es.deviceNum != 0 || space.es.functionNum != 0 || (offset >= kIOPCIConfigurationOffsetBaseAddress0 && offset <= kIOPCIConfigurationOffsetBaseAddress5) || offset == kIOPCIConfigurationOffsetExpansionROMBase) {
    return;
  }
  
  ints = IOSimpleLockLockDisableInterrupt(_pciLock);
  OSWriteLittleInt16(_fakePCIDeviceSpace, offset, data);
  
  IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);
}

UInt8 HyperVGraphicsBridge::configRead8(IOPCIAddressSpace space, UInt8 offset) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  UInt8 data;
  IOInterruptState ints;
  
  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return 0xFF;
  }
  
  ints = IOSimpleLockLockDisableInterrupt(_pciLock);
  data = _fakePCIDeviceSpace[offset];
  
  IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);
  return data;
}

void HyperVGraphicsBridge::configWrite8(IOPCIAddressSpace space, UInt8 offset, UInt8 data) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  IOInterruptState ints;
  
  if (space.es.deviceNum != 0 || space.es.functionNum != 0 || (offset >= kIOPCIConfigurationOffsetBaseAddress0 && offset <= kIOPCIConfigurationOffsetBaseAddress5) || offset == kIOPCIConfigurationOffsetExpansionROMBase) {
    return;
  }
  
  ints = IOSimpleLockLockDisableInterrupt(_pciLock);
  _fakePCIDeviceSpace[offset] = data;
  
  IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);
}
