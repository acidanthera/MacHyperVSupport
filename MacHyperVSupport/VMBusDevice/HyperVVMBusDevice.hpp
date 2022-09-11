//
//  HyperVVMBusDevice.hpp
//  Hyper-V VMBus device nub
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#ifndef HyperVVMBusDevice_hpp
#define HyperVVMBusDevice_hpp

#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>

#include "HyperVVMBus.hpp"
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
  
public:
  //
  // Packet action handlers.
  //
  typedef void (*PacketReadyAction)(void *target, UInt8 *packet, UInt32 packetLength);
  typedef bool (*WakePacketAction)(void *target, UInt8 *packet, UInt32 packetLength);
  
#if DEBUG
  typedef void (*TimerDebugAction)(void *target);
#endif
  
private:
  //
  // VMBus channel information.
  //
  HyperVVMBus   *_vmbusProvider = nullptr;
  uuid_string_t _typeId;
  UInt32        _channelId      = 0;
  uuid_t        _instanceId;
  bool          _channelIsOpen = false;
  
  //
  // Work loop and related.
  //
  IOWorkLoop             *_workLoop           = nullptr;
  IOCommandGate          *_commandGate        = nullptr;
  bool                   _commandLock         = false;
  IOInterruptEventSource *_interruptSource    = nullptr;
  OSObject               *_packetActionTarget = nullptr;
  PacketReadyAction     _packetReadyAction    = nullptr;
  WakePacketAction      _wakePacketAction     = nullptr;
  
  //
  // Ring buffers for channel.
  //
  VMBusRingBuffer *_txBuffer    = nullptr;
  UInt32          _txBufferSize = 0;
  VMBusRingBuffer *_rxBuffer    = nullptr;
  UInt32          _rxBufferSize = 0;
  
  UInt8           *_receivePacketBuffer      = nullptr;
  UInt32          _receivePacketBufferLength = 0;
  
#if DEBUG
  //
  // Timer event source for debug prints.
  //
  IOWorkLoop          *_debugTimerWorkLoop = nullptr;
  IOTimerEventSource  *_debugTimerSource   = nullptr;
  OSObject            *_timerDebugTarget   = nullptr;
  TimerDebugAction    _timerDebugAction    = nullptr;
  UInt64              _numInterrupts       = 0;
  UInt64              _numPackets          = 0;
  
  void handleDebugPrintTimer(IOTimerEventSource *sender);
#endif
  
public:
  UInt64                  txBufferWriteCount;
  UInt64                  rxBufferReadCount;
  
  HyperVVMBusDeviceRequest      *vmbusRequests = NULL;
  IOLock                        *vmbusRequestsLock;
  UInt64                        vmbusTransId = 1; // Some devices have issues with 0 as a transaction ID.
  UInt64                        _maxAutoTransId = UINT64_MAX;
  IOLock                        *vmbusTransLock;
  
  HyperVVMBusDeviceRequest      threadZeroRequest;

  
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
  
  //
  // Ring buffer.
  //
  inline UInt32 getRingReadIndex(VMBusRingBuffer *ringBuffer) {
    return ringBuffer->readIndex;
  }
  inline UInt32 getRingWriteIndex(VMBusRingBuffer *ringBuffer) {
    return ringBuffer->writeIndex;
  }
  inline void getAvailableRingSpace(VMBusRingBuffer *ringBuffer, UInt32 ringBufferSize, UInt32 *readBytes, UInt32 *writeBytes) {
    __sync_synchronize();
    UInt32 writeIndex = getRingWriteIndex(ringBuffer);
    UInt32 readIndex  = getRingReadIndex(ringBuffer);
    
    *writeBytes = (writeIndex >= readIndex) ? (ringBufferSize - (writeIndex - readIndex)) : (readIndex - writeIndex);
    *readBytes = ringBufferSize - *writeBytes;
  }
  
private:
  void handleInterrupt(IOInterruptEventSource *sender, int count);
  IOReturn openVMBusChannelGated(UInt32 *txBufferSize, UInt32 *rxBufferSize);

public:
  //
  // IOService overrides.
  //
  bool attach(IOService *provider) APPLE_KEXT_OVERRIDE;
  void detach(IOService *provider) APPLE_KEXT_OVERRIDE;
  bool matchPropertyTable(OSDictionary *table, SInt32 *score) APPLE_KEXT_OVERRIDE;
  IOWorkLoop* getWorkLoop() const APPLE_KEXT_OVERRIDE;
  
  //
  // Channel management.
  //
  IOReturn installPacketActions(OSObject *target, PacketReadyAction packetReadyAction, WakePacketAction wakePacketAction,
                                UInt32 initialResponseBufferLength, bool registerInterrupt = true);
  IOReturn openVMBusChannel(UInt32 txSize, UInt32 rxSize, UInt64 maxAutoTransId = UINT64_MAX);
  
  //
  // Ring buffer.
  //
  inline UInt32 getTxReadIndex() {
    return getRingReadIndex(_txBuffer);
  }
  inline UInt32 getTxWriteIndex() {
    return getRingWriteIndex(_txBuffer);
  }
  inline void getAvailableTxSpace(UInt32 *readBytes, UInt32 *writeBytes) {
    getAvailableRingSpace(_txBuffer, _txBufferSize, readBytes, writeBytes);
  }
  inline UInt32 getRxReadIndex() {
    return getRingReadIndex(_rxBuffer);
  }
  inline UInt32 getRxWriteIndex() {
    return getRingWriteIndex(_rxBuffer);
  }
  inline void getAvailableRxSpace(UInt32 *readBytes, UInt32 *writeBytes) {
    getAvailableRingSpace(_rxBuffer, _rxBufferSize, readBytes, writeBytes);
  }
  
  //
  // Misc.
  //
  void setDebugMessagePrinting(bool enabled) { debugPackets = enabled; }
  
  bool openChannel(UInt32 txSize, UInt32 rxSize, UInt64 maxAutoTransId = UINT64_MAX);
  void closeChannel();
  bool createGpadlBuffer(UInt32 bufferSize, UInt32 *gpadlHandle, void **buffer);
  bool allocateDmaBuffer(HyperVDMABuffer *dmaBuf, size_t size);
  void freeDmaBuffer(HyperVDMABuffer *dmaBuf);
  UInt32 getChannelId() { return _channelId; }
  uuid_t* getInstanceId() { return &_instanceId; }
  
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
  
  //
  // Timer debug printing.
  //
#if DEBUG
  void enableTimerDebugPrints();
  void installTimerDebugPrintAction(OSObject *target, TimerDebugAction action);
#endif
};

#endif
