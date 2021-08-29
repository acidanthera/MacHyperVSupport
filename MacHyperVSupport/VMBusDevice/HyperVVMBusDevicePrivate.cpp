//
//  HyperVVMBusDevicePrivate.cpp
//  Hyper-V VMBus device nub
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVVMBusDevice.hpp"
#include "HyperVVMBusDeviceInternal.hpp"

bool HyperVVMBusDevice::setupInterrupt() {
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
    
    interruptSource = IOInterruptEventSource::interruptEventSource(this,
                                                                   OSMemberFunctionCast(IOInterruptEventAction, this, &HyperVVMBusDevice::handleInterrupt),
                                                                   this, 0);
    if (interruptSource == NULL) {
      workLoop->removeEventSource(commandGate);
      break;
    }
    interruptSource->enable();
    workLoop->addEventSource(interruptSource);
    initialized = true;
    
  } while (false);
  
  if (!initialized) {
    OSSafeReleaseNULL(interruptSource);
    OSSafeReleaseNULL(commandGate);
    OSSafeReleaseNULL(workLoop);
    
    interruptSource = NULL;
    commandGate = NULL;
    workLoop = NULL;
  }
  
  return initialized;
}

void HyperVVMBusDevice::teardownInterrupt() {
  if (childInterruptSource != NULL) {
    childInterruptSource->disable();
    workLoop->removeEventSource(childInterruptSource);
    childInterruptSource = NULL;
  }
  
  if (interruptSource == NULL) {
    return;
  }
  
  interruptSource->disable();
  workLoop->removeEventSource(interruptSource);
  workLoop->removeEventSource(commandGate);
  
  interruptSource->release();
  commandGate->release();
  workLoop->release();
  
  interruptSource = NULL;
  commandGate = NULL;
  workLoop = NULL;
}

void HyperVVMBusDevice::handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count) {
  //
  // If there is a command in progress, handle that.
  //
  if (commandSleeping) {
    commandSleeping = false;
    commandGate->commandWakeup(&commandLock);

  } else {
    if (childInterruptSource != NULL) {
      childInterruptSource->interruptOccurred(0, 0, 0);
    }
  }
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

IOReturn HyperVVMBusDevice::doRequestGated(HyperVVMBusDeviceRequest *request, void *pageBufferData, UInt32 *pageBufferLength) {
  //
  // If there is data to send, send it first.
  //
  if (request->sendData != NULL && request->sendDataLength != 0) {
    //
    // Create packet header.
    // Sizes are represented as 8 byte units.
    //
    VMBusPacketHeader *pktHeader;
    VMBusPacketHeader pktHeaderStack;
    UInt32 pktHeaderLength;
    
    //
    // In-band packets have a fixed length.
    // GPA direct packets and other buffer packets have a variable length.
    //
    if (request->sendPacketType == kVMBusPacketTypeDataUsingGPADirect) {
      if (request->multiPageBuffer != NULL) {
             pktHeader       = (VMBusPacketHeader*) request->multiPageBuffer;
             pktHeaderLength = request->multiPageBufferLength;

             request->multiPageBuffer->reserved   = 0;
             request->multiPageBuffer->rangeCount = 1;
      } else {
        return kIOReturnBadArgument;
      }
    } else {
      pktHeader       = &pktHeaderStack;
      pktHeaderLength = sizeof (pktHeaderStack);
    }
    pktHeader->type           = request->sendPacketType;
    pktHeader->flags          = request->responseRequired ? kVMBusPacketResponseRequired : 0;
    pktHeader->transactionId  = request->transactionId;
    
    UInt32 pktTotalLength         = pktHeaderLength + request->sendDataLength;
    UInt32 pktTotalLengthAligned  = HV_PACKETALIGN(pktTotalLength);
    
    UInt32 writeIndexOld      = txBuffer->writeIndex;
    UInt32 writeIndexNew      = writeIndexOld;
    UInt64 writeIndexShifted  = ((UInt64)writeIndexOld) << 32;
    
    //
    // Ensure there is space for the packet.
    //
    // We cannot end up with read index == write index after the write, as that would indicate an empty buffer.
    //
    if (getAvailableTxSpace() <= pktTotalLengthAligned) {
      SYSLOG("Packet is too large for buffer (TXR: %X, TXW: %X)", txBuffer->readIndex, txBuffer->writeIndex);
      return kIOReturnNoResources;
    }
    
    pktHeader->headerLength = pktHeaderLength >> kVMBusPacketSizeShift;
    pktHeader->totalLength = pktTotalLength >> kVMBusPacketSizeShift;
    
    //
    // Copy header, data, padding, and index to this packet.
    //
    // DBGLOG("Packet type %u, flags %u, header length %u, total length %u, pad %u",
    //        pktHeader->type, pktHeader->flags, pktHeaderLength, pktTotalLength, pktTotalLengthAligned - pktTotalLength);
    writeIndexNew = copyPacketDataToRingBuffer(writeIndexNew, pktHeader, pktHeaderLength);
    writeIndexNew = copyPacketDataToRingBuffer(writeIndexNew, request->sendData, request->sendDataLength);
    writeIndexNew = zeroPacketDataToRingBuffer(writeIndexNew, pktTotalLengthAligned - pktTotalLength);
    writeIndexNew = copyPacketDataToRingBuffer(writeIndexNew, &writeIndexShifted, sizeof (writeIndexShifted));
    // DBGLOG("TX read index %X, new TX write index %X", txBuffer->readIndex, writeIndexNew);
    
    //
    // Update write index and notify host if needed.
    //
    //DBGLOG("TX imask %X rx imask %X, channel ID %u", txBuffer->interruptMask, rxBuffer->interruptMask, channelId);
    txBuffer->writeIndex = writeIndexNew;
    if (txBuffer->interruptMask == 0) {
      vmbusProvider->signalVMBusChannel(channelId);
    }
    //DBGLOG("TX read index %X, new TX write index %X", txBuffer->readIndex, txBuffer->writeIndex);
    
    //
    // Wait for a response if a response is expected.
    //
    if (request->responseData != NULL) {
      commandSleeping = true;
      commandGate->commandSleep(&commandLock);
      DBGLOG("Waking up for response");
    }
  }
  
  //
  // Process response.
  //
  if (request->responseData != NULL) {
    //
    // No data to read.
    //
    if (rxBuffer->readIndex == rxBuffer->writeIndex) {
      request->responseDataLength = 0;
      return kIOReturnSuccess;
    }
    
    //
    // Read packet header.
    //
    UInt32 readIndexNew = rxBuffer->readIndex;
    VMBusPacketHeader pktHeader;
    
    readIndexNew = copyPacketDataFromRingBuffer(readIndexNew, sizeof (VMBusPacketHeader), &pktHeader, sizeof (VMBusPacketHeader));
    
    UInt32 packetHeaderLength = pktHeader.headerLength << kVMBusPacketSizeShift;
    UInt32 packetTotalLength = pktHeader.totalLength << kVMBusPacketSizeShift;
    UInt32 packetDataLength = packetTotalLength - packetHeaderLength;
    //DBGLOG("Packet type %u, flags %u, trans %u, header length %u, total length %u", pktHeader.type, pktHeader.flags, pktHeader.transactionId, packetHeaderLength, packetTotalLength);
    //DBGLOG("RX read index %X, RX write index %X", rxBuffer->readIndex, rxBuffer->writeIndex);
    
    UInt32 actualReadLength = packetDataLength;
    if (request->responseDataLength < packetDataLength) {
      DBGLOG("Buffer too small, %u < %u", request->responseDataLength, packetDataLength);
      
      if (!request->ignoreLargePackets) {
        request->responseDataLength = packetDataLength;
        return kIOReturnMessageTooLarge;
      } else {
        actualReadLength = request->responseDataLength;
      }
    }

    readIndexNew = copyPacketDataFromRingBuffer(readIndexNew, packetDataLength, request->responseData, actualReadLength);
    request->responseDataLength = actualReadLength;
    
    UInt64 readIndexShifted;
    readIndexNew = copyPacketDataFromRingBuffer(readIndexNew, sizeof (readIndexShifted), &readIndexShifted, sizeof (readIndexShifted));
    
    rxBuffer->readIndex = readIndexNew;
    // DBGLOG("New RX read index %X, RX write index %X", rxBuffer->readIndex, rxBuffer->writeIndex);
    
    //
    // If there is more data to be read, and we returned from an interrupt, raise child interrupt.
    //
    if (request->sendData != NULL && rxBuffer->readIndex != rxBuffer->writeIndex) {
      if (childInterruptSource != NULL) {
        childInterruptSource->interruptOccurred(0, 0, 0);
      }
    }
  }
  
  return kIOReturnSuccess;
}

IOReturn HyperVVMBusDevice::readRawPacketGated(void *buffer, UInt32 *bufferLength) {
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
  
  if (*bufferLength < packetTotalLength) {
    MSGDBG("RAW buffer too small, %u < %u", *bufferLength, packetTotalLength);
    return kIOReturnNoResources;
  }
  
  //
  // Read raw packet.
  //
  UInt32 readIndexNew = rxBuffer->readIndex;
  readIndexNew = copyPacketDataFromRingBuffer(readIndexNew, packetTotalLength, buffer, packetTotalLength);
  
  UInt64 readIndexShifted;
  readIndexNew = copyPacketDataFromRingBuffer(readIndexNew, sizeof (readIndexShifted), &readIndexShifted, sizeof (readIndexShifted));
  
  rxBuffer->readIndex = readIndexNew;
  MSGDBG("RAW new RX read index %X, RX write index %X", rxBuffer->readIndex, rxBuffer->writeIndex);
  return kIOReturnSuccess;
}

IOReturn HyperVVMBusDevice::readInbandPacketGated(void *buffer, UInt32 *bufferLength, UInt64 *transactionId) {
  //
  // No data to read.
  //
  if (rxBuffer->readIndex == rxBuffer->writeIndex) {
    return kIOReturnNotFound;
  }
  
  //
  // Read packet header and verify it's inband.
  //
  UInt32 readIndexNew = rxBuffer->readIndex;
  VMBusPacketHeader pktHeader;
  readIndexNew = copyPacketDataFromRingBuffer(readIndexNew, sizeof (VMBusPacketHeader), &pktHeader, sizeof (VMBusPacketHeader));
  if (pktHeader.type != kVMBusPacketTypeDataInband) {
    MSGDBG("INBAND attempted to read non-inband packet");
    return kIOReturnUnsupported;
  }
  
  UInt32 packetHeaderLength = pktHeader.headerLength << kVMBusPacketSizeShift;
  UInt32 packetTotalLength = pktHeader.totalLength << kVMBusPacketSizeShift;
  UInt32 packetDataLength = packetTotalLength - packetHeaderLength;
  MSGDBG("INBAND packet type %u, flags %u, trans %u, header length %u, total length %u", pktHeader.type, pktHeader.flags, pktHeader.transactionId, packetHeaderLength, packetTotalLength);
  MSGDBG("INBAND old RX read index %X, RX write index %X", rxBuffer->readIndex, rxBuffer->writeIndex);
  
  if (*bufferLength < packetDataLength) {
    MSGDBG("INBAND buffer too small, %u < %u", *bufferLength, packetDataLength);
    return kIOReturnNoResources;
  }
  
  if (transactionId != NULL) {
    *transactionId = pktHeader.transactionId;
  }
  
  //
  // Read inband packet data.
  //
  readIndexNew = copyPacketDataFromRingBuffer(readIndexNew, packetDataLength, buffer, packetDataLength);
  
  UInt64 readIndexShifted;
  readIndexNew = copyPacketDataFromRingBuffer(readIndexNew, sizeof (readIndexShifted), &readIndexShifted, sizeof (readIndexShifted));
  
  rxBuffer->readIndex = readIndexNew;
  MSGDBG("INBAND new RX read index %X, RX write index %X", rxBuffer->readIndex, rxBuffer->writeIndex);
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
  MSGDBG("RAW TX imask %X, channel ID %u", txBuffer->interruptMask, channelId);
  txBuffer->writeIndex = writeIndexNew;
  if (txBuffer->interruptMask == 0) {
    vmbusProvider->signalVMBusChannel(channelId);
  }
  MSGDBG("RAW TX read index %X, new TX write index %X", txBuffer->readIndex, txBuffer->writeIndex);
  return kIOReturnSuccess;
}

IOReturn HyperVVMBusDevice::writeInbandPacketGated(void *buffer, UInt32 *bufferLength, bool *responseRequired, UInt64 *transactionId) {
  //
  // Create inband packet header.
  // Sizes are represented as 8 byte units.
  //
  VMBusPacketHeader pktHeader;
  pktHeader.type                = kVMBusPacketTypeDataInband;
  pktHeader.flags               = *responseRequired ? kVMBusPacketResponseRequired : 0;
  pktHeader.transactionId       = *transactionId;
  
  UInt32 pktHeaderLength        = sizeof (pktHeader);
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
    SYSLOG("INBAND packet is too large for buffer (TXR: %X, TXW: %X)", txBuffer->readIndex, txBuffer->writeIndex);
    return kIOReturnNoResources;
  }
  
  pktHeader.headerLength  = pktHeaderLength >> kVMBusPacketSizeShift;
  pktHeader.totalLength   = pktTotalLength >> kVMBusPacketSizeShift;
  
  //
  // Copy header, data, padding, and index to this packet.
  //
  MSGDBG("INBAND packet type %u, flags %u, trans %llu, header length %u, total length %u, pad %u",
         pktHeader.type, pktHeader.flags, pktHeader.transactionId,
         pktHeaderLength, pktTotalLength, pktTotalLengthAligned - pktTotalLength);
  writeIndexNew = copyPacketDataToRingBuffer(writeIndexNew, &pktHeader, pktHeaderLength);
  writeIndexNew = copyPacketDataToRingBuffer(writeIndexNew, buffer, *bufferLength);
  writeIndexNew = zeroPacketDataToRingBuffer(writeIndexNew, pktTotalLengthAligned - pktTotalLength);
  writeIndexNew = copyPacketDataToRingBuffer(writeIndexNew, &writeIndexShifted, sizeof (writeIndexShifted));
  MSGDBG("INBAND TX read index %X, new TX write index %X", txBuffer->readIndex, writeIndexNew);
  
  //
  // Update write index and notify Hyper-V if needed.
  //
  MSGDBG("INBAND TX imask %X, channel ID %u", txBuffer->interruptMask, channelId);
  txBuffer->writeIndex = writeIndexNew;
  if (txBuffer->interruptMask == 0) {
    vmbusProvider->signalVMBusChannel(channelId);
  }
  MSGDBG("INBAND TX read index %X, new TX write index %X", txBuffer->readIndex, txBuffer->writeIndex);
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

void HyperVVMBusDevice::addPacketRequest(HyperVVMBusDeviceRequestNew *vmbusRequest) {
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

void HyperVVMBusDevice::sleepPacketRequest(HyperVVMBusDeviceRequestNew *vmbusRequest) {
  MSGDBG("Sleeping transaction %u", vmbusRequest->transactionId);
  IOLockLock(vmbusRequest->lock);
  while (vmbusRequest->isSleeping) {
    IOLockSleep(vmbusRequest->lock, &vmbusRequest->isSleeping, THREAD_INTERRUPTIBLE);
  }
  IOLockUnlock(vmbusRequest->lock);
  MSGDBG("Woken transaction %u after sleep", vmbusRequest->transactionId);
}
