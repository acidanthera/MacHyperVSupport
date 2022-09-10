//
//  HyperVVMBusDevicePrivate.cpp
//  Hyper-V VMBus device nub
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVVMBusDevice.hpp"

void HyperVVMBusDevice::handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count) {
  IOReturn status;
  UInt32 readBytes;
  UInt32 writeBytes;
  
  VMBusPacketHeader *pktHeader;
  UInt32 headerLength;
  UInt32 packetLength;
  void *responseBuffer;
  UInt32 responseLength;
  
  //
  // Flush RX buffer of all packets.
  //
  // Setting the interrupt mask will prevent Hyper-V from sending
  // any more interrupts until it is cleared.
  //
  // During each cycle, invoke previously passed in handler function from client driver.
  //
  do {
    _rxBuffer->interruptMask = 1;
    __sync_synchronize();
    
    while (true) {
      status = readRawPacket(_receivePacketBuffer, _receivePacketBufferLength);
      if (status == kIOReturnNotReady) {
        //
        // No more packets in RX buffer.
        //
        break;
      } else if (status == kIOReturnNoSpace) {
        //
        // Received packet is larger than current buffer, reallocate and try again.
        //
        IOFree(_receivePacketBuffer, _receivePacketBufferLength);
        _receivePacketBufferLength *= 2;
        _receivePacketBuffer = (UInt8*) IOMalloc(_receivePacketBufferLength);
        HVDBGLOG("Incoming packet too big for buffer, reallocated to %u bytes", _receivePacketBufferLength);
        continue;
      }
      
      pktHeader = (VMBusPacketHeader*) _receivePacketBuffer;
      headerLength = HV_GET_VMBUS_PACKETSIZE(pktHeader->headerLength);
      packetLength = HV_GET_VMBUS_PACKETSIZE(pktHeader->totalLength);
      
      //
      // If a wake packet handler was specified, determine if this is a packet type that should be checked and woken up.
      //
      if (_wakePacketAction != nullptr && (*_wakePacketAction)(_packetActionTarget, _receivePacketBuffer, packetLength)) {
        if (getPendingTransaction(pktHeader->transactionId, &responseBuffer, &responseLength)) {
          memcpy(responseBuffer, &_receivePacketBuffer[headerLength], responseLength);
          wakeTransaction(pktHeader->transactionId);
          continue;
        }
      }
      
      //
      // Invoke handler for child to process packet.
      //
      (*_packetReadyAction)(_packetActionTarget, _receivePacketBuffer, packetLength);
    }
    
    _rxBuffer->interruptMask = 0;
    __sync_synchronize();
    
    getAvailableRxSpace(&readBytes, &writeBytes);
  } while (readBytes != 0);
}

IOReturn HyperVVMBusDevice::openVMBusChannelGated(UInt32 *txSize, UInt32 *rxSize) {
  IOReturn status;
  
  _txBufferSize = *txSize;
  _rxBufferSize = *rxSize;
  
  status = _vmbusProvider->openVMBusChannel(_channelId, _txBufferSize, &_txBuffer, _rxBufferSize, &_rxBuffer);
  if (status == kIOReturnSuccess) {
    _channelIsOpen = true;
  }
  return status;
}

IOReturn HyperVVMBusDevice::writePacketInternal(void *buffer, UInt32 bufferLength, VMBusPacketType packetType, UInt64 transactionId,
                                                bool responseRequired, void *responseBuffer, UInt32 responseBufferLength) {
  //
  // Disallow 0 for a transaction ID.
  //
  if (transactionId == 0) {
    return kIOReturnBadArgument;
  }
  
  //
  // Create inband packet header.
  // Sizes are represented as 8 byte units.
  //
  VMBusPacketHeader pktHeader;
  pktHeader.type                = packetType;
  pktHeader.flags               = responseRequired ? kVMBusPacketResponseRequired : 0;
  pktHeader.transactionId       = transactionId;
  
  UInt32 pktHeaderLength        = sizeof (pktHeader);
  UInt32 pktTotalLength         = pktHeaderLength + bufferLength;
  
  pktHeader.headerLength  = pktHeaderLength >> kVMBusPacketSizeShift;
  pktHeader.totalLength   = HV_PACKETALIGN(pktTotalLength) >> kVMBusPacketSizeShift;
  
  //
  // Copy header, data, padding, and index to this packet.
  //
  HVMSGLOG("Packet type %u, flags %u, trans %llu, header length %u, total length %u",
           pktHeader.type, pktHeader.flags, pktHeader.transactionId,
           pktHeaderLength, pktTotalLength);
  
  HyperVVMBusDeviceRequest req;
  if (responseBuffer != NULL) {
    req.isSleeping = true;
    req.lock = IOLockAlloc();
    req.responseData = responseBuffer;
    req.responseDataLength = responseBufferLength;
    req.transactionId = transactionId;
    addPacketRequest(&req);
  }

  IOReturn status = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBusDevice::writeRawPacketGated),
                                           &pktHeader, &pktHeaderLength, buffer, &bufferLength);
  
  if (responseBuffer != NULL) {
    if (status == kIOReturnSuccess) {
      sleepPacketRequest(&req);
    } else {
      wakeTransaction(transactionId);
    }
    IOLockFree(req.lock);
  }
  return status;
}

IOReturn HyperVVMBusDevice::nextPacketAvailableGated(VMBusPacketType *type, UInt32 *packetHeaderLength, UInt32 *packetTotalLength) {
  //
  // No data to read.
  //
  __sync_synchronize();
  if (_rxBuffer->readIndex == _rxBuffer->writeIndex) {
    return kIOReturnNotFound;
  }
  
  VMBusPacketHeader pktHeader;
  copyPacketDataFromRingBuffer(_rxBuffer->readIndex, sizeof (VMBusPacketHeader), &pktHeader, sizeof (VMBusPacketHeader));
  HVMSGLOG("Packet type %u, header size %u, total size %u",
           pktHeader.type, pktHeader.headerLength << kVMBusPacketSizeShift, pktHeader.totalLength << kVMBusPacketSizeShift);

  if (type != NULL) {
    *type = pktHeader.type;
  }
  if (packetHeaderLength != NULL) {
    *packetHeaderLength = pktHeader.headerLength << kVMBusPacketSizeShift;
  }
  if (packetTotalLength != NULL) {
    *packetTotalLength = pktHeader.totalLength << kVMBusPacketSizeShift;
  }
  
  return kIOReturnSuccess;
}

IOReturn HyperVVMBusDevice::readRawPacketGated(void *header, UInt32 *headerLength, void *buffer, UInt32 *bufferLength) {
  //
  // No data to read.
  //
  if (_rxBuffer->readIndex == _rxBuffer->writeIndex) {
    return kIOReturnNotReady;
  }
  
  //
  // Read packet header.
  //
  VMBusPacketHeader pktHeader;
  copyPacketDataFromRingBuffer(_rxBuffer->readIndex, sizeof (VMBusPacketHeader), &pktHeader, sizeof (VMBusPacketHeader));

  UInt32 packetTotalLength = pktHeader.totalLength << kVMBusPacketSizeShift;
  HVMSGLOG("RAW packet type %u, flags %u, trans %llu, header length %u, total length %u", pktHeader.type, pktHeader.flags,
           pktHeader.transactionId, pktHeader.headerLength << kVMBusPacketSizeShift, packetTotalLength);
  HVMSGLOG("RAW old RX read index 0x%X, RX write index 0x%X", _rxBuffer->readIndex, _rxBuffer->writeIndex);
  
  UInt32 packetDataLength = headerLength != NULL ? packetTotalLength - *headerLength : packetTotalLength;
  if (*bufferLength < packetDataLength) {
    HVMSGLOG("RAW buffer too small, %u < %u", *bufferLength, packetDataLength);
    return kIOReturnNoSpace;
  }
  
  //
  // Read raw packet.
  //
  UInt32 readIndexNew = _rxBuffer->readIndex;
  if (headerLength != NULL) {
    readIndexNew = copyPacketDataFromRingBuffer(readIndexNew, *headerLength, header, *headerLength);
  }
  readIndexNew = copyPacketDataFromRingBuffer(readIndexNew, packetDataLength, buffer, packetDataLength);
  
  UInt64 readIndexShifted;
  readIndexNew = copyPacketDataFromRingBuffer(readIndexNew, sizeof (readIndexShifted), &readIndexShifted, sizeof (readIndexShifted));
  __sync_synchronize();
  
  _rxBuffer->readIndex = readIndexNew;
  HVMSGLOG("RAW new RX read index 0x%X, RX new write index 0x%X", _rxBuffer->readIndex, _rxBuffer->writeIndex);
  rxBufferReadCount++;
  return kIOReturnSuccess;
}

IOReturn HyperVVMBusDevice::writeRawPacketGated(void *header, UInt32 *headerLength, void *buffer, UInt32 *bufferLength) {
  UInt32 pktHeaderLength        = headerLength != NULL ? *headerLength : 0;
  UInt32 pktTotalLength         = pktHeaderLength + *bufferLength;
  UInt32 pktTotalLengthAligned  = HV_PACKETALIGN(pktTotalLength);
  
  UInt32 writeIndexOld          = _txBuffer->writeIndex;
  UInt32 writeIndexNew          = writeIndexOld;
  UInt64 writeIndexShifted      = ((UInt64)writeIndexOld) << 32;
  
  UInt32 readBytes;
  UInt32 writeBytes;
  
  //
  // Ensure there is space for the packet.
  //
  // We cannot end up with read index == write index after the write, as that would indicate an empty buffer.
  //
  getAvailableTxSpace(&readBytes, &writeBytes);
  if (writeBytes <= pktTotalLengthAligned) {
    HVSYSLOG("Packet is too large for buffer (%u bytes remaining)", writeBytes);
    return kIOReturnNoResources;
  }
  
  //
  // Copy header, data, padding, and index to this packet.
  //
  HVMSGLOG("RAW packet header length %u, total length %u, pad %u", pktHeaderLength, pktTotalLength, pktTotalLengthAligned - pktTotalLength);
  if (header != NULL && headerLength != NULL) {
    writeIndexNew = copyPacketDataToRingBuffer(writeIndexNew, header, *headerLength);
  }
  writeIndexNew = copyPacketDataToRingBuffer(writeIndexNew, buffer, *bufferLength);
  writeIndexNew = zeroPacketDataToRingBuffer(writeIndexNew, pktTotalLengthAligned - pktTotalLength);
  writeIndexNew = copyPacketDataToRingBuffer(writeIndexNew, &writeIndexShifted, sizeof (writeIndexShifted));
  HVMSGLOG("RAW TX read index 0x%X, old TX write index 0x%X", _txBuffer->readIndex, _txBuffer->writeIndex);
  __sync_synchronize();
  
  //
  // Update write index and notify Hyper-V if needed.
  //
  HVMSGLOG("RAW TX imask 0x%X, RX imask 0x%X, channel ID %u", _txBuffer->interruptMask, _rxBuffer->interruptMask, _channelId);
  _txBuffer->writeIndex = writeIndexNew;
  if (_txBuffer->interruptMask == 0) {
    _txBuffer->guestToHostInterruptCount++;
    _vmbusProvider->signalVMBusChannel(_channelId);
  }
  HVMSGLOG("RAW TX read index 0x%X, new TX write index 0x%X", _txBuffer->readIndex, _txBuffer->writeIndex);
  txBufferWriteCount++;
  return kIOReturnSuccess;
}

UInt32 HyperVVMBusDevice::copyPacketDataFromRingBuffer(UInt32 readIndex, UInt32 readLength, void *data, UInt32 dataLength) {
  //
  // Check for wraparound.
  //
  if (dataLength > _rxBufferSize - readIndex) {
    UInt32 fragmentLength = _rxBufferSize - readIndex;
    HVMSGLOG("RX wraparound by %u bytes", fragmentLength);
    memcpy(data, &_rxBuffer->buffer[readIndex], fragmentLength);
    memcpy((UInt8*) data + fragmentLength, _rxBuffer->buffer, dataLength - fragmentLength);
  } else {
    memcpy(data, &_rxBuffer->buffer[readIndex], dataLength);
  }
  
  return (readIndex + readLength) % _rxBufferSize;
}

UInt32 HyperVVMBusDevice::seekPacketDataFromRingBuffer(UInt32 readIndex, UInt32 readLength) {
  return (readIndex + readLength) % _rxBufferSize;
}

UInt32 HyperVVMBusDevice::copyPacketDataToRingBuffer(UInt32 writeIndex, void *data, UInt32 length) {
  //
  // Check for wraparound.
  //
  if (length > _txBufferSize - writeIndex) {
    UInt32 fragmentLength = _txBufferSize - writeIndex;
    HVMSGLOG("TX wraparound by %u bytes", fragmentLength);
    memcpy(&_txBuffer->buffer[writeIndex], data, fragmentLength);
    memcpy(_txBuffer->buffer, (UInt8*) data + fragmentLength, length - fragmentLength);
  } else {
    memcpy(&_txBuffer->buffer[writeIndex], data, length);
  }
  
  return (writeIndex + length) % _txBufferSize;
}

UInt32 HyperVVMBusDevice::zeroPacketDataToRingBuffer(UInt32 writeIndex, UInt32 length) {
  //
  // Check for wraparound.
  //
  if (length > _txBufferSize - writeIndex) {
    UInt32 fragmentLength = _txBufferSize - writeIndex;
    HVMSGLOG("TX wraparound by %u bytes", fragmentLength);
    memset(&_txBuffer->buffer[writeIndex], 0, fragmentLength);
    memset(_txBuffer->buffer, 0, length - fragmentLength);
  } else {
    memset(&_txBuffer->buffer[writeIndex], 0, length);
  }
  
  return (writeIndex + length) % _txBufferSize;
}

void HyperVVMBusDevice::addPacketRequest(HyperVVMBusDeviceRequest *vmbusRequest) {
  IOLockLock(vmbusRequestsLock);
  if (vmbusRequests == NULL) {
    vmbusRequests = vmbusRequest;
    vmbusRequests->next = NULL;
  } else {
    vmbusRequest->next = vmbusRequests;
    vmbusRequests = vmbusRequest;
  }
  IOLockUnlock(vmbusRequestsLock);
}

void HyperVVMBusDevice::sleepPacketRequest(HyperVVMBusDeviceRequest *vmbusRequest) {
  HVMSGLOG("Sleeping transaction %u", vmbusRequest->transactionId);
  IOLockLock(vmbusRequest->lock);
  while (vmbusRequest->isSleeping) {
    IOLockSleep(vmbusRequest->lock, &vmbusRequest->isSleeping, THREAD_INTERRUPTIBLE);
  }
  IOLockUnlock(vmbusRequest->lock);
  HVMSGLOG("Woken transaction %u after sleep", vmbusRequest->transactionId);
}

void HyperVVMBusDevice::prepareSleepThread() {
  //
  // Sleep on transaction 0.
  // Used by clients for disconnected response sleeping.
  //
  threadZeroRequest.isSleeping = true;
  threadZeroRequest.transactionId = 0;
  addPacketRequest(&threadZeroRequest);
}
