//
//  HyperVGraphics.hpp
//  Hyper-V basic graphics driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVGraphics_hpp
#define HyperVGraphics_hpp

#include "HyperVVMBusDevice.hpp"
#include "HyperV.hpp"

#include <IOKit/pci/IOPCIBridge.h>

#define super IOPCIBridge

#define SYSLOG(str, ...) SYSLOG_PRINT("HyperVGraphics", str, ## __VA_ARGS__)
#define DBGLOG(str, ...) DBGLOG_PRINT("HyperVGraphics", str, ## __VA_ARGS__)

class HyperVGraphics : public IOPCIBridge {
  OSDeclareDefaultStructors(HyperVGraphics);
  
private:
  
public:
  //
  // IOService overrides.
  //
  virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  
  //
  // IOPCIBridge overrides.
  //
  IODeviceMemory *ioDeviceMemory() APPLE_KEXT_OVERRIDE { DBGLOG("start");return NULL; }
  UInt32 configRead32(IOPCIAddressSpace space, UInt8 offset) APPLE_KEXT_OVERRIDE {
    DBGLOG("configRead32 bus %u func %u offset %u", space.es.busNum, space.es.functionNum, offset);
    return 0xFFFFFFFF; }
  void configWrite32(IOPCIAddressSpace space, UInt8 offset, UInt32 data) APPLE_KEXT_OVERRIDE { }
  UInt16 configRead16(IOPCIAddressSpace space, UInt8 offset) APPLE_KEXT_OVERRIDE {  DBGLOG("configRead16 bus %u func %u offset %u", space.es.busNum, space.es.functionNum, offset);return 0xFFFF; }
  void configWrite16(IOPCIAddressSpace space, UInt8 offset, UInt16 data) APPLE_KEXT_OVERRIDE { }
  UInt8 configRead8(IOPCIAddressSpace space, UInt8 offset) APPLE_KEXT_OVERRIDE { DBGLOG("configRead8 bus %u func %u offset %u", space.es.busNum, space.es.functionNum, offset); return 0xFF; }
  void configWrite8(IOPCIAddressSpace space, UInt8 offset, UInt8 data) APPLE_KEXT_OVERRIDE { }

  IOPCIAddressSpace getBridgeSpace() APPLE_KEXT_OVERRIDE {
    DBGLOG("start");
    IOPCIAddressSpace space = { 0 };
    return space;
  }

  UInt8 firstBusNum() APPLE_KEXT_OVERRIDE {
    DBGLOG("start");
    return kHyperVPCIBusSyntheticGraphics;
  }
  
  UInt8 lastBusNum() APPLE_KEXT_OVERRIDE {
    DBGLOG("start");
    return kHyperVPCIBusSyntheticGraphics;
  }
};

#endif
