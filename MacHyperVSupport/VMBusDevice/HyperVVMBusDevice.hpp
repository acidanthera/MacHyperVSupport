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

typedef struct {
  void            *sendData;
  UInt32          sendDataLength;
  VMBusPacketType sendPacketType;
  UInt64          transactionId;
  bool            responseRequired;
  
  VMBusPacketMultiPageBuffer  *multiPageBuffer;
  UInt32                      multiPageBufferLength;
  
  void            *responseData;
  UInt32          responseDataLength;
  
  bool            ignoreLargePackets;
} HyperVVMBusDeviceRequest;

typedef struct HyperVVMBusDeviceRequestNew {
  HyperVVMBusDeviceRequestNew *next;
  IOLock                    *lock;
  bool                      isSleeping;
  
  UInt64          transactionId;
  void            *responseData;
  UInt32          responseDataLength;
} HyperVVMBusDeviceRequestNew;

class HyperVVMBusDevice : public IOService {
  OSDeclareDefaultStructors(HyperVVMBusDevice);
  
private:
  HyperVVMBusController   *vmbusProvider;
  UInt32                  channelId;
  bool                    channelIsOpen;
  
  IOWorkLoop              *workLoop;
  IOCommandGate           *commandGate;
  bool                    commandLock;
  bool                    commandSleeping;
  IOInterruptEventSource  *interruptSource;
  IOInterruptEventSource  *childInterruptSource;
  
  VMBusRingBuffer         *txBuffer;
  UInt32                  txBufferSize;
  VMBusRingBuffer         *rxBuffer;
  UInt32                  rxBufferSize;
  
  HyperVVMBusDeviceRequestNew   *vmbusRequests = NULL;
  IOLock                        *vmbusRequestsLock;
  UInt64                        vmbusTransId = 0;
  IOLock                        *vmbusTransLock;
  
  bool                    debugPackets = false;

  bool setupInterrupt();
  void teardownInterrupt();
  
  void handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count);
  
  IOReturn nextPacketAvailableGated(VMBusPacketType *type, UInt32 *packetHeaderLength, UInt32 *packetTotalLength);
  IOReturn doRequestGated(HyperVVMBusDeviceRequest *request, void *pageBufferData, UInt32 *pageBufferLength);
  IOReturn readRawPacketGated(void *buffer, UInt32 *bufferLength);
  IOReturn readInbandPacketGated(void *buffer, UInt32 *bufferLength, UInt64 *transactionId);
  IOReturn writeRawPacketGated(void *header, UInt32 *headerLength, void *buffer, UInt32 *bufferLength);
  IOReturn writeInbandPacketGated(void *buffer, UInt32 *bufferLength, bool *responseRequired, UInt64 *transactionId);
  
  UInt32 copyPacketDataFromRingBuffer(UInt32 readIndex, UInt32 readLength, void *data, UInt32 dataLength);
  UInt32 seekPacketDataFromRingBuffer(UInt32 readIndex, UInt32 readLength);
  UInt32 copyPacketDataToRingBuffer(UInt32 writeIndex, void *data, UInt32 length);
  UInt32 zeroPacketDataToRingBuffer(UInt32 writeIndex, UInt32 length);
  
  void addPacketRequest(HyperVVMBusDeviceRequestNew *vmbusRequest);
  void sleepPacketRequest(HyperVVMBusDeviceRequestNew *vmbusRequest);
  
  inline UInt32 getAvailableTxSpace() {
    return (txBuffer->writeIndex >= txBuffer->readIndex) ?
      (txBufferSize - (txBuffer->writeIndex - txBuffer->readIndex)) :
      (txBuffer->readIndex - txBuffer->writeIndex);
  }

public:
  
  //
  // IOService overrides.
  //
  virtual bool attach(IOService *provider) APPLE_KEXT_OVERRIDE;
  virtual void detach(IOService *provider) APPLE_KEXT_OVERRIDE;
  
  //
  // General functions.
  //
  void setDebugMessagePrinting(bool enabled) { debugPackets = enabled; }
  bool openChannel(UInt32 txSize, UInt32 rxSize, OSObject *owner = NULL, IOInterruptEventAction intAction = NULL);
  void closeChannel();
  bool createGpadlBuffer(UInt32 bufferSize, UInt32 *gpadlHandle, void **buffer);

  
  //
  // Messages.
  //
  bool nextPacketAvailable(VMBusPacketType *type, UInt32 *packetHeaderLength, UInt32 *packetTotalLength);
  bool nextInbandPacketAvailable(UInt32 *packetDataLength);
  UInt64 getNextTransId();
  
  IOReturn doRequest(HyperVVMBusDeviceRequest *request);
  IOReturn readRawPacket(void *buffer, UInt32 bufferLength);
  IOReturn readInbandPacket(void *buffer, UInt32 bufferLength, UInt64 *transactionId);
  
  IOReturn writeRawPacket(void *buffer, UInt32 bufferLength);
  IOReturn writeInbandPacket(void *buffer, UInt32 bufferLength, bool responseRequired,
                             void *responseBuffer = NULL, UInt32 responseBufferLength = 0);
  IOReturn writeGPADirectSinglePagePacket(void *buffer, UInt32 bufferLength, bool responseRequired,
                                          VMBusSinglePageBuffer pageBuffers[], UInt32 pageBufferCount,
                                          void *responseBuffer = NULL, UInt32 responseBufferLength = 0);
  
  IOReturn sendMessage(void *message, UInt32 messageLength, VMBusPacketType type, UInt64 transactionId,
                       bool responseRequired = false, void *response = NULL, UInt32 *responseLength = NULL);
  IOReturn sendMessageSinglePageBuffers(void *message, UInt32 messageLength, UInt64 transactionId,
                                        VMBusSinglePageBuffer pageBuffers[], UInt32 pageBufferCount,
                                        bool responseRequired = false, void *response = NULL, UInt32 *responseLength = NULL);
  bool getPendingTransaction(UInt64 transactionId, void **buffer, UInt32 *bufferLength);
  void wakeTransaction(UInt64 transactionId);
  
  
};

#endif
