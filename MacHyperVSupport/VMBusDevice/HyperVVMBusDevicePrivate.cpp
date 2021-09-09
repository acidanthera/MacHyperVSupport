//
//  HyperVVMBusDevicePrivate.cpp
//  Hyper-V VMBus device nub
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVVMBusDevice.hpp"
#include "HyperVVMBusDeviceInternal.hpp"

bool HyperVVMBusDevice::setupCommandGate() {
  bool initialized = false;
  
  do {    
    workLoop = IOWorkLoop::workLoop();
    if (workLoop == NULL) {
      break;
    }
    
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
  workLoop->release();

  commandGate = NULL;
  workLoop = NULL;
}


IOReturn HyperVVMBusDevice::writePacketInternal(void *buffer, UInt32 bufferLength, VMBusPacketType packetType, UInt64 transactionId,
                                                bool responseRequired, void *responseBuffer, UInt32 responseBufferLength) {
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
  pktHeader.totalLength   = pktTotalLength >> kVMBusPacketSizeShift;
  
  //
  // Copy header, data, padding, and index to this packet.
  //
  MSGDBG("Packet type %u, flags %u, trans %llu, header length %u, total length %u",
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
  MSGDBG("Packet type %X, header size %X, total size %X",
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
  MSGDBG("RAW packet type %u, flags %u, trans %llu, header length %u, total length %u", pktHeader.type, pktHeader.flags,
         pktHeader.transactionId, pktHeader.headerLength << kVMBusPacketSizeShift, packetTotalLength);
  MSGDBG("RAW old RX read index %X, RX write index %X", rxBuffer->readIndex, rxBuffer->writeIndex);
  
  UInt32 packetDataLength = headerLength != NULL ? packetTotalLength - *headerLength : packetTotalLength;
  if (*bufferLength < packetDataLength) {
    MSGDBG("RAW buffer too small, %u < %u", *bufferLength, packetDataLength);
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
  MSGDBG("RAW new RX read index %X, RX write index %X", rxBuffer->readIndex, rxBuffer->writeIndex);
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
    SYSLOG("RAW packet is too large for buffer (TXR: %X, TXW: %X)", txBuffer->readIndex, txBuffer->writeIndex);
    return kIOReturnNoResources;
  }
  
  //
  // Copy header, data, padding, and index to this packet.
  //
  MSGDBG("RAW packet header length %u, total length %u, pad %u", pktHeaderLength, pktTotalLength, pktTotalLengthAligned - pktTotalLength);
  if (header != NULL && headerLength != NULL) {
    writeIndexNew = copyPacketDataToRingBuffer(writeIndexNew, header, *headerLength);
  }
  writeIndexNew = copyPacketDataToRingBuffer(writeIndexNew, buffer, *bufferLength);
  writeIndexNew = zeroPacketDataToRingBuffer(writeIndexNew, pktTotalLengthAligned - pktTotalLength);
  writeIndexNew = copyPacketDataToRingBuffer(writeIndexNew, &writeIndexShifted, sizeof (writeIndexShifted));
  MSGDBG("RAW TX read index %X, new TX write index %X", txBuffer->readIndex, writeIndexNew);
  
  //
  // Update write index and notify Hyper-V if needed.
  //
  MSGDBG("RAW TX imask %X, RX imask %X, channel ID %u", txBuffer->interruptMask, rxBuffer->interruptMask, channelId);
  txBuffer->writeIndex = writeIndexNew;
  if (txBuffer->interruptMask == 0) {
    vmbusProvider->signalVMBusChannel(channelId);
  }
  MSGDBG("RAW TX read index %X, new TX write index %X", txBuffer->readIndex, txBuffer->writeIndex);
  return kIOReturnSuccess;
}

UInt32 HyperVVMBusDevice::copyPacketDataFromRingBuffer(UInt32 readIndex, UInt32 readLength, void *data, UInt32 dataLength) {
  //
  // Check for wraparound.
  //
  if (dataLength > rxBufferSize - readIndex) {
    UInt32 fragmentLength = rxBufferSize - readIndex;
    MSGDBG("RX wraparound by %u bytes", fragmentLength);
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
    MSGDBG("TX wraparound by %u bytes", fragmentLength);
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
    MSGDBG("TX wraparound by %u bytes", fragmentLength);
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
  MSGDBG("Sleeping transaction %u", vmbusRequest->transactionId);
  IOLockLock(vmbusRequest->lock);
  while (vmbusRequest->isSleeping) {
    IOLockSleep(vmbusRequest->lock, &vmbusRequest->isSleeping, THREAD_INTERRUPTIBLE);
  }
  IOLockUnlock(vmbusRequest->lock);
  MSGDBG("Woken transaction %u after sleep", vmbusRequest->transactionId);
}
