//
//  HyperVModuleDevice.cpp
//  Hyper-V module device driver (ACPI resources for VMBus on Gen2)
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVModuleDevice.hpp"
#include <Headers/kern_api.hpp>

#include "AppleACPIRange.hpp"

OSDefineMetaClassAndStructors(HyperVModuleDevice, super);

bool HyperVModuleDevice::start(IOService *provider) {
  if (!super::start(provider)) {
    return false;
  }
  debugEnabled = checkKernelArgument("-hvmoddbg");
  
  //
  // Add memory ranges from ACPI.
  //
  OSData *acpiAddressSpaces = OSDynamicCast(OSData, provider->getProperty("acpi-address-spaces"));
  if (acpiAddressSpaces == nullptr) {
    HVSYSLOG("Unable to locate acpi-address-spaces property on VMOD device");
    
    super::stop(provider);
    return false;
  }
  
  AppleACPIRange *acpiRanges = (AppleACPIRange*) acpiAddressSpaces->getBytesNoCopy();
  UInt32 acpiRangeCount = acpiAddressSpaces->getLength() / sizeof (AppleACPIRange);
  
  OSArray *deviceMemoryArray = OSArray::withCapacity(acpiRangeCount);
  rangeAllocatorLow = IORangeAllocator::withRange(0);
  rangeAllocatorHigh = IORangeAllocator::withRange(0);
  if (deviceMemoryArray == nullptr || rangeAllocatorLow == nullptr || rangeAllocatorHigh == nullptr) {
    HVSYSLOG("Unable to allocate range allocators");
    
    OSSafeReleaseNULL(deviceMemoryArray);
    OSSafeReleaseNULL(rangeAllocatorLow);
    OSSafeReleaseNULL(rangeAllocatorHigh);
    super::stop(provider);
    return false;
  }
  
  for (int i = 0; i < acpiRangeCount; i++) {
    bool isHighRange = acpiRanges[i].min > UINT32_MAX;
    
    HVDBGLOG("Range type %u, min 0x%llX, max 0x%llX, len 0x%llX, high %u", acpiRanges[i].type, acpiRanges[i].min, acpiRanges[i].max, acpiRanges[i].length, isHighRange);
    
    IODeviceMemory *deviceMemory = IODeviceMemory::withRange(acpiRanges[i].min, acpiRanges[i].length);
    if (deviceMemory == nullptr) {
      HVSYSLOG("Unable to allocate device memory for range 0x%llX", acpiRanges[i].min);
      deviceMemoryArray->release();
      rangeAllocatorLow->release();
      rangeAllocatorHigh->release();
      super::stop(provider);
      return false;
    }
    
    // Add to device memory array and appropriate range allocator.
    deviceMemoryArray->setObject(deviceMemory);
    deviceMemory->release();
    if (isHighRange) {
      rangeAllocatorHigh->deallocate(acpiRanges[i].min, acpiRanges[i].length);
    } else {
      rangeAllocatorLow->deallocate(acpiRanges[i].min, acpiRanges[i].length);
    }
  }
  
  // Set device memory with found ranges.
  setDeviceMemory(deviceMemoryArray);
  deviceMemoryArray->release();
  
  // Reserve FB memory
  // TODO: make this dynamic
  rangeAllocatorLow->allocateRange(0x40000000, 0x4000000);
  
  HVDBGLOG("Hyper-V Module Device initialized with free size: %u bytes (low) %u bytes (high)",
           rangeAllocatorLow->getFreeCount(), rangeAllocatorHigh->getFreeCount());
  registerService();
  return true;
}

void HyperVModuleDevice::stop(IOService *provider) {
  rangeAllocatorLow->release();
  rangeAllocatorHigh->release();
}

IORangeScalar HyperVModuleDevice::allocateRange(IORangeScalar size, IORangeScalar alignment, bool highMemory) {
  IORangeScalar range;
  bool result = false;
  
  if (highMemory) {
    result = rangeAllocatorHigh->allocate(size, &range, alignment);
  } else {
    result = rangeAllocatorLow->allocate(size, &range, alignment);
  }
  HVDBGLOG("Allocation result for size 0x%llX (high: %u) - %u", size, highMemory, result);
  HVDBGLOG("Range result: 0x%llX", result ? range : 0);
  
  return result ? range : 0;
}

void HyperVModuleDevice::freeRange(IORangeScalar start, IORangeScalar size) {
  if (start > UINT32_MAX) {
    rangeAllocatorHigh->deallocate(start, size);
  } else {
    rangeAllocatorLow->deallocate(start, size);
  }
}
