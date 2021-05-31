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

  bool setupInterrupt();
  void teardownInterrupt();
  
  void handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count);
  
  IOReturn doRequestGated(HyperVVMBusDeviceRequest *request);
  
  UInt32 copyPacketDataFromRingBuffer(UInt32 readIndex, UInt32 readLength, void *data, UInt32 dataLength);
  UInt32 copyPacketDataToRingBuffer(UInt32 writeIndex, void *data, UInt32 length);
  UInt32 zeroPacketDataToRingBuffer(UInt32 writeIndex, UInt32 length);
  
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
  bool openChannel(UInt32 txSize, UInt32 rxSize, OSObject *owner = NULL, IOInterruptEventAction intAction = NULL);
  void closeChannel();

  IOReturn doRequest(HyperVVMBusDeviceRequest *request);
};

#endif
