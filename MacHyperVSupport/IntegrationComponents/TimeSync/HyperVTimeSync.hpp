//
//  HyperVTimeSync.hpp
//  Hyper-V time synchronization driver
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVTimeSync_hpp
#define HyperVTimeSync_hpp

#include "HyperVICService.hpp"
#include "HyperVTimeSyncRegs.hpp"

class HyperVTimeSync : public HyperVICService {
  OSDeclareDefaultStructors(HyperVTimeSync);
  HVDeclareLogFunctionsVMBusChild("time");
  typedef HyperVICService super;

private:
  VMBusICVersion _timeSyncCurrentVersion = kHyperVTimeSyncVersionV1_0;
  IOService      *_userClientInstance    = nullptr;

protected:
  void handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) APPLE_KEXT_OVERRIDE;
  void handleTimeAdjust(UInt64 hostTime, UInt64 referenceTime, VMBusICTimeSyncFlags flags);

public:
  //
  // IOService overrides.
  //
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
  bool open(IOService *forClient, IOOptionBits options = 0, void *arg = nullptr) APPLE_KEXT_OVERRIDE;
  void close(IOService *forClient, IOOptionBits options = 0) APPLE_KEXT_OVERRIDE;
};

#endif
