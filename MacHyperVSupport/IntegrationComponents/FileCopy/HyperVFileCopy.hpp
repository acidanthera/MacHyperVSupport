//
//  HyperVFileCopy.hpp
//  Hyper-V file copy driver
//
//  Copyright © 2022 flagers. All rights reserved.
//

#ifndef HyperVFileCopy_hpp
#define HyperVFileCopy_hpp

#include "HyperVICService.hpp"
#include "HyperVFileCopyRegs.hpp"
#include "HyperVFileCopyUserClientInternal.hpp"

class HyperVFileCopy : public HyperVICService {
  OSDeclareDefaultStructors(HyperVFileCopy);
  HVDeclareLogFunctionsVMBusChild("fcopy");
  typedef HyperVICService super;

protected:
  void handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) APPLE_KEXT_OVERRIDE;
  UInt32 txBufferSize() APPLE_KEXT_OVERRIDE { return kHyperVFileCopyBufferSize; };
  UInt32 rxBufferSize() APPLE_KEXT_OVERRIDE { return kHyperVFileCopyBufferSize; };

private:
  HyperVFileCopyUserClient *_userClientInstance = nullptr;


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
