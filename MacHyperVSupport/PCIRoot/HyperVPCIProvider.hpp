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

#define HVSYSLOG(str, ...) HVSYSLOG_PRINT("HyperVPCIProvider", false, 0, str, ## __VA_ARGS__)
#define HVDBGLOG(str, ...) HVDBGLOG_PRINT("HyperVPCIProvider", false, 0, str, ## __VA_ARGS__)

class HyperVPCIProvider : public IOACPIPlatformDevice {
  OSDeclareDefaultStructors(HyperVPCIProvider);
  typedef IOACPIPlatformDevice super;
  
public:
  //
  // IOService overrides.
  //
  virtual IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
  virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#endif
