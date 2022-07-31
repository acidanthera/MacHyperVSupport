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
  HVDeclareLogFunctions();
  typedef IOACPIPlatformDevice super;
  
public:
  //
  // IOService overrides.
  //
  virtual IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
  virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#endif
