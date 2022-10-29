//
//  HyperVVMBus.hpp
//  Hyper-V VMBus controller
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#ifndef HyperVVMBus_hpp
#define HyperVVMBus_hpp

#include <IOKit/IOCommandGate.h>
#include <IOKit/IOEventSource.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOService.h>

#include "HyperV.hpp"
#include "VMBus.hpp"

#include "HyperVController.hpp"

#define kVMBusArrayInitialChildrenCount        10

class HyperVVMBusDevice;
class VMBusInterruptProcessor;


//
// Channel status.
//
typedef enum {
  kVMBusChannelStatusNotPresent = 0,
  kVMBusChannelStatusClosed,
  kVMBusChannelStatusOpen
} VMBusChannelStatus;

//
// Used for per-channel tracking of buffers and stats.
//
typedef struct {
  VMBusChannelStatus              status;
  uuid_string_t                   typeGuidString;
  uuid_t                          instanceId;
  VMBusChannelMessageChannelOffer offerMessage;
  
  //
  // Unique GPADL handle for this channel.
  //
  UInt32                          dataGpadlHandle;
  HyperVDMABuffer                 dataBuffer;
  HyperVDMABuffer                 eventBuffer;
  
  //
  // Index into ring buffer where receive pages begin.
  //
  UInt16                          rxPageIndex;
  
  VMBusRingBuffer                 *txBuffer;
  VMBusRingBuffer                 *rxBuffer;
  
  //
  // I/O Kit nub for VMBus device.
  //
  HyperVVMBusDevice               *deviceNub;
} VMBusChannel;

class HyperVVMBus : public IOService {
  OSDeclareDefaultStructors(HyperVVMBus);
  HVDeclareLogFunctions("vmbus");
  typedef IOService super;
  
  friend class HyperVVMBusDevice;
  friend class VMBusInterruptProcessor;
  
  
private:
  //
  // Parent controller.
  //
  HyperVController *hvController = nullptr;
  
  //
  // Interrupt event sources for VMBus management packets.
  // This may arrive on any CPU, so one is used for each CPU.
  //
  UInt32                  vmbusInterruptProcsCount = 0;
  VMBusInterruptProcessor **vmbusInterruptProcs    = nullptr;
  
  //
  // VMBus event flags.
  //
  bool                useLegacyEventFlags = false;
  HyperVDMABuffer     vmbusEventFlags;
  HyperVEventFlags    *vmbusRxEventFlags;
  HyperVEventFlags    *vmbusTxEventFlags;
  HyperVDMABuffer     _vmbusMnf1 = { };
  HyperVDMABuffer     _vmbusMnf2 = { };
  
  //
  // Flag used for waiting for incoming message response.
  // 0 = disable.
  //
  UInt8               _vmbusWaitForMessageType = 0;
  UInt32              _vmbusWaitMessageCpu = 0;
  HyperVMessage       _vmbusWaitMessage = { };
  
  IOCommandGate           *_cmdGate = nullptr;
  bool                    _cmdShouldWake = false;
  bool                    _cmdGateEvent = false;
  
  
  UInt32                  _nextGpadlHandle      = kHyperVGpadlNullHandle;
  UInt32                  _vmbusVersion         = 0;
  UInt16                  _vmbusMsgConnectionId = 0;
public:
  VMBusChannel            _vmbusChannels[kVMBusMaxChannels] = { };
  
  
private:
  
  //
  // Interrupt handling for VMBus management packets.
  //
  void handleDirectInterrupt(OSObject *target, void *refCon, IOService *nub, int source);
  bool allocateInterruptEventSources();
  void freeInterruptEventSources();
  
  void processIncomingVMBusMessage(UInt32 cpu);
  //
  // VMBus functions.
  //
  bool allocateVMBusBuffers();
  bool sendVMBusMessage(VMBusChannelMessage *message, VMBusChannelMessageType responseType = kVMBusChannelMessageTypeInvalid, VMBusChannelMessage *response = NULL);
  bool sendVMBusMessageWithSize(VMBusChannelMessage *message, UInt32 messageSize, VMBusChannelMessageType responseType = kVMBusChannelMessageTypeInvalid, VMBusChannelMessage *response = NULL);
  IOReturn sendVMBusMessageGated(VMBusChannelMessage *message, UInt32 *messageSize, VMBusChannelMessageType *responseType, VMBusChannelMessage *response);
  bool connectVMBus();
  bool negotiateVMBus(UInt32 version);
  bool scanVMBus();
  bool addVMBusDevice(VMBusChannelMessageChannelOffer *offerMessage);
  void removeVMBusDevice(VMBusChannelMessageChannelRescindOffer *rescindOfferMessage);
  bool registerVMBusDevice(VMBusChannel *channel);
  void cleanupVMBusDevice(VMBusChannel *channel);
  
  //
  // VMBus channel management.
  //
  

  void freeVMBusChannel(UInt32 channelId);
  
public:
  //
  // IOService overrides.
  //
  bool attach(IOService *provider) APPLE_KEXT_OVERRIDE;
  
  //
  // Misc functions.
  //
  inline HyperVController *getHvController() { return hvController; }
  
  //
  // VMBus channel management.
  //
  VMBusChannelStatus getVMBusChannelStatus(UInt32 channelId);
  IOReturn openVMBusChannel(UInt32 channelId, UInt32 txBufferSize, VMBusRingBuffer **txBuffer, UInt32 rxBufferSize, VMBusRingBuffer **rxBuffer);
  IOReturn closeVMBusChannel(UInt32 channelId);
  IOReturn initVMBusChannelGPADL(UInt32 channelId, HyperVDMABuffer *dmaBuffer, UInt32 *gpadlHandle);
  IOReturn freeVMBusChannelGPADL(UInt32 channelId, UInt32 gpadlHandle);
  void signalVMBusChannel(UInt32 channelId);
};

#endif
