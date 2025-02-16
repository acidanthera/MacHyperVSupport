//
//  HyperVGraphicsBridge.hpp
//  Hyper-V synthetic graphics bridge
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#ifndef HyperVGraphicsBridge_hpp
#define HyperVGraphicsBridge_hpp

#include <IOKit/pci/IOPCIBridge.h>
#include "HyperVVMBusDevice.hpp"
#include "HyperVGraphicsRegs.hpp"
#include "HyperVPCIRoot.hpp"

class HyperVGraphicsBridge : public HV_PCIBRIDGE_CLASS {
  OSDeclareDefaultStructors(HyperVGraphicsBridge);
  HVDeclareLogFunctions("gfxb");
  typedef HV_PCIBRIDGE_CLASS super;

private:
  HyperVPCIRoot     *_hvPCIRoot = nullptr;
  VMBusVersion      _currentGraphicsVersion = { };
  UInt8             _pciBusNumber = 0;

  //
  // Fake PCI structures.
  //
  IOSimpleLock      *_pciLock    = nullptr;
  UInt8             _fakePCIDeviceSpace[256];
  PE_Video          _consoleInfo = { };

  void fillFakePCIDeviceSpace();

public:
  //
  // IOService overrides.
  //
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  void stop(IOService *provider) APPLE_KEXT_OVERRIDE;

  //
  // IOPCIBridge overrides.
  //
  bool configure(IOService *provider) APPLE_KEXT_OVERRIDE;
  IODeviceMemory *ioDeviceMemory() APPLE_KEXT_OVERRIDE { HVDBGLOG("start"); return nullptr; }
  UInt32 configRead32(IOPCIAddressSpace space, UInt8 offset) APPLE_KEXT_OVERRIDE;
  void configWrite32(IOPCIAddressSpace space, UInt8 offset, UInt32 data) APPLE_KEXT_OVERRIDE;
  UInt16 configRead16(IOPCIAddressSpace space, UInt8 offset) APPLE_KEXT_OVERRIDE;
  void configWrite16(IOPCIAddressSpace space, UInt8 offset, UInt16 data) APPLE_KEXT_OVERRIDE;
  UInt8 configRead8(IOPCIAddressSpace space, UInt8 offset) APPLE_KEXT_OVERRIDE;
  void configWrite8(IOPCIAddressSpace space, UInt8 offset, UInt8 data) APPLE_KEXT_OVERRIDE;

  IOPCIAddressSpace getBridgeSpace() APPLE_KEXT_OVERRIDE {
    HVDBGLOG("start");
    IOPCIAddressSpace space = { 0 };
    return space;
  }

  UInt8 firstBusNum() APPLE_KEXT_OVERRIDE {
    HVDBGLOG("start");
    return _pciBusNumber;
  }

  UInt8 lastBusNum() APPLE_KEXT_OVERRIDE {
    HVDBGLOG("start");
    return _pciBusNumber;
  }
};

#endif
