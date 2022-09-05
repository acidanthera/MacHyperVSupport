//
//  HyperVVMBusDevicePrivate.cpp
//  Hyper-V VMBus device nub
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVVMBusDevice.hpp"

bool HyperVVMBusDevice::setupCommandGate() {
  bool initialized = false;
  
  do {    

    
    commandGate = IOCommandGate::commandGate(this);
    if (commandGate == NULL) {
      break;
    }
    workLoop->addEventSource(commandGate);
    initialized = true;
    
  } while (false);
  
  if (!initialized) {
    OSSafeReleaseNULL(commandGate);
    OSSafeReleaseNULL(workLoop);

    commandGate = NULL;
    workLoop = NULL;
  }
  
  return initialized;
}

void HyperVVMBusDevice::teardownCommandGate() {
  workLoop->removeEventSource(commandGate);

  commandGate->release();
 // workLoop->release();

  commandGate = NULL;
 // workLoop = NULL;
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

  IOReturn status = commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBusDevice::writeRawPacketGated),
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
  if (rxBuffer->readIndex == rxBuffer->writeIndex) {
    return kIOReturnNotFound;
  }
  
  VMBusPacketHeader pktHeader;
  copyPacketDataFromRingBuffer(rxBuffer->readIndex, sizeof (VMBusPacketHeader), &pktHeader, sizeof (VMBusPacketHeader));
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
  if (rxBuffer->readIndex == rxBuffer->writeIndex) {
    return kIOReturnNotFound;
  }
  
  //
  // Read packet header.
  //
  VMBusPacketHeader pktHeader;
  copyPacketDataFromRingBuffer(rxBuffer->readIndex, sizeof (VMBusPacketHeader), &pktHeader, sizeof (VMBusPacketHeader));

  UInt32 packetTotalLength = pktHeader.totalLength << kVMBusPacketSizeShift;
  HVMSGLOG("RAW packet type %u, flags %u, trans %llu, header length %u, total length %u", pktHeader.type, pktHeader.flags,
           pktHeader.transactionId, pktHeader.headerLength << kVMBusPacketSizeShift, packetTotalLength);
  HVMSGLOG("RAW old RX read index 0x%X, RX write index 0x%X", rxBuffer->readIndex, rxBuffer->writeIndex);
  
  UInt32 packetDataLength = headerLength != NULL ? packetTotalLength - *headerLength : packetTotalLength;
  if (*bufferLength < packetDataLength) {
    HVMSGLOG("RAW buffer too small, %u < %u", *bufferLength, packetDataLength);
    return kIOReturnNoResources;
  }
  
  //
  // Read raw packet.
  //
  UInt32 readIndexNew = rxBuffer->readIndex;
  if (headerLength != NULL) {
    readIndexNew = copyPacketDataFromRingBuffer(readIndexNew, *headerLength, header, *headerLength);
  }
  readIndexNew = copyPacketDataFromRingBuffer(readIndexNew, packetDataLength, buffer, packetDataLength);
  
  UInt64 readIndexShifted;
  readIndexNew = copyPacketDataFromRingBuffer(readIndexNew, sizeof (readIndexShifted), &readIndexShifted, sizeof (readIndexShifted));
  
  rxBuffer->readIndex = readIndexNew;
  HVMSGLOG("RAW new RX read index 0x%X, RX new write index 0x%X", rxBuffer->readIndex, rxBuffer->writeIndex);
  return kIOReturnSuccess;
}

IOReturn HyperVVMBusDevice::writeRawPacketGated(void *header, UInt32 *headerLength, void *buffer, UInt32 *bufferLength) {
  UInt32 pktHeaderLength        = headerLength != NULL ? *headerLength : 0;
  UInt32 pktTotalLength         = pktHeaderLength + *bufferLength;
  UInt32 pktTotalLengthAligned  = HV_PACKETALIGN(pktTotalLength);
  
  UInt32 writeIndexOld          = txBuffer->writeIndex;
  UInt32 writeIndexNew          = writeIndexOld;
  UInt64 writeIndexShifted      = ((UInt64)writeIndexOld) << 32;
  
  //
  // Ensure there is space for the packet.
  //
  // We cannot end up with read index == write index after the write, as that would indicate an empty buffer.
  //
  if (getAvailableTxSpace() <= pktTotalLengthAligned) {
    HVSYSLOG("RAW packet is too large for buffer (TXR: 0x%X, TXW: 0x%X)", txBuffer->readIndex, txBuffer->writeIndex);
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
  HVMSGLOG("RAW TX read index 0x%X, old TX write index 0x%X", txBuffer->readIndex, txBuffer->writeIndex);
  
  //
  // Update write index and notify Hyper-V if needed.
  //
  HVMSGLOG("RAW TX imask 0x%X, RX imask 0x%X, channel ID %u", txBuffer->interruptMask, rxBuffer->interruptMask, channelId);
  txBuffer->writeIndex = writeIndexNew;
  if (txBuffer->interruptMask == 0) {
    vmbusProvider->signalVMBusChannel(channelId);
  }
  HVMSGLOG("RAW TX read index 0x%X, new TX write index 0x%X", txBuffer->readIndex, txBuffer->writeIndex);
  return kIOReturnSuccess;
}

UInt32 HyperVVMBusDevice::copyPacketDataFromRingBuffer(UInt32 readIndex, UInt32 readLength, void *data, UInt32 dataLength) {
  //
  // Check for wraparound.
  //
  if (dataLength > rxBufferSize - readIndex) {
    UInt32 fragmentLength = rxBufferSize - readIndex;
    HVMSGLOG("RX wraparound by %u bytes", fragmentLength);
    memcpy(data, &rxBuffer->buffer[readIndex], fragmentLength);
    memcpy((UInt8*) data + fragmentLength, rxBuffer->buffer, dataLength - fragmentLength);
  } else {
    memcpy(data, &rxBuffer->buffer[readIndex], dataLength);
  }
  
  return (readIndex + readLength) % rxBufferSize;
}

UInt32 HyperVVMBusDevice::seekPacketDataFromRingBuffer(UInt32 readIndex, UInt32 readLength) {
  return (readIndex + readLength) % rxBufferSize;
}

UInt32 HyperVVMBusDevice::copyPacketDataToRingBuffer(UInt32 writeIndex, void *data, UInt32 length) {
  //
  // Check for wraparound.
  //
  if (length > txBufferSize - writeIndex) {
    UInt32 fragmentLength = txBufferSize - writeIndex;
    HVMSGLOG("TX wraparound by %u bytes", fragmentLength);
    memcpy(&txBuffer->buffer[writeIndex], data, fragmentLength);
    memcpy(txBuffer->buffer, (UInt8*) data + fragmentLength, length - fragmentLength);
  } else {
    memcpy(&txBuffer->buffer[writeIndex], data, length);
  }
  
  return (writeIndex + length) % txBufferSize;
}

UInt32 HyperVVMBusDevice::zeroPacketDataToRingBuffer(UInt32 writeIndex, UInt32 length) {
  //
  // Check for wraparound.
  //
  if (length > txBufferSize - writeIndex) {
    UInt32 fragmentLength = txBufferSize - writeIndex;
    HVMSGLOG("TX wraparound by %u bytes", fragmentLength);
    memset(&txBuffer->buffer[writeIndex], 0, fragmentLength);
    memset(txBuffer->buffer, 0, length - fragmentLength);
  } else {
    memset(&txBuffer->buffer[writeIndex], 0, length);
  }
  
  return (writeIndex + length) % txBufferSize;
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
