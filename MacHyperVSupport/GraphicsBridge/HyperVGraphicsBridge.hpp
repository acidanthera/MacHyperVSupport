//
//  HyperVGraphicsBridge.hpp
//  Hyper-V synthetic graphics bridge
//
//  Copyright © 2021-2025 Goldfish64. All rights reserved.
//

#ifndef HyperVGraphicsBridge_hpp
#define HyperVGraphicsBridge_hpp

#include <IOKit/pci/IOPCIBridge.h>

#include "HyperVVMBusDevice.hpp"

class HyperVGraphicsBridge : public HV_PCIBRIDGE_CLASS {
  OSDeclareDefaultStructors(HyperVGraphicsBridge);
  HVDeclareLogFunctions("gfxb");
  typedef HV_PCIBRIDGE_CLASS super;

private:
  UInt8             _pciBusNumber = 0;

  //
  // Fake PCI structures.
  //
  IOSimpleLock      *_pciLock   = nullptr;
  UInt8             _fakePCIDeviceSpace[256];
  UInt32            _fbInitialBase    = 0;
  UInt32            _fbInitialLength  = 0;

public:
  //
  // IOService overrides.
  //
  IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  void stop(IOService *provider) APPLE_KEXT_OVERRIDE;

  //
  // IOPCIBridge overrides.
  //
  bool configure(IOService *provider) APPLE_KEXT_OVERRIDE;
  IODeviceMemory *ioDeviceMemory() APPLE_KEXT_OVERRIDE { return nullptr; }
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
  UInt8 firstBusNum() APPLE_KEXT_OVERRIDE { return _pciBusNumber; }
  UInt8 lastBusNum() APPLE_KEXT_OVERRIDE { return _pciBusNumber; }
};

#endif
