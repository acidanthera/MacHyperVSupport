//
//  HyperVPCIProvider.hpp
//  Hyper-V PCI root bridge provider
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVPCIProvider_hpp
#define HyperVPCIProvider_hpp

#include "HyperV.hpp"
#include <IOKit/acpi/IOACPIPlatformDevice.h>

class HyperVPCIProvider : public IOACPIPlatformDevice {
  OSDeclareDefaultStructors(HyperVPCIProvider);
  HVDeclareLogFunctions("pcip");
  typedef IOACPIPlatformDevice super;

public:
  //
  // IOService overrides.
  //
  IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#endif
