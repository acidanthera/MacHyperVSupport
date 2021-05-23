//
//  HyperVVMBus.hpp
//  MacHyperVServices
//
//  Created by John Davis on 5/2/21.
//

#ifndef _HV_VMBUS_H_
#define _HV_VMBUS_H_

#include <i386/cpuid.h>
#include <i386/proc_reg.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOInterruptController.h>
#include <IOKit/IODMACommand.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLocks.h>

#include "Registers.h"
#include "VMBus.hpp"
#include "VMBusRegisters.h"
#include "VMBusDriver.hpp"

#include <IOKit/IOCommandGate.h>

#include "SynICProcessor.h"

#include "HyperVVMBusDevice.hpp"

#include "HyperV.hpp"

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
  
  void                *hypercallPage;
  IOMemoryDescriptor  *hypercallDesc;
  
  HyperVCPUData       cpuData;
  
  HyperVDMABuffer     vmbusEventFlags;
  UInt8               *vmbusRxEventFlags;
  UInt8               *vmbusTxEventFlags;
  
  HyperVDMABuffer     vmbusMnf1;
  HyperVDMABuffer     vmbusMnf2;
  
  HyperVDMABuffer     vmbusMsgBuffer;
  //
  // Flag used for waiting for incoming message response.
  // 0 = disable.
  //
  UInt8               vmbusWaitForMessageType;
  UInt32              vmbusWaitMessageCpu;
  
  IOWorkLoop              *workloop;
  IOCommandGate           *cmdGate;
  bool                    cmdShouldWake = false;
  bool                    cmdGateEvent = false;
  
  
  UInt32                  nextGpadlHandle;
  IOLock                  *nextGpadlHandleLock;
  VMBusChannel            vmbusChannels[kHyperVMaxChannels];
  UInt32                  vmbusChannelHighest;
  
  const OSSymbol          *interruptControllerName;
  
  //
  // Misc functions.
  //
 // void logPrint(const char *func, const char *format, ...);
  bool identifyHyperV();
  bool allocateDmaBuffer(HyperVDMABuffer *dmaBuf, size_t size);
  void freeDmaBuffer(HyperVDMABuffer *dmaBuf);

  
  //
  // Hypercalls
  //
  bool initHypercalls();
  void destroyHypercalls();
  void freeHypercallPage();
  UInt64 executeHypercallMem(UInt64 value, UInt64 inPhysAddr, UInt64 outPhysAddr);
  
  UInt64 hypercallPostMessage(UInt64 msgAddr);
  UInt64 hypercallSignalEvent(UInt64 addr);
  
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
  void completeVMBusMessage(UInt32 cpu);
  bool connectVMBus();
  bool scanVMBus();
  bool addVMBusChannelInfo(VMBusChannelMessageChannelOffer *offerMessage);
  OSDictionary *getVMBusDevicePropertyDictionary(VMBusChannel *channel);
  bool registerVMBusDevice(VMBusChannel *channel);
  
  //
  // Private VMBus channel management.
  //
  bool configureVMBusChannelGpadl(VMBusChannel *channel);
  void destroyVMBusChannelGpadl(VMBusChannel *channel);
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
  
  //
  // IOInterruptController overrides.
  //
  virtual void initVector(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector);
  
  

};

#endif
