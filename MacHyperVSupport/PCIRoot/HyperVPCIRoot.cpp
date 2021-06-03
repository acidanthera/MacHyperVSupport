//
//  HyperVPCIRoot.cpp
//  Hyper-V PCI root bridge driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVPCIRoot.hpp"
#include <architecture/i386/pio.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>

OSDefineMetaClassAndStructors(HyperVPCIRoot, super);

typedef struct __attribute__((packed)) {
  UInt64 type;
  UInt64 reserved1;
  UInt64 reserved2;
  UInt64 min;
  UInt64 max;
  UInt64 reserved3;
  UInt64 length;
  UInt64 reserved4;
  UInt64 reserved5;
  UInt64 reserved6;
} AppleACPIRange;

inline bool HyperVPCIRoot::setConfigSpace(IOPCIAddressSpace space, UInt8 offset) {
  offset &= 0xFC;
  
  UInt32 pciCycle = (1 << 31) | space.bits | offset;
  
  outl(0xCF8, pciCycle);
  return true;
}

bool HyperVPCIRoot::start(IOService *provider) {
  pciLock = IOSimpleLockAlloc();
  
  if (!super::start(provider)) {
    SYSLOG("Dummy PCI bridge failed to initialize");
    return false;
  }
  
  
  
  DBGLOG("Dummy PCI bridge initialized");
  return true;
}

bool HyperVPCIRoot::configure(IOService *provider) {
  //
  // Add memory ranges.
  //
  //UInt32 i = provider->getDeviceMemoryCount();
  //DBGLOG("%u memory count", i);
  for (int i = 0; i < provider->getDeviceMemoryCount(); i++) {
    IODeviceMemory *devMem = provider->getDeviceMemoryWithIndex(i);
    DBGLOG("devmem %u: 0x%X size %X", i, devMem->getPhysicalAddress(), devMem->getLength());
  }
  
  OSData *acpiAddressSpaces = OSDynamicCast(OSData, provider->getProperty("acpi-address-spaces"));
  
  AppleACPIRange *acpiRanges = (AppleACPIRange*) acpiAddressSpaces->getBytesNoCopy();
  UInt32 acpiRangeCount = acpiAddressSpaces->getLength() / sizeof (AppleACPIRange);
  
  /*UInt8 *dat = (UInt8*) acpiAddressSpaces->getBytesNoCopy();
  for (int i = 0; i < acpiAddressSpaces->getLength(); i++) {
    IOLog(" %X", dat[i]);
  }*/
  
  for (int i = 0; i < acpiRangeCount; i++) {
    DBGLOG("type %u, min %llX, max %llX, len %llX", acpiRanges[i].type, acpiRanges[i].min, acpiRanges[i].max, acpiRanges[i].length);
    if (acpiRanges[i].type == 1) {
      addBridgeIORange(acpiRanges[i].min, acpiRanges[i].length);
    } else if (acpiRanges[i].type == 0) {
      addBridgeMemoryRange(acpiRanges[i].min, acpiRanges[i].length, true);
    }
  }
  
  //IOACPIPlatformDevice *dev = OSDynamicCast(IOACPIPlatformDevice, provider);
 // dev->ioWrite8(<#UInt16 offset#>, <#UInt8 value#>)

  
  return super::configure(provider);
}

UInt32 HyperVPCIRoot::configRead32(IOPCIAddressSpace space, UInt8 offset) {
  DBGLOG("Bus: %u, device: %u, function: %u", space.es.busNum, space.es.deviceNum, space.es.functionNum);
  
  UInt32 data;
  IOInterruptState ints;
  
  ints = IOSimpleLockLockDisableInterrupt(pciLock);
  if (setConfigSpace(space, offset)) {
    data = inl(0xCFC);
  }
  
  IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
  return data;
}

void HyperVPCIRoot::configWrite32(IOPCIAddressSpace space, UInt8 offset, UInt32 data) {
  DBGLOG("Bus: %u, device: %u, function: %u", space.es.busNum, space.es.deviceNum, space.es.functionNum);
  
  IOInterruptState ints;
  
  ints = IOSimpleLockLockDisableInterrupt(pciLock);
  if (setConfigSpace(space, offset)) {
    outl(0xCFC, data);
  }
  
  IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
}

UInt16 HyperVPCIRoot::configRead16(IOPCIAddressSpace space, UInt8 offset) {
  DBGLOG("Bus: %u, device: %u, function: %u", space.es.busNum, space.es.deviceNum, space.es.functionNum);
  
  UInt16 data;
  IOInterruptState ints;
  
  ints = IOSimpleLockLockDisableInterrupt(pciLock);
  if (setConfigSpace(space, offset)) {
    data = inw(0xCFC);
  }
  
  IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
  return data;
}

void HyperVPCIRoot::configWrite16(IOPCIAddressSpace space, UInt8 offset, UInt16 data) {
  DBGLOG("Bus: %u, device: %u, function: %u", space.es.busNum, space.es.deviceNum, space.es.functionNum);
  
  IOInterruptState ints;
  
  ints = IOSimpleLockLockDisableInterrupt(pciLock);
  if (setConfigSpace(space, offset)) {
    outw(0xCFC, data);
  }
  
  IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
}

UInt8 HyperVPCIRoot::configRead8(IOPCIAddressSpace space, UInt8 offset) {
  DBGLOG("Bus: %u, device: %u, function: %u", space.es.busNum, space.es.deviceNum, space.es.functionNum);
  
  UInt8 data;
  IOInterruptState ints;
  
  ints = IOSimpleLockLockDisableInterrupt(pciLock);
  if (setConfigSpace(space, offset)) {
    data = inb(0xCFC);
  }
  
  IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
  return data;
}

void HyperVPCIRoot::configWrite8(IOPCIAddressSpace space, UInt8 offset, UInt8 data) {
  DBGLOG("Bus: %u, device: %u, function: %u", space.es.busNum, space.es.deviceNum, space.es.functionNum);
  
  IOInterruptState ints;
  
  ints = IOSimpleLockLockDisableInterrupt(pciLock);
  if (setConfigSpace(space, offset)) {
    outb(0xCFC, data);
  }
  
  IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
}
