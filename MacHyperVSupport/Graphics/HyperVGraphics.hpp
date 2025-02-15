//
//  HyperVGraphics.hpp
//  Hyper-V synthetic graphics driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#ifndef HyperVGraphics_hpp
#define HyperVGraphics_hpp

#include <IOKit/IOService.h>

#include "HyperVVMBusDevice.hpp"
#include "HyperVGraphicsRegs.hpp"

class HyperVGraphics : public IOService {
  OSDeclareDefaultStructors(HyperVGraphics);
  HVDeclareLogFunctionsVMBusChild("gfx");
  typedef IOService super;

private:
  HyperVVMBusDevice *_hvDevice  = nullptr;
  VMBusVersion      _currentGraphicsVersion = { };

  void handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength);
  IOReturn sendGraphicsMessage(HyperVGraphicsMessage *gfxMessage, HyperVGraphicsMessage *gfxMessageResponse = nullptr, UInt32 gfxMessageResponseSize = 0);
  IOReturn negotiateVersion(VMBusVersion version);
  IOReturn connectGraphics();

public:
  //
  // IOService overrides.
  //
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#endif
