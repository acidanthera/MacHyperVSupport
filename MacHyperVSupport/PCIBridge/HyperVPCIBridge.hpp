//
//  HyperVPCIBridge.hpp
//  Hyper-V PCI passthrough device support
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVPCIBridge_hpp
#define HyperVPCIBridge_hpp

#include <IOKit/pci/IOPCIBridge.h>

#include "HyperVVMBusDevice.hpp"
#include "HyperVPCIBridgeRegs.hpp"
#include "HyperVPCIRoot.hpp"

#include "HyperVModuleDevice.hpp"

class HyperVPCIBridge : public HV_PCIBRIDGE_CLASS {
  OSDeclareDefaultStructors(HyperVPCIBridge);
  HVDeclareLogFunctionsVMBusChild("pcib");
  typedef HV_PCIBRIDGE_CLASS super;
  
private:
  //
  // Parent VMBus device and PCI state.
  //
  HyperVVMBusDevice       *_hvDevice    = nullptr;
  HyperVPCIRoot           *_hvPCIRoot   = nullptr;
  UInt8                   _pciBusNumber = 0;
  IOSimpleLock            *_pciLock     = nullptr;
  
  HyperVPCIBridgeProtocolVersion  currentPciVersion;
  
  //
  // MMIO.
  //
  HyperVModuleDevice  *_hvModuleDevice              = nullptr;
  IORangeScalar       _pciConfigSpace               = 0;
  IOMemoryDescriptor  *_pciConfigMemoryDescriptor   = nullptr;
  IOMemoryMap         *_pciConfigMemoryMap          = nullptr;
  UInt64              _bars[kHyperVPCIBarCount]     = { };
  UInt64              _barSizes[kHyperVPCIBarCount] = { };

  UInt32                        _pciFunctionsCount = 0;
  HyperVPCIFunctionDescription  *_pciFunctions     = nullptr;

  //
  // Packet handling.
  //
  bool wakePacketHandler(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength);
  void handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength);
  void handleIncomingPCIMessage(HyperVPCIBridgeIncomingMessageHeader *pciMsgHeader, UInt32 msgSize);

  //
  // PCI bus setup.
  //
  IOReturn connectPCIBus();
  IOReturn negotiateProtocolVersion();
  IOReturn allocatePCIConfigWindow();
  IOReturn queryBusRelations();
  IOReturn enterPCID0();
  IOReturn queryResourceRequirements();
  IOReturn sendResourcesAssigned(UInt32 slot);

  inline UInt64 getBarSize(UInt64 barValue) {
    if (barValue < UINT32_MAX) {
      barValue |= 0xFFFFFFFF00000000;
    }
    return roundup((1 + ~(barValue & kHyperVPCIBarMemoryMask)), PAGE_SIZE);
  }

  //
  // Device properties handling.
  //
  IOReturn mergePropertiesFromDT(UInt32 slot, OSDictionary *dict);

  //
  // PCI config space read/write.
  //
  UInt32 readPCIConfig(UInt32 offset, UInt8 size);
  void writePCIConfig(UInt32 offset, UInt8 size, UInt32 value);

public:
  //
  // IOService overrides.
  //
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  IOReturn callPlatformFunction(const OSSymbol *functionName, bool waitForFunction,
                                void *param1, void *param2, void *param3, void *param4) APPLE_KEXT_OVERRIDE;

  //
  // IOPCIBridge overrides.
  //
  bool configure(IOService *provider) APPLE_KEXT_OVERRIDE;
  IODeviceMemory *ioDeviceMemory() APPLE_KEXT_OVERRIDE { HVDBGLOG("start"); return NULL; }
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

  bool initializeNub(IOPCIDevice *nub, OSDictionary *from) APPLE_KEXT_OVERRIDE;
};

#endif
