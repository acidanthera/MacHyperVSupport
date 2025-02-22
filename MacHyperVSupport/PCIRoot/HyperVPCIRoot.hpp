//
//  HyperVPCIRoot.hpp
//  Hyper-V PCI root bridge driver
//
//  Copyright © 2021-2025 Goldfish64. All rights reserved.
//

#ifndef HyperVPCIRoot_hpp
#define HyperVPCIRoot_hpp

#include <architecture/i386/pio.h>
#include <IOKit/pci/IOPCIBridge.h>

#include "HyperV.hpp"

class HyperVPCIRoot : public HV_PCIBRIDGE_CLASS {
  OSDeclareDefaultStructors(HyperVPCIRoot);
  HVDeclareLogFunctions("pcir");
  typedef HV_PCIBRIDGE_CLASS super;

private:
  IOSimpleLock *_pciLock = NULL;

  //
  // Child PCI bridges.
  //
  IOPCIBridge *pciBridges[256] {};

  //
  // Range allocators for low and high MMIO allocations.
  //
  bool canAllocateMMIO                  = false;
  IORangeAllocator *_rangeAllocatorLow  = nullptr;
  IORangeAllocator *_rangeAllocatorHigh = nullptr;
  bool reserveFramebufferArea();

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
  UInt8 firstBusNum() APPLE_KEXT_OVERRIDE { return 0; }
  UInt8 lastBusNum() APPLE_KEXT_OVERRIDE { return 0; }

  static HyperVPCIRoot* getPCIRootInstance();
  IOReturn registerChildPCIBridge(IOPCIBridge *pciBridge, UInt8 *busNumber);

  bool isHyperVGen2();
  IORangeScalar allocateRange(IORangeScalar size, IORangeScalar alignment, IORangeScalar maxAddress);
  void freeRange(IORangeScalar start, IORangeScalar size);
};

#endif
