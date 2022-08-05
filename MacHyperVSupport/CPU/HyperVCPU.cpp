//
//  HyperVCPU.cpp
//  Hyper-V CPU disabler driver
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVCPU.hpp"

OSDefineMetaClassAndStructors(HyperVCPU, super);

#define kMaxCPUsTiger 16

IOService* HyperVCPU::probe(IOService *provider, SInt32 *score) {
#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_5
  //
  // Do not probe on versions newer than 10.4.
  // macOS 10.4 Tiger is the only version observed to have issues with this, as
  // Hyper-V can have large amounts of CPU objects in ACPI.
  //
  // Having more than the max number of CPU objects present will
  // cause stalls/panics during boot.
  //
  if (getKernelVersion() >= KernelVersion::Leopard) {
    return nullptr;
  }
  HVCheckDebugArgs();
  
  OSNumber *cpuIndexNumber = OSDynamicCast(OSNumber, provider->getProperty("cpu index"));
  if (cpuIndexNumber == nullptr) {
    return nullptr;
  }
  
  UInt32 cpuIndex = cpuIndexNumber->unsigned32BitValue();
  HVDBGLOG("Probing CPU %u", cpuIndex);
  
  //
  // Allow CPUs within the limit to load AppleACPICPU.
  //
  if (cpuIndex < kMaxCPUsTiger) {
    return nullptr;
  }
  
  HVDBGLOG("Blocking CPU %u", cpuIndex);
  return this;
#else
  return nullptr;
#endif
}
