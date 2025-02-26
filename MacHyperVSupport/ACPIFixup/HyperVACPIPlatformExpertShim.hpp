//
//  HyperVACPIPlatformExpertShim.hpp
//  Hyper-V ACPI platform expert shim
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#ifndef HyperVACPIPlatformExpertShim_hpp
#define HyperVACPIPlatformExpertShim_hpp

#include <IOKit/IOService.h>
#include "HyperV.hpp"

class HyperVACPIPlatformExpertShim : public IOService {
  OSDeclareDefaultStructors(HyperVACPIPlatformExpertShim);
  HVDeclareLogFunctions("acpishim");
  typedef IOService super;

public:
  //
  // IOService overrides.
  //
  IOService* probe(IOService *provider,SInt32 *score) APPLE_KEXT_OVERRIDE;
};

#endif
