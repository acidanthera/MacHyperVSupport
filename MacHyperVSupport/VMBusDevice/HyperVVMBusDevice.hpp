//
//  HyperVVMBusDevice.hpp
//  Hyper-V VMBus device nub
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVVMBusDevice_hpp
#define HyperVVMBusDevice_hpp

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOInterruptEventSource.h>

#include "HyperVVMBusController.hpp"
#include "HyperV.hpp"
#include "VMBus.hpp"

#define kHyperVVMBusDeviceChannelTypeKey      "HVType"
#define kHyperVVMBusDeviceChannelInstanceKey  "HVInstance"
#define kHyperVVMBusDeviceChannelIDKey        "HVChannel"

typedef struct HyperVVMBusDeviceRequest {
  HyperVVMBusDeviceRequest  *next;
  IOLock                    *lock;
  bool                      isSleeping;
  
  UInt64                    transactionId;
  void                      *responseData;
  UInt32                    responseDataLength;
} HyperVVMBusDeviceRequest;

class HyperVVMBusDevice : public IOService {
  OSDeclareDefaultStructors(HyperVVMBusDevice);
  HVDeclareLogFunctionsVMBusDeviceNub("vmbusdev");
  typedef IOService super;
  
private:
  HyperVVMBusController   *vmbusProvider;
  uuid_string_t           typeId;
  UInt32                  channelId;
  uuid_t                  instanceId;
  bool                    channelIsOpen = false;
  
  IOWorkLoop              *workLoop;
  IOCommandGate           *commandGate;
  bool                    commandLock;
  
  VMBusRingBuffer         *txBuffer;
  UInt32                  txBufferSize;
  VMBusRingBuffer         *rxBuffer;
  UInt32                  rxBufferSize;
  
  HyperVVMBusDeviceRequest      *vmbusRequests = NULL;
  IOLock                        *vmbusRequestsLock;
  UInt64                        vmbusTransId = 1; // Some devices have issues with 0 as a transaction ID.
  UInt64                        vmbusMaxAutoTransId = UINT64_MAX;
  IOLock                        *vmbusTransLock;
  
  HyperVVMBusDeviceRequest      threadZeroRequest;

  bool setupCommandGate();
  void teardownCommandGate();

  
  IOReturn writePacketInternal(void *buffer, UInt32 bufferLength, VMBusPacketType packetType, UInt64 transactionId,
                               bool responseRequired, void *responseBuffer, UInt32 responseBufferLength);
  
  IOReturn nextPacketAvailableGated(VMBusPacketType *type, UInt32 *packetHeaderLength, UInt32 *packetTotalLength);
  IOReturn readRawPacketGated(void *header, UInt32 *headerLength, void *buffer, UInt32 *bufferLength);
  IOReturn writeRawPacketGated(void *header, UInt32 *headerLength, void *buffer, UInt32 *bufferLength);
  IOReturn writeInbandPacketGated(void *buffer, UInt32 *bufferLength, bool *responseRequired, UInt64 *transactionId);
  
  UInt32 copyPacketDataFromRingBuffer(UInt32 readIndex, UInt32 readLength, void *data, UInt32 dataLength);
  UInt32 seekPacketDataFromRingBuffer(UInt32 readIndex, UInt32 readLength);
  UInt32 copyPacketDataToRingBuffer(UInt32 writeIndex, void *data, UInt32 length);
  UInt32 zeroPacketDataToRingBuffer(UInt32 writeIndex, UInt32 length);
  
  void addPacketRequest(HyperVVMBusDeviceRequest *vmbusRequest);
  void sleepPacketRequest(HyperVVMBusDeviceRequest *vmbusRequest);
  void prepareSleepThread();
  
  inline UInt32 getAvailableTxSpace() {
    return (txBuffer->writeIndex >= txBuffer->readIndex) ?
      (txBufferSize - (txBuffer->writeIndex - txBuffer->readIndex)) :
      (txBuffer->readIndex - txBuffer->writeIndex);
  }

public:
  //
  // IOService overrides.
  //
  bool attach(IOService *provider) APPLE_KEXT_OVERRIDE;
  void detach(IOService *provider) APPLE_KEXT_OVERRIDE;
  bool matchPropertyTable(OSDictionary *table, SInt32 *score) APPLE_KEXT_OVERRIDE;
  
  //
  // General functions.
  //
  void setDebugMessagePrinting(bool enabled) { debugPackets = enabled; }
  bool openChannel(UInt32 txSize, UInt32 rxSize, UInt64 maxAutoTransId = UINT64_MAX);
  void closeChannel();
  bool createGpadlBuffer(UInt32 bufferSize, UInt32 *gpadlHandle, void **buffer);
  bool allocateDmaBuffer(HyperVDMABuffer *dmaBuf, size_t size);
  void freeDmaBuffer(HyperVDMABuffer *dmaBuf);
  UInt32 getChannelId() { return channelId; }
  uuid_t* getInstanceId() { return &instanceId; }
  
  //
  // Messages.
  //
  bool nextPacketAvailable(VMBusPacketType *type, UInt32 *packetHeaderLength, UInt32 *packetTotalLength);
  bool nextInbandPacketAvailable(UInt32 *packetDataLength);
  UInt64 getNextTransId();
  
  IOReturn readRawPacket(void *buffer, UInt32 bufferLength);
  IOReturn readInbandCompletionPacket(void *buffer, UInt32 bufferLength, UInt64 *transactionId = NULL);
  
  IOReturn writeRawPacket(void *buffer, UInt32 bufferLength);
  IOReturn writeInbandPacket(void *buffer, UInt32 bufferLength, bool responseRequired,
                             void *responseBuffer = NULL, UInt32 responseBufferLength = 0);
  IOReturn writeInbandPacketWithTransactionId(void *buffer, UInt32 bufferLength, UInt64 transactionId, bool responseRequired,
                                              void *responseBuffer = NULL, UInt32 responseBufferLength = 0);
  IOReturn writeGPADirectSinglePagePacket(void *buffer, UInt32 bufferLength, bool responseRequired,
                                          VMBusSinglePageBuffer pageBuffers[], UInt32 pageBufferCount,
                                          void *responseBuffer = NULL, UInt32 responseBufferLength = 0);
  IOReturn writeGPADirectMultiPagePacket(void *buffer, UInt32 bufferLength, bool responseRequired,
                                         VMBusPacketMultiPageBuffer *pagePacket, UInt32 pagePacketLength,
                                         void *responseBuffer = NULL, UInt32 responseBufferLength = 0);
  IOReturn writeCompletionPacketWithTransactionId(void *buffer, UInt32 bufferLength, UInt64 transactionId, bool responseRequired);
  
  
  bool getPendingTransaction(UInt64 transactionId, void **buffer, UInt32 *bufferLength);
  void wakeTransaction(UInt64 transactionId);
  void doSleepThread();
  
  
};

#endif
