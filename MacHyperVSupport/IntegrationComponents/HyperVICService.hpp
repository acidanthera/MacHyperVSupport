//
//  HyperVICService.hpp
//  Hyper-V IC base class
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVICService_hpp
#define HyperVICService_hpp

#include <IOKit/IOService.h>

#include "HyperVVMBusDevice.hpp"
#include "HyperVIC.hpp"

#define kHyperVICBufferSize     (PAGE_SIZE * 4)

class HyperVICService : public IOService {
  OSDeclareDefaultStructors(HyperVICService);
  HVDeclareLogFunctionsVMBusChild("ic");
  typedef IOService super;

protected:
  HyperVVMBusDevice *_hvDevice = nullptr;
  void setICDebug(bool debug) { debugEnabled = debug; }

  virtual void handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) = 0;
  virtual UInt32 txBufferSize() { return kHyperVICBufferSize; };
  virtual UInt32 rxBufferSize() { return kHyperVICBufferSize; };
  bool processNegotiationResponse(VMBusICMessageNegotiate *negMsg, const VMBusVersion *msgVersions,
                                  UInt32 msgVersionsCount, VMBusVersion *msgVersionUsed = nullptr);

public:
  //
  // IOService overrides.
  //
  virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#endif
