//
//  HyperVGraphics.cpp
//  Hyper-V basic graphics driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVGraphics.hpp"
#include "HyperVPCIRoot.hpp"

#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>

OSDefineMetaClassAndStructors(HyperVGraphics, super);

void HyperVGraphics::fillFakePCIDeviceSpace() {
  //
  // Fill PCI device config space.
  //
  memset(fakePCIDeviceSpace, 0, sizeof (fakePCIDeviceSpace));
  
  OSWriteLittleInt16(fakePCIDeviceSpace, kIOPCIConfigVendorID, 0x1414);
  OSWriteLittleInt16(fakePCIDeviceSpace, kIOPCIConfigDeviceID, 0x5353);
  OSWriteLittleInt32(fakePCIDeviceSpace, kIOPCIConfigRevisionID, 0x3000000);
  OSWriteLittleInt16(fakePCIDeviceSpace, kIOPCIConfigSubSystemVendorID, 0x1414);
  OSWriteLittleInt16(fakePCIDeviceSpace, kIOPCIConfigSubSystemID, 0x5353);
  
  OSWriteLittleInt32(fakePCIDeviceSpace, kIOPCIConfigBaseAddress0, (UInt32)consoleInfo.v_baseAddr);
}

bool HyperVGraphics::configure(IOService *provider) {
  UInt32 fbSize = (UInt32)(consoleInfo.v_height * consoleInfo.v_rowBytes);
  
  addBridgeMemoryRange(consoleInfo.v_baseAddr, fbSize, true);
  return super::configure(provider);
}

bool HyperVGraphics::start(IOService *provider) {
  //
  // Do not start on Gen1 VMs.
  //
  IORegistryEntry *pciEntry = IORegistryEntry::fromPath("/PCI0@0", gIODTPlane);
  if (pciEntry != NULL) {
    HVDBGLOG("Existing PCI bus found (Gen1 VM), will not start");
    
    pciEntry->release();
    return false;
  }
  
  //
  // Locate root PCI bus instance and register ourselves.
  //
  if (!HyperVPCIRoot::registerChildPCIBridge(this)) {
    HVSYSLOG("Failed to register with root PCI bus instance");
    return false;
  }
  
  //
  // Pull console info.
  // TODO: Use actual info from Hyper-V VMBus device for this.
  //
  if (getPlatform()->getConsoleInfo(&consoleInfo) != kIOReturnSuccess) {
    HVSYSLOG("Failed to get console info");
    return false;
  }
  HVDBGLOG("Console is at 0x%X (%ux%u, bpp: %u, bytes/row: %u)",
         consoleInfo.v_baseAddr, consoleInfo.v_height, consoleInfo.v_width, consoleInfo.v_depth, consoleInfo.v_rowBytes);
  
  pciLock = IOSimpleLockAlloc();
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

UInt32 HyperVGraphics::configRead32(IOPCIAddressSpace space, UInt8 offset) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  UInt32 data;
  IOInterruptState ints;
  
  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return 0xFFFFFFFF;
  }
  
  ints = IOSimpleLockLockDisableInterrupt(pciLock);
  data = OSReadLittleInt32(fakePCIDeviceSpace, offset);
  
  if (offset == kIOPCIConfigurationOffsetBaseAddress0) {
    HVDBGLOG("gonna read %X", data);
  }
  
  IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
  return data;
}

void HyperVGraphics::configWrite32(IOPCIAddressSpace space, UInt8 offset, UInt32 data) {
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
    UInt32 fbSize = (UInt32)(consoleInfo.v_height * consoleInfo.v_rowBytes);
    OSWriteLittleInt32(fakePCIDeviceSpace, offset, (0xFFFFFFFF - fbSize) + 1);
    return;
  }
  
  ints = IOSimpleLockLockDisableInterrupt(pciLock);
  OSWriteLittleInt32(fakePCIDeviceSpace, offset, data);
  
  IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
}

UInt16 HyperVGraphics::configRead16(IOPCIAddressSpace space, UInt8 offset) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  UInt16 data;
  IOInterruptState ints;
  
  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return 0xFFFF;
  }
  
  ints = IOSimpleLockLockDisableInterrupt(pciLock);
  data = OSReadLittleInt16(fakePCIDeviceSpace, offset);
  
  IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
  return data;
}

void HyperVGraphics::configWrite16(IOPCIAddressSpace space, UInt8 offset, UInt16 data) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  IOInterruptState ints;
  
  if (space.es.deviceNum != 0 || space.es.functionNum != 0 || (offset >= kIOPCIConfigurationOffsetBaseAddress0 && offset <= kIOPCIConfigurationOffsetBaseAddress5) || offset == kIOPCIConfigurationOffsetExpansionROMBase) {
    return;
  }
  
  ints = IOSimpleLockLockDisableInterrupt(pciLock);
  OSWriteLittleInt16(fakePCIDeviceSpace, offset, data);
  
  IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
}

UInt8 HyperVGraphics::configRead8(IOPCIAddressSpace space, UInt8 offset) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  UInt8 data;
  IOInterruptState ints;
  
  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return 0xFF;
  }
  
  ints = IOSimpleLockLockDisableInterrupt(pciLock);
  data = fakePCIDeviceSpace[offset];
  
  IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
  return data;
}

void HyperVGraphics::configWrite8(IOPCIAddressSpace space, UInt8 offset, UInt8 data) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  IOInterruptState ints;
  
  if (space.es.deviceNum != 0 || space.es.functionNum != 0 || (offset >= kIOPCIConfigurationOffsetBaseAddress0 && offset <= kIOPCIConfigurationOffsetBaseAddress5) || offset == kIOPCIConfigurationOffsetExpansionROMBase) {
    return;
  }
  
  ints = IOSimpleLockLockDisableInterrupt(pciLock);
  fakePCIDeviceSpace[offset] = data;
  
  IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
}
