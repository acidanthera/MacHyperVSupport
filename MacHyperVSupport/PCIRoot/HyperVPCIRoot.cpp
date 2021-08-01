//
//  HyperVPCIRoot.cpp
//  Hyper-V PCI root bridge driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVPCIRoot.hpp"
#include <architecture/i386/pio.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>

#define super IOPCIBridge

#define SYSLOG(str, ...) SYSLOG_PRINT("HyperVPCIRoot", str, ## __VA_ARGS__)
#define DBGLOG(str, ...) DBGLOG_PRINT("HyperVPCIRoot", str, ## __VA_ARGS__)

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

bool HyperVPCIRoot::registerChildPCIBridge(IOPCIBridge *pciBridge) {
  //
  // Locate root PCI bus instance.
  //
  OSDictionary *pciMatching = IOService::serviceMatching("HyperVPCIRoot");
  if (pciMatching == NULL) {
    SYSLOG("Failed to create HyperVPCIRoot matching dictionary");
    return false;
  }
  
  OSIterator *pciIterator = IOService::getMatchingServices(pciMatching);
  if (pciIterator == NULL) {
    SYSLOG("Failed to create HyperVPCIRoot matching iterator");
    return false;
  }
  
  pciIterator->reset();
  HyperVPCIRoot *pciInstance = OSDynamicCast(HyperVPCIRoot, pciIterator->getNextObject());
  pciIterator->release();
  
  if (pciInstance == NULL) {
    SYSLOG("Failed to locate HyperVPCIRoot instance");
    return false;
  }
  
  UInt8 busNum = pciBridge->firstBusNum();
  if (busNum != pciBridge->lastBusNum()) {
    return false;
  }
  
  if (pciInstance->pciBridges[busNum] != NULL) {
    return false;
  }
  
  DBGLOG("Bus %u registered", busNum);
  pciInstance->pciBridges[busNum] = pciBridge;
  return true;
}

bool HyperVPCIRoot::start(IOService *provider) {
  pciLock = IOSimpleLockAlloc();
  
  //
  // First bridge represents ourselves and will be NULL.
  //
  memset(pciBridges, 0, sizeof (pciBridges));
  
  if (!super::start(provider)) {
    SYSLOG("Dummy PCI bridge failed to initialize");
    return false;
  }
  
  DBGLOG("Dummy PCI bridge initialized");
  return true;
}

bool HyperVPCIRoot::configure(IOService *provider) {
  //
  // Add memory ranges from ACPI.
  //
  OSData *acpiAddressSpaces = OSDynamicCast(OSData, provider->getProperty("acpi-address-spaces"));
  if (acpiAddressSpaces != NULL) {
    AppleACPIRange *acpiRanges = (AppleACPIRange*) acpiAddressSpaces->getBytesNoCopy();
    UInt32 acpiRangeCount = acpiAddressSpaces->getLength() / sizeof (AppleACPIRange);
    
    for (int i = 0; i < acpiRangeCount; i++) {
      DBGLOG("type %u, min %llX, max %llX, len %llX", acpiRanges[i].type, acpiRanges[i].min, acpiRanges[i].max, acpiRanges[i].length);
      if (acpiRanges[i].type == 1) {
        addBridgeIORange(acpiRanges[i].min, acpiRanges[i].length);
      } else if (acpiRanges[i].type == 0) {
        addBridgeMemoryRange(acpiRanges[i].min, acpiRanges[i].length, true);
      }
    }
  }
  
  return super::configure(provider);
}

UInt32 HyperVPCIRoot::configRead32(IOPCIAddressSpace space, UInt8 offset) {
  DBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  UInt32 data;
  IOInterruptState ints;
  
  if (pciBridges[space.es.busNum] != NULL) {
    return pciBridges[space.es.busNum]->configRead32(space, offset);
  }
  

  
  ints = IOSimpleLockLockDisableInterrupt(pciLock);
  if (setConfigSpace(space, offset)) {
    data = inl(0xCFC);
  }
  
  if (offset == kIOPCIConfigurationOffsetBaseAddress0) {
    DBGLOG("gonna read BAR0 %X", data);
  }
  
  IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
  return data;
}

void HyperVPCIRoot::configWrite32(IOPCIAddressSpace space, UInt8 offset, UInt32 data) {
  DBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  IOInterruptState ints;
  
  if (pciBridges[space.es.busNum] != NULL) {
    pciBridges[space.es.busNum]->configWrite32(space, offset, data);
  }
  
  ints = IOSimpleLockLockDisableInterrupt(pciLock);
  if (setConfigSpace(space, offset)) {
    outl(0xCFC, data);
  }
  
  if (offset == kIOPCIConfigurationOffsetBaseAddress0) {
    DBGLOG("wrote BAR0 %X", data);
  }
  
  IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
}

UInt16 HyperVPCIRoot::configRead16(IOPCIAddressSpace space, UInt8 offset) {
  DBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  UInt16 data;
  IOInterruptState ints;
  
  if (pciBridges[space.es.busNum] != NULL) {
    return pciBridges[space.es.busNum]->configRead16(space, offset);
  }
  
  ints = IOSimpleLockLockDisableInterrupt(pciLock);
  if (setConfigSpace(space, offset)) {
    data = inw(0xCFC);
  }
  
  IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
  return data;
}

void HyperVPCIRoot::configWrite16(IOPCIAddressSpace space, UInt8 offset, UInt16 data) {
  DBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  IOInterruptState ints;
  
  if (pciBridges[space.es.busNum] != NULL) {
    pciBridges[space.es.busNum]->configWrite16(space, offset, data);
  }
  
  ints = IOSimpleLockLockDisableInterrupt(pciLock);
  if (setConfigSpace(space, offset)) {
    outw(0xCFC, data);
  }
  
  IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
}

UInt8 HyperVPCIRoot::configRead8(IOPCIAddressSpace space, UInt8 offset) {
  DBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  UInt8 data;
  IOInterruptState ints;
  
  if (pciBridges[space.es.busNum] != NULL) {
    return pciBridges[space.es.busNum]->configRead8(space, offset);
  }
  
  ints = IOSimpleLockLockDisableInterrupt(pciLock);
  if (setConfigSpace(space, offset)) {
    data = inb(0xCFC);
  }
  
  IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
  return data;
}

void HyperVPCIRoot::configWrite8(IOPCIAddressSpace space, UInt8 offset, UInt8 data) {
  DBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  IOInterruptState ints;
  
  if (pciBridges[space.es.busNum] != NULL) {
    pciBridges[space.es.busNum]->configWrite8(space, offset, data);
  }
  
  ints = IOSimpleLockLockDisableInterrupt(pciLock);
  if (setConfigSpace(space, offset)) {
    outb(0xCFC, data);
  }
  
  IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
}
