//
//  HyperVModuleDevice.cpp
//  Hyper-V module device driver (ACPI resources for VMBus on Gen2)
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVModuleDevice.hpp"
#include <Headers/kern_api.hpp>

#include "AppleACPIRange.hpp"

#define super IOService

#define HVSYSLOG(str, ...) HVSYSLOG_PRINT("HyperVModuleDevice", false, 0, str, ## __VA_ARGS__)
#define HVDBGLOG(str, ...) \
  if (this->debugEnabled) HVDBGLOG_PRINT("HyperVModuleDevice", false, 0, str, ## __VA_ARGS__)

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
  if (acpiAddressSpaces != nullptr) {
    AppleACPIRange *acpiRanges = (AppleACPIRange*) acpiAddressSpaces->getBytesNoCopy();
    UInt32 acpiRangeCount = acpiAddressSpaces->getLength() / sizeof (AppleACPIRange);
    
    OSArray *deviceMemoryArray = OSArray::withCapacity(acpiRangeCount);
    rangeAllocator = IORangeAllocator::withRange(0);
    if (deviceMemoryArray != nullptr) {
      for (int i = 0; i < acpiRangeCount; i++) {
        HVDBGLOG("Range type %u, min 0x%llX, max 0x%llX, len 0x%llX", acpiRanges[i].type, acpiRanges[i].min, acpiRanges[i].max, acpiRanges[i].length);
        
        IODeviceMemory *deviceMemory = IODeviceMemory::withRange(acpiRanges[i].min, acpiRanges[i].length);
        if (deviceMemory != nullptr) {
          deviceMemoryArray->setObject(deviceMemory);
          rangeAllocator->deallocate(acpiRanges[i].min, acpiRanges[i].length);
          deviceMemory->release();
        }
      }
      setDeviceMemory(deviceMemoryArray);
      deviceMemoryArray->release();
    }
  }
  
  rangeAllocator->allocateRange(0x40000000, 0x4000000);
  
  HVDBGLOG("Hyper-V Module Device initialized with free size: %u bytes", rangeAllocator->getFreeCount());
  registerService();
  return true;
}
