//
//  HyperVPCIBridge.hpp
//  Hyper-V PCI passthrough device support
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVPCIBridge_hpp
#define HyperVPCIBridge_hpp

#include "HyperV.hpp"
#include <IOKit/pci/IOPCIBridge.h>

#define super IOService

#define HVSYSLOG(str, ...) HVSYSLOG_PRINT("HyperVPCIBridge", str, ## __VA_ARGS__)
#define HVDBGLOG(str, ...) HVDBGLOG_PRINT("HyperVPCIBridge", str, ## __VA_ARGS__)

class HyperVPCIBridge : public IOService {
  OSDeclareDefaultStructors(HyperVPCIBridge);
  
public:
  //
  // IOService overrides.
  //
  virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#endif
