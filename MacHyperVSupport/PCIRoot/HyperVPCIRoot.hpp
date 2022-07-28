//
//  HyperVPCIRoot.hpp
//  Hyper-V PCI root bridge driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVPCIRoot_hpp
#define HyperVPCIRoot_hpp

#include "HyperV.hpp"
#include <IOKit/pci/IOPCIBridge.h>

class HyperVPCIRoot : public HV_PCIBRIDGE_CLASS {
  OSDeclareDefaultStructors(HyperVPCIRoot);
  typedef HV_PCIBRIDGE_CLASS super;
  
private:
  IOSimpleLock *pciLock = NULL;
  
  inline bool setConfigSpace(IOPCIAddressSpace space, UInt8 offset);
  
  IOPCIBridge *pciBridges[256] {};
  
public:
  static bool registerChildPCIBridge(IOPCIBridge *pciBridge);
  
  //
  // IOService overrides.
  //
  virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  
  //
  // IOPCIBridge overrides.
  //
  virtual bool configure(IOService *provider) APPLE_KEXT_OVERRIDE;
  IODeviceMemory *ioDeviceMemory() APPLE_KEXT_OVERRIDE { return NULL; }
  
  UInt32 configRead32(IOPCIAddressSpace space, UInt8 offset) APPLE_KEXT_OVERRIDE;
  void configWrite32(IOPCIAddressSpace space, UInt8 offset, UInt32 data) APPLE_KEXT_OVERRIDE;
  UInt16 configRead16(IOPCIAddressSpace space, UInt8 offset) APPLE_KEXT_OVERRIDE;
  void configWrite16(IOPCIAddressSpace space, UInt8 offset, UInt16 data) APPLE_KEXT_OVERRIDE;
  UInt8 configRead8(IOPCIAddressSpace space, UInt8 offset) APPLE_KEXT_OVERRIDE;
  void configWrite8(IOPCIAddressSpace space, UInt8 offset, UInt8 data) APPLE_KEXT_OVERRIDE;

  IOPCIAddressSpace getBridgeSpace() APPLE_KEXT_OVERRIDE {
    IOPCIAddressSpace space = { 0 };
    return space;
  }

  UInt8 firstBusNum() APPLE_KEXT_OVERRIDE {
    return 0;
  }
  
  UInt8 lastBusNum() APPLE_KEXT_OVERRIDE {
    return 0;
  }
};

#endif
