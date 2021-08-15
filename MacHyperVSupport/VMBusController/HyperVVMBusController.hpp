//
//  HyperVVMBus.hpp
//  Hyper-V VMBus controller
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVVMBusController_hpp
#define HyperVVMBusController_hpp

extern "C" {
#include <i386/cpuid.h>
#include <i386/proc_reg.h>
#include <i386/pmCPU.h>
}

#include <IOKit/IOLib.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOService.h>
#include <IOKit/IOInterruptController.h>
#include <IOKit/IODMACommand.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLocks.h>

#include "HyperV.hpp"
#include "VMBus.hpp"
#include "VMBusDriver.hpp"
#include "SynICProcessor.hpp"

inline void
guid_unparse(const uuid_t uu, uuid_string_t out)
{
  snprintf(out,
    sizeof(uuid_string_t),
    "%02x%02x%02x%02x-"
    "%02x%02x-"
    "%02x%02x-"
    "%02x%02x-"
    "%02x%02x%02x%02x%02x%02x",
    uu[3], uu[2], uu[1], uu[0],
    uu[5], uu[4],
    uu[7], uu[6],
    uu[8], uu[9],
    uu[10], uu[11], uu[12], uu[13], uu[14], uu[15]);
}

typedef struct {
  UInt64                  interruptCounter;
  UInt64                  virtualCPUID;
  
  SynICProcessor          *synProc;
  
  HyperVDMABuffer         messageDma;
  HyperVDMABuffer         eventFlagsDma;
  
  HyperVMessage           *messages;
  HyperVEventFlags        *eventFlags;
  
  HyperVDMABuffer         postMessageDma;
} HyperVPerCPUData;

typedef struct {
  UInt32            perCPUDataCount;
  HyperVPerCPUData  *perCPUData;
  
  UInt32            interruptVector;
  bool              supportsHvVpIndex;
} HyperVCPUData;

class HyperVVMBusController : public IOInterruptController {
  OSDeclareDefaultStructors(HyperVVMBusController);
  
private:
  UInt32 hvMaxLeaf;
  UInt32 hvFeatures;
  UInt32 hvPmFeatures;
  UInt32 hvFeatures3;
  UInt16 hvMajorVersion;
  UInt32 hvRecommends;
  
  pmCallBacks_t pmCallbacks;
  IOSimpleLock *preemptionLock;
  
  void                *hypercallPage;
  IOMemoryDescriptor  *hypercallDesc;
  
  HyperVCPUData       cpuData;
  
  HyperVDMABuffer     vmbusEventFlags;
  UInt8               *vmbusRxEventFlags;
  UInt8               *vmbusTxEventFlags;
  
  HyperVDMABuffer     vmbusMnf1;
  HyperVDMABuffer     vmbusMnf2;

  //
  // Flag used for waiting for incoming message response.
  // 0 = disable.
  //
  UInt8               vmbusWaitForMessageType;
  UInt32              vmbusWaitMessageCpu;
  HyperVMessage       vmbusWaitMessage;
  
  IOWorkLoop              *workloop;
  IOCommandGate           *cmdGate;
  bool                    cmdShouldWake = false;
  bool                    cmdGateEvent = false;
  
  
  UInt32                  nextGpadlHandle;
  IOSimpleLock            *nextGpadlHandleLock;
  VMBusChannel            vmbusChannels[kHyperVMaxChannels];
  UInt32                  vmbusChannelHighest;
  
  const OSSymbol          *interruptControllerName;
  
  //
  // Misc functions.
  //
  bool identifyHyperV();
  bool allocateDmaBuffer(HyperVDMABuffer *dmaBuf, size_t size);
  void freeDmaBuffer(HyperVDMABuffer *dmaBuf);

  //
  // Hypercalls
  //
  bool initHypercalls();
  void destroyHypercalls();
  void freeHypercallPage();  
  UInt32 hypercallPostMessage(UInt32 connectionId, HyperVMessageType messageType, void *data, UInt32 size);
  bool hypercallSignalEvent(UInt32 connectionId);
  
  //
  // SynIC and interrupts.
  //
  bool allocateSynICBuffers();
  void freeSynICBuffers();
  bool initSynIC();
  void destroySynIC();
  void sendSynICEOM(UInt32 cpu);
  void handleSynICInterrupt(OSObject *target, void *refCon, IOService *nub, int source);
  
  
  
  
  //
  // VMBus functions.
  //
  bool allocateVMBusBuffers();
  bool sendVMBusMessage(VMBusChannelMessage *message, VMBusChannelMessageType responseType = kVMBusChannelMessageTypeInvalid, VMBusChannelMessage *response = NULL);
  bool sendVMBusMessageWithSize(VMBusChannelMessage *message, UInt32 messageSize, VMBusChannelMessageType responseType = kVMBusChannelMessageTypeInvalid, VMBusChannelMessage *response = NULL);
  IOReturn sendVMBusMessageGated(VMBusChannelMessage *message, UInt32 *messageSize, VMBusChannelMessageType *responseType, VMBusChannelMessage *response);
  bool connectVMBus();
  bool scanVMBus();
  bool addVMBusDevice(VMBusChannelMessageChannelOffer *offerMessage);
  void removeVMBusDevice(VMBusChannelMessageChannelRescindOffer *rescindOfferMessage);
  bool registerVMBusDevice(VMBusChannel *channel);
  void cleanupVMBusDevice(VMBusChannel *channel);
  
  //
  // Private VMBus channel management.
  //
  bool configureVMBusChannelGpadl(VMBusChannel *channel, HyperVDMABuffer *buffer, UInt32 *gpadlHandle);
  bool configureVMBusChannel(VMBusChannel *channel);
  
public:
  //
  // IOService functions.
  //
  virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  
  //
  // External SynIC process function.
  //
  void processIncomingVMBusMessage(UInt32 cpu);
  IOWorkLoop *getSynICWorkLoop();
  
  //
  // Public VMBus channel management.
  //
  bool initVMBusChannel(UInt32 channelId, UInt32 txBufferSize, VMBusRingBuffer **txBuffer, UInt32 rxBufferSize, VMBusRingBuffer **rxBuffer);
  bool openVMBusChannel(UInt32 channelId);
  void signalVMBusChannel(UInt32 channelId);
  void closeVMBusChannel(UInt32 channelId);
  void freeVMBusChannel(UInt32 channelId);
  
  bool initVMBusChannelGpadl(UInt32 channelId, UInt32 bufferSize, UInt32 *gpadlHandle, void **buffer);
};

#endif
