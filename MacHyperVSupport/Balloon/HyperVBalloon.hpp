//
//  HyperVBalloon.hpp
//  Hyper-V balloon driver interface
//
//  Copyright © 2022-2023 xdqi. All rights reserved.
//

#ifndef HyperVBalloon_hpp
#define HyperVBalloon_hpp

#include <IOKit/IOService.h>

#include "HyperVVMBusDevice.hpp"
#include "HyperVBalloonRegs.hpp"

class HyperVBalloon : public IOService {
  OSDeclareDefaultStructors(HyperVBalloon);
  HVDeclareLogFunctionsVMBusChild("balloon");
  typedef IOService super;

private:
  HyperVVMBusDevice  *_hvDevice    = nullptr;
  IOWorkLoop         *_workLoop    = nullptr;
  IOTimerEventSource *_timerSource = nullptr;
  UInt32             transactionId = 0;
  
  void handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength);
  bool setupBalloon();
  IOReturn sendStatusReport(void*, void*, void*);
  
public:
  virtual bool start(IOService* provider) APPLE_KEXT_OVERRIDE;
  virtual void stop(IOService* provider) APPLE_KEXT_OVERRIDE;
};
#endif /* HyperVBalloon_hpp */
