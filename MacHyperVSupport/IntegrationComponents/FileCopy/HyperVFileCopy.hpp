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
#include <sys/syslimits.h>
#include <sys/utfconv.h>

class HyperVFileCopy : public HyperVICService {
  OSDeclareDefaultStructors(HyperVFileCopy);
  HVDeclareLogFunctionsVMBusChild("fcopy");
  typedef HyperVICService super;

private:
  bool firstFileCopyReceived = false;

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
