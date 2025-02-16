//
//  HyperVGraphicsBridge.cpp
//  Hyper-V synthetic graphics bridge
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#include "HyperVGraphicsBridge.hpp"
#include "HyperVPCIRoot.hpp"

#include <IOKit/IOPlatformExpert.h>

OSDefineMetaClassAndStructors(HyperVGraphicsBridge, super);

bool HyperVGraphicsBridge::start(IOService *provider) {
  bool     result = false;
  IOReturn status;

  HVCheckDebugArgs();
  HVDBGLOG("Initializing Hyper-V Synthetic Graphics Bridge");

  if (HVCheckOffArg()) {
    HVSYSLOG("Disabling Hyper-V Synthetic Graphics Bridge due to boot arg");
    return false;
  }

  //
  // Locate root PCI bus instance.
  //
  _hvPCIRoot = HyperVPCIRoot::getPCIRootInstance();
  if (_hvPCIRoot == nullptr) {
    return false;
  }

  //
  // Do not start on Gen1 VMs.
  //
  if (!_hvPCIRoot->isHyperVGen2()) {
    HVDBGLOG("Not starting on Hyper-V Gen1 VM");
    return false;
  }

  //
  // Register with root PCI bridge.
  //
  if (_hvPCIRoot->registerChildPCIBridge(this, &_pciBusNumber) != kIOReturnSuccess) {
    HVSYSLOG("Failed to register with root PCI bus instance");
    return false;
  }

  //
  // Pull console info.
  // TODO: Use actual info from Hyper-V VMBus device for this.
  //
  if (getPlatform()->getConsoleInfo(&_consoleInfo) != kIOReturnSuccess) {
    HVSYSLOG("Failed to get console info");
    return false;
  }
  HVDBGLOG("Console is at 0x%X (%ux%u, bpp: %u, bytes/row: %u)",
         _consoleInfo.v_baseAddr, _consoleInfo.v_height, _consoleInfo.v_width, _consoleInfo.v_depth, _consoleInfo.v_rowBytes);

  _pciLock = IOSimpleLockAlloc();
  fillFakePCIDeviceSpace();

  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
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

  do {


    HVDBGLOG("Initialized Hyper-V Synthetic Graphics Bridge");
    result = true;
  } while (false);

  if (!result) {
    stop(provider);
  }
  return result;
}

void HyperVGraphicsBridge::stop(IOService *provider) {
  HVDBGLOG("Hyper-V Synthetic Graphics Bridge is stopping");

  super::stop(provider);
}

bool HyperVGraphicsBridge::configure(IOService *provider) {
  //
  // Add framebuffer memory range to bridge.
  //
  UInt32 fbSize = (UInt32)(_consoleInfo.v_height * _consoleInfo.v_rowBytes);
  addBridgeMemoryRange(_consoleInfo.v_baseAddr, fbSize, true);
  return super::configure(provider);
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
