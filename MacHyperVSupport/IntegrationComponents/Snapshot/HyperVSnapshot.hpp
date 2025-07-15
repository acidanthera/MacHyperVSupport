//
//  HyperVSnapshot.hpp
//  Hyper-V snapshot (VSS) driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#ifndef HyperVSnapshot_hpp
#define HyperVSnapshot_hpp

#include "HyperVICService.hpp"
#include "HyperVSnapshotRegs.hpp"
#include "HyperVSnapshotUserClientInternal.hpp"

class HyperVSnapshot : public HyperVICService {
  OSDeclareDefaultStructors(HyperVSnapshot);
  HVDeclareLogFunctionsVMBusChild("snap");
  typedef HyperVICService super;

private:
  HyperVSnapshotUserClient *_userClientInstance = nullptr;

protected:
  void handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) APPLE_KEXT_OVERRIDE;

public:
  //
  // IOService overrides.
  //
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
  bool open(IOService *forClient, IOOptionBits options, void *arg) APPLE_KEXT_OVERRIDE;
  void close(IOService *forClient, IOOptionBits options) APPLE_KEXT_OVERRIDE;
};

#endif
