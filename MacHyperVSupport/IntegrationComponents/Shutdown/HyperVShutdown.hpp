//
//  HyperVShutdown.hpp
//  Hyper-V guest shutdown driver
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#ifndef HyperVShutdown_hpp
#define HyperVShutdown_hpp

#include "HyperVICService.hpp"
#include "HyperVShutdownRegs.hpp"

class HyperVShutdown : public HyperVICService {
  OSDeclareDefaultStructors(HyperVShutdown);
  HVDeclareLogFunctionsVMBusChild("shut");
  typedef HyperVICService super;

private:
  bool handleShutdown(VMBusICMessageShutdownData *shutdownData);
  bool performShutdown(VMBusICMessageShutdownData *shutdownData, bool doShutdown);

protected:
  void handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) APPLE_KEXT_OVERRIDE;

public:
  //
  // IOService overrides.
  //
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#endif
