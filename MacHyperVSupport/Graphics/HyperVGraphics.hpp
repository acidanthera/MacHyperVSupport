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
  IOSimpleLock *pciLock;
  UInt8 fakePCIDeviceSpace[256];
  
  void fillFakePCIDeviceSpace();
  
  PE_Video consoleInfo;
  
public:
  //
  // IOService overrides.
  //
  virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  
  //
  // IOPCIBridge overrides.
  //
  virtual bool configure(IOService *provider) APPLE_KEXT_OVERRIDE;
  IODeviceMemory *ioDeviceMemory() APPLE_KEXT_OVERRIDE { DBGLOG("start"); return NULL; }
  UInt32 configRead32(IOPCIAddressSpace space, UInt8 offset) APPLE_KEXT_OVERRIDE;
  void configWrite32(IOPCIAddressSpace space, UInt8 offset, UInt32 data) APPLE_KEXT_OVERRIDE;
  UInt16 configRead16(IOPCIAddressSpace space, UInt8 offset) APPLE_KEXT_OVERRIDE;
  void configWrite16(IOPCIAddressSpace space, UInt8 offset, UInt16 data) APPLE_KEXT_OVERRIDE;
  UInt8 configRead8(IOPCIAddressSpace space, UInt8 offset) APPLE_KEXT_OVERRIDE;
  void configWrite8(IOPCIAddressSpace space, UInt8 offset, UInt8 data) APPLE_KEXT_OVERRIDE;

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
