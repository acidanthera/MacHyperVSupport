//
//  HyperVPCIRoot.cpp
//  Hyper-V PCI root bridge driver
//
//  Copyright © 2021-2025 Goldfish64. All rights reserved.
//

#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include <IOKit/acpi/IOACPITypes.h>
#include <IOKit/IODeviceTreeSupport.h>

#include "HyperVPCIRoot.hpp"

OSDefineMetaClassAndStructors(HyperVPCIRoot, super);

inline bool setConfigSpace(IOPCIAddressSpace space, UInt8 offset) {
  offset &= 0xFC;
  
  UInt32 pciCycle = (1 << 31) | space.bits | offset;
  
  outl(0xCF8, pciCycle);
  return true;
}

bool HyperVPCIRoot::start(IOService *provider) {
  bool result = false;

  HVCheckDebugArgs();
  HVDBGLOG("Initializing Hyper-V Root PCI Bridge");

  _pciLock = IOSimpleLockAlloc();
  if (_pciLock == nullptr) {
    HVSYSLOG("Failed to allocate lock");
    return false;
  }

  //
  // First bridge represents ourselves and will be NULL.
  //
  memset(pciBridges, 0, sizeof (pciBridges));

  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    return false;
  }

  do {
    //
    // Add memory ranges from ACPI.
    //
    OSData *acpiAddressSpaces = OSDynamicCast(OSData, provider->getProperty("acpi-address-spaces"));
    if (acpiAddressSpaces == nullptr) {
      HVSYSLOG("Unable to locate acpi-address-spaces property, MMIO services will be unavailable");
    } else {
      IOACPIAddressSpaceDescriptor *acpiRanges    = (IOACPIAddressSpaceDescriptor*) acpiAddressSpaces->getBytesNoCopy();
      UInt32                       acpiRangeCount = acpiAddressSpaces->getLength() / sizeof (*acpiRanges);

      _rangeAllocatorLow  = IORangeAllocator::withRange(0);
      _rangeAllocatorHigh = IORangeAllocator::withRange(0);
      if (_rangeAllocatorLow == nullptr || _rangeAllocatorHigh == nullptr) {
        HVSYSLOG("Unable to allocate range allocators");
        break;
      }
      
      HVDBGLOG("Got %u ACPI ranges", acpiRangeCount);
      for (int i = 0; i < acpiRangeCount; i++) {
        HVDBGLOG("ACPI range type %u: minimum 0x%llX, maximum 0x%llX, length 0x%llX, high %u", acpiRanges[i].resourceType,
                 acpiRanges[i].minAddressRange, acpiRanges[i].maxAddressRange, acpiRanges[i].addressLength,
                 acpiRanges[i].minAddressRange > UINT32_MAX);

        //
        // Skip any non-memory ranges (should not occur normally).
        //
        if (acpiRanges[i].resourceType != kIOACPIMemoryRange) {
          continue;
        }

        //
        // Add to appropriate range allocator.
        //
        if (acpiRanges[i].minAddressRange > UINT32_MAX) {
          _rangeAllocatorHigh->deallocate(static_cast<IOPhysicalAddress>(acpiRanges[i].minAddressRange),
                                          static_cast<IOPhysicalLength>(acpiRanges[i].addressLength));
        } else {
          _rangeAllocatorLow->deallocate(static_cast<IOPhysicalAddress>(acpiRanges[i].minAddressRange),
                                         static_cast<IOPhysicalLength>(acpiRanges[i].addressLength));
        }
      }

      //
      // Reserve FB memory.
      //
      reserveFramebufferArea();
      HVDBGLOG("Initialized Hyper-V Root PCI Bridge with free size: %u bytes (low) %u bytes (high)",
               _rangeAllocatorLow->getFreeCount(), _rangeAllocatorHigh->getFreeCount());
      canAllocateMMIO = true;
    }

    result = true;
  } while (false);

  if (!result) {
    stop(provider);
  }
  return result;
}

void HyperVPCIRoot::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V Root PCI Bridge");

  if (_pciLock != nullptr) {
    IOSimpleLockFree(_pciLock);
  }

  super::stop(provider);
}

bool HyperVPCIRoot::configure(IOService *provider) {
  //
  // Add memory ranges from ACPI.
  //
  OSData *acpiAddressSpaces = OSDynamicCast(OSData, provider->getProperty("acpi-address-spaces"));
  if (acpiAddressSpaces != nullptr) {
    IOACPIAddressSpaceDescriptor *acpiRanges = (IOACPIAddressSpaceDescriptor*) acpiAddressSpaces->getBytesNoCopy();
    UInt32 acpiRangeCount = acpiAddressSpaces->getLength() / sizeof (*acpiRanges);

    HVDBGLOG("Got %u ACPI ranges", acpiRangeCount);
    for (int i = 0; i < acpiRangeCount; i++) {
      HVDBGLOG("ACPI range type %u: minimum 0x%llX, maximum 0x%llX, length 0x%llX", acpiRanges[i].resourceType,
               acpiRanges[i].minAddressRange, acpiRanges[i].maxAddressRange, acpiRanges[i].addressLength);
      if (acpiRanges[i].resourceType == kIOACPIIORange) {
        addBridgeIORange(acpiRanges[i].minAddressRange, acpiRanges[i].addressLength);
      } else if (acpiRanges[i].resourceType == kIOACPIMemoryRange) {
        addBridgeMemoryRange(acpiRanges[i].minAddressRange, acpiRanges[i].addressLength, true);
      }
    }
  }

  return super::configure(provider);
}

UInt32 HyperVPCIRoot::configRead32(IOPCIAddressSpace space, UInt8 offset) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  UInt32 data = 0xFFFFFFFF;
  IOInterruptState ints;
  
  if (pciBridges[space.es.busNum] != NULL) {
    return pciBridges[space.es.busNum]->configRead32(space, offset);
  }
  

  
  ints = IOSimpleLockLockDisableInterrupt(_pciLock);
  if (setConfigSpace(space, offset)) {
    data = inl(0xCFC);
  }
  
  if (offset == kIOPCIConfigurationOffsetBaseAddress0) {
    HVDBGLOG("gonna read BAR0 %X", data);
  }
  
  IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);
  return data;
}

void HyperVPCIRoot::configWrite32(IOPCIAddressSpace space, UInt8 offset, UInt32 data) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  IOInterruptState ints;
  
  if (pciBridges[space.es.busNum] != NULL) {
    pciBridges[space.es.busNum]->configWrite32(space, offset, data);
  }
  
  ints = IOSimpleLockLockDisableInterrupt(_pciLock);
  if (setConfigSpace(space, offset)) {
    outl(0xCFC, data);
  }
  
  if (offset == kIOPCIConfigurationOffsetBaseAddress0) {
    HVDBGLOG("wrote BAR0 %X", data);
  }
  
  IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);
}

UInt16 HyperVPCIRoot::configRead16(IOPCIAddressSpace space, UInt8 offset) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  UInt16 data = 0xFFFF;
  IOInterruptState ints;
  
  if (pciBridges[space.es.busNum] != NULL) {
    return pciBridges[space.es.busNum]->configRead16(space, offset);
  }
  
  ints = IOSimpleLockLockDisableInterrupt(_pciLock);
  if (setConfigSpace(space, offset)) {
    data = inw(0xCFC);
  }
  
  IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);
  return data;
}

void HyperVPCIRoot::configWrite16(IOPCIAddressSpace space, UInt8 offset, UInt16 data) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  IOInterruptState ints;
  
  if (pciBridges[space.es.busNum] != NULL) {
    pciBridges[space.es.busNum]->configWrite16(space, offset, data);
  }
  
  ints = IOSimpleLockLockDisableInterrupt(_pciLock);
  if (setConfigSpace(space, offset)) {
    outw(0xCFC, data);
  }
  
  IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);
}

UInt8 HyperVPCIRoot::configRead8(IOPCIAddressSpace space, UInt8 offset) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  UInt8 data = 0xFF;
  IOInterruptState ints;
  
  if (pciBridges[space.es.busNum] != NULL) {
    return pciBridges[space.es.busNum]->configRead8(space, offset);
  }
  
  ints = IOSimpleLockLockDisableInterrupt(_pciLock);
  if (setConfigSpace(space, offset)) {
    data = inb(0xCFC);
  }
  
  IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);
  return data;
}

void HyperVPCIRoot::configWrite8(IOPCIAddressSpace space, UInt8 offset, UInt8 data) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  IOInterruptState ints;
  
  if (pciBridges[space.es.busNum] != NULL) {
    pciBridges[space.es.busNum]->configWrite8(space, offset, data);
  }
  
  ints = IOSimpleLockLockDisableInterrupt(_pciLock);
  if (setConfigSpace(space, offset)) {
    outb(0xCFC, data);
  }
  
  IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);
}

HyperVPCIRoot* HyperVPCIRoot::getPCIRootInstance() {
  HyperVPCIRoot *hvPCIRoot = nullptr;

  //
  // Get HyperVPCIRoot instance used for allocating MMIO regions for Hyper-V Gen1 VMs.
  //
  OSDictionary *pciRootMatching = IOService::serviceMatching("HyperVPCIRoot");
  if (pciRootMatching == nullptr) {
    return nullptr;
  }

#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_6
  IOService *pciRootService = IOService::waitForService(pciRootMatching);
  if (pciRootService != nullptr) {
    pciRootService->retain();
  }
#else
  IOService *pciRootService = waitForMatchingService(pciRootMatching);
  pciRootMatching->release();
#endif

  if (pciRootService == nullptr) {
    return nullptr;
  }
  hvPCIRoot = OSDynamicCast(HyperVPCIRoot, pciRootService);
  pciRootService->release(); // TODO: is this needed?

  if (hvPCIRoot != nullptr) {
    hvPCIRoot->HVDBGLOG("Got instance of HyperVPCIRoot");
  }
  return hvPCIRoot;
}

IOReturn HyperVPCIRoot::registerChildPCIBridge(IOPCIBridge *pciBridge, UInt8 *busNumber) {
  IOInterruptState ints = IOSimpleLockLockDisableInterrupt(_pciLock);
  
  //
  // Find free bus number.
  //
  for (UInt8 busIndex = 0x10; busIndex <= 0xFE; busIndex++) {
    if (pciBridges[busIndex] == nullptr) {
      pciBridges[busIndex] = pciBridge;
      *busNumber = busIndex;

      IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);
      HVDBGLOG("PCI bus %u registered", busIndex);
      return kIOReturnSuccess;
    }
  }
  IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);

  HVSYSLOG("No more free PCI bus numbers available");
  return kIOReturnNoResources;
}

bool HyperVPCIRoot::isHyperVGen2() {
  //
  // Check for presence of PCI0. Gen2 VMs do not have a PCI bus.
  //
  IORegistryEntry *pciEntry = IORegistryEntry::fromPath("/PCI0@0", gIODTPlane);
  bool isGen2 = pciEntry == nullptr;
  OSSafeReleaseNULL(pciEntry);

  HVDBGLOG("Hyper-V VM generation: %u", isGen2 ? 2 : 1);
  return isGen2;
}

IORangeScalar HyperVPCIRoot::allocateRange(IORangeScalar size, IORangeScalar alignment, IORangeScalar maxAddress) {
  IORangeScalar range = 0;
  bool result         = false;

  if (!canAllocateMMIO) {
    return 0;
  }

  if (maxAddress > UINT32_MAX) {
    result = _rangeAllocatorHigh->allocate(size, &range, alignment);
  }
  if (!result) {
    result = _rangeAllocatorLow->allocate(size, &range, alignment);
  }

  HVDBGLOG("Allocation result for length %p (max %p) - %u", size, maxAddress, result);
  HVDBGLOG("Range base: %p", result ? range : 0);

  return result ? range : 0;
}

void HyperVPCIRoot::freeRange(IORangeScalar start, IORangeScalar size) {
  if (!canAllocateMMIO) {
    return;
  }

  if (start > UINT32_MAX) {
    _rangeAllocatorHigh->deallocate(start, size);
  } else {
    _rangeAllocatorLow->deallocate(start, size);
  }
  HVDBGLOG("Freed range base %p length %p", start, size);
}

bool HyperVPCIRoot::reserveFramebufferArea() {
  PE_Video consoleInfo;
  IORangeScalar fbStart;
  IORangeScalar fbLength;

  //
  // Pull console info. We'll use the base address but the length will be gathered from Hyper-V.
  //
  if (getPlatform()->getConsoleInfo(&consoleInfo) != kIOReturnSuccess) {
    HVSYSLOG("Failed to get console info");
    return false;
  }
  fbStart  = consoleInfo.v_baseAddr;
  fbLength = consoleInfo.v_height * consoleInfo.v_rowBytes;
  HVDBGLOG("Console is at 0x%X size 0x%X (%ux%u, bpp: %u, bytes/row: %u)",
           fbStart, fbLength, consoleInfo.v_width, consoleInfo.v_height,
           consoleInfo.v_depth, consoleInfo.v_rowBytes);

  //
  // Allocate intial framebuffer area to prevent use.
  // On some versions of Hyper-V, the initial framebuffer may not actually be in the MMIO ranges.
  // This can be silently ignored.
  //
  if (fbStart > UINT32_MAX) {
    _rangeAllocatorHigh->allocateRange(fbStart, fbLength);
  } else {
    _rangeAllocatorLow->allocateRange(fbStart, fbLength);
  }
  return true;
}
