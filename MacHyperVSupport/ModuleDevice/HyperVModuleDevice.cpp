//
//  HyperVModuleDevice.cpp
//  Hyper-V module device driver (ACPI resources for VMBus on Gen2)
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVModuleDevice.hpp"
#include <IOKit/acpi/IOACPITypes.h>

OSDefineMetaClassAndStructors(HyperVModuleDevice, super);

bool HyperVModuleDevice::start(IOService *provider) {
  IOACPIAddressSpaceDescriptor  *acpiRanges;
  UInt32                        acpiRangeCount;
  OSArray                       *deviceMemoryArray;
  IODeviceMemory                *deviceMemory;

  HVCheckDebugArgs();
  HVDBGLOG("Initializing Hyper-V Module Device");
  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    return false;
  }

  //
  // Add memory ranges from ACPI.
  //
  OSData *acpiAddressSpaces = OSDynamicCast(OSData, provider->getProperty("acpi-address-spaces"));
  if (acpiAddressSpaces == nullptr) {
    HVSYSLOG("Unable to locate acpi-address-spaces property on VMOD device");
    stop(provider);
    return false;
  }

  acpiRanges     = (IOACPIAddressSpaceDescriptor*) acpiAddressSpaces->getBytesNoCopy();
  acpiRangeCount = acpiAddressSpaces->getLength() / sizeof (*acpiRanges);

  deviceMemoryArray   = OSArray::withCapacity(acpiRangeCount);
  _rangeAllocatorLow  = IORangeAllocator::withRange(0);
  _rangeAllocatorHigh = IORangeAllocator::withRange(0);
  if (deviceMemoryArray == nullptr || _rangeAllocatorLow == nullptr || _rangeAllocatorHigh == nullptr) {
    HVSYSLOG("Unable to allocate range allocators");

    OSSafeReleaseNULL(deviceMemoryArray);
    stop(provider);
    return false;
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
    // Create device memory range.
    // Add to device memory array and appropriate range allocator.
    //
    deviceMemory = IODeviceMemory::withRange(static_cast<IOPhysicalAddress>(acpiRanges[i].minAddressRange),
                                             static_cast<IOPhysicalLength>(acpiRanges[i].addressLength));
    if (deviceMemory == nullptr) {
      HVSYSLOG("Unable to allocate device memory for range 0x%llX", acpiRanges[i].minAddressRange);
      OSSafeReleaseNULL(deviceMemoryArray);
      stop(provider);
      return false;
    }

    deviceMemoryArray->setObject(deviceMemory);
    deviceMemory->release();
    if (acpiRanges[i].minAddressRange > UINT32_MAX) {
      _rangeAllocatorHigh->deallocate(static_cast<IOPhysicalAddress>(acpiRanges[i].minAddressRange),
                                      static_cast<IOPhysicalLength>(acpiRanges[i].addressLength));
    } else {
      _rangeAllocatorLow->deallocate(static_cast<IOPhysicalAddress>(acpiRanges[i].minAddressRange),
                                     static_cast<IOPhysicalLength>(acpiRanges[i].addressLength));
    }
  }

  //
  // Set device memory with found ranges.
  //
  setDeviceMemory(deviceMemoryArray);
  deviceMemoryArray->release();

  //
  // Reserve FB memory.
  // TODO: make this dynamic
  //
  _rangeAllocatorLow->allocateRange(0x40000000, 0x4000000);

  HVDBGLOG("Initialized Hyper-V Module Device with free size: %u bytes (low) %u bytes (high)",
           _rangeAllocatorLow->getFreeCount(), _rangeAllocatorHigh->getFreeCount());
  registerService();
  return true;
}

void HyperVModuleDevice::stop(IOService *provider) {
  HVDBGLOG("Hyper-V Module Device is stopping");

  OSSafeReleaseNULL(_rangeAllocatorLow);
  OSSafeReleaseNULL(_rangeAllocatorHigh);

  super::stop(provider);
}

IORangeScalar HyperVModuleDevice::allocateRange(IORangeScalar size, IORangeScalar alignment, bool highMemory) {
  IORangeScalar range;
  bool result = false;

  if (highMemory) {
    result = _rangeAllocatorHigh->allocate(size, &range, alignment);
  } else {
    result = _rangeAllocatorLow->allocate(size, &range, alignment);
  }
  HVDBGLOG("Allocation result for length %p (high: %u) - %u", size, highMemory, result);
  HVDBGLOG("Range base: %p", result ? range : 0);

  return result ? range : 0;
}

void HyperVModuleDevice::freeRange(IORangeScalar start, IORangeScalar size) {
  if (start > UINT32_MAX) {
    _rangeAllocatorHigh->deallocate(start, size);
  } else {
    _rangeAllocatorLow->deallocate(start, size);
  }
  HVDBGLOG("Freed range base %p length %p", start, size);
}
