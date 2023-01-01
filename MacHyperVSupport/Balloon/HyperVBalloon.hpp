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
  HVDeclareLogFunctionsVMBusChild("ball");
  typedef IOService super;

private:
  HyperVVMBusDevice  *_hvDevice                             = nullptr;

  IOWorkLoop         *_workLoop                             = nullptr;
  IOTimerEventSource *_timerSource                          = nullptr;

  UInt32              _transactionId                        = 0;
  void               *_balloonInflationSendBuffer           = nullptr;
  OSDictionary       *_pageFrameNumberToMemoryDescriptorMap = nullptr;

  void handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength);
  void handleBalloonInflationRequest(HyperVDynamicMemoryMessageBalloonInflationRequest *request);
  void handleBalloonDeflationRequest(HyperVDynamicMemoryMessageBalloonDeflationRequest *request);
  void handleHotAddRequest(HyperVDynamicMemoryMessageHotAddRequest *request);
  void handleInformationMessage(HyperVDynamicMemoryMessageInformation *info);
  bool setupBalloon();
  bool doProtocolNegotitation(HyperVDynamicMemoryProtocolVersion version, bool isLastAttempt);
  IOReturn sendStatusReport(void*, void*, void*);
  bool inflateBalloon(UInt32 pageCount, bool morePages);
  static UInt64 getPhysicalMemorySizeInPages();
  static void getPagesStatus(UInt64 *committedPages, UInt64 *usingPages);
  static const OSSymbol *pageFrameNumberToString(UInt64 pfn);
  
public:
  virtual bool start(IOService* provider) APPLE_KEXT_OVERRIDE;
  virtual void stop(IOService* provider) APPLE_KEXT_OVERRIDE;
};
#endif /* HyperVBalloon_hpp */
