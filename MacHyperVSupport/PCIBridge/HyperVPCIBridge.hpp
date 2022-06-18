//
//  HyperVPCIBridge.hpp
//  Hyper-V PCI passthrough device support
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVPCIBridge_hpp
#define HyperVPCIBridge_hpp

#include <IOKit/pci/IOPCIBridge.h>

#include "HyperVVMBusDevice.hpp"
#include "HyperVPCIBridgeRegs.hpp"

#define super IOService

#define HVSYSLOG(str, ...) HVSYSLOG_PRINT("HyperVPCIBridge", true, hvDevice->getChannelId(), str, ## __VA_ARGS__)
#define HVDBGLOG(str, ...) HVDBGLOG_PRINT("HyperVPCIBridge", true, hvDevice->getChannelId(), str, ## __VA_ARGS__)

class HyperVPCIBridge : public IOService {
  OSDeclareDefaultStructors(HyperVPCIBridge);
  
private:
  //
  // Parent VMBus device.
  //
  HyperVVMBusDevice       *hvDevice;
  IOInterruptEventSource  *interruptSource;
  bool                    debugEnabled = false;
  
  HyperVPCIBridgeProtocolVersion  currentPciVersion;
  
  UInt32                        pciFunctionCount = 0;
  HyperVPCIFunctionDescription  *pciFunctions = nullptr;
  
  void handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count);
  void handleIncomingPCIMessage(HyperVPCIBridgeIncomingMessageHeader *pciMsgHeader, UInt32 msgSize);
  
  bool negotiateProtocolVersion();
  bool queryBusRelations();
  
public:
  //
  // IOService overrides.
  //
  virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#endif
