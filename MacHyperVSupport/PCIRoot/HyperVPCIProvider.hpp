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

#define super IOACPIPlatformDevice

#define SYSLOG(str, ...) SYSLOG_PRINT("HyperVPCIProvider", str, ## __VA_ARGS__)
#define DBGLOG(str, ...) DBGLOG_PRINT("HyperVPCIProvider", str, ## __VA_ARGS__)

class HyperVPCIProvider : public IOACPIPlatformDevice {
  OSDeclareDefaultStructors(HyperVPCIProvider);
  
public:
  //
  // IOService overrides.
  //
  virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#endif
