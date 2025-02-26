//
//  HyperVACPIPlatformExpertShim.cpp
//  Hyper-V ACPI platform expert shim
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "HyperVACPIPlatformExpertShim.hpp"
#include "HyperVACPIFixup.hpp"

OSDefineMetaClassAndStructors(HyperVACPIPlatformExpertShim, super);

IOService* HyperVACPIPlatformExpertShim::probe(IOService *provider, SInt32 *score) {
  HVCheckDebugArgs();

  if (getKernelVersion() <= KernelVersion::SnowLeopard) {
    HVDBGLOG("Creating HyperVACPIFixup instance");
    HyperVACPIFixup::createInstance();
  } else {
    HVDBGLOG("Skipping ACPI fixup on XNU %u", getKernelVersion());
  }

  //
  // Don't actually bind to anything.
  //
  return nullptr;
}
