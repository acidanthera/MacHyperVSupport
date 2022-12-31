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
  HyperVVMBusDevice  *_hvDevice     = nullptr;
  IOWorkLoop         *_workLoop     = nullptr;
  IOTimerEventSource *_timerSource  = nullptr;
  UInt32             _transactionId = 0;
  UInt8              _balloonInflationSendBuffer[PAGE_SIZE];
  OSDictionary       *_pageFrameNumberToMemoryDescriptorMap;
  
  void handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength);
  void handleBalloonInflationRequest(HyperVDynamicMemoryMessageBalloonInflationRequest *request);
  void handleBalloonDeflationRequest(HyperVDynamicMemoryMessageBalloonDeflationRequest *request);
  void handleHotAddRequest(HyperVDynamicMemoryMessageHotAddRequest *request);
  bool setupBalloon();
  bool doProtocolNegotitation(HyperVDynamicMemoryProtocolVersion version, bool isLastAttempt);
  IOReturn sendStatusReport(void*, void*, void*);
  bool inflationBalloon(UInt32 pageCount, bool morePages);
  static void getPagesStatus(UInt64 *availablePages);
  static const OSSymbol *pageFrameNumberToString(UInt64 pfn);
  
public:
  virtual bool start(IOService* provider) APPLE_KEXT_OVERRIDE;
  virtual void stop(IOService* provider) APPLE_KEXT_OVERRIDE;
};
#endif /* HyperVBalloon_hpp */
