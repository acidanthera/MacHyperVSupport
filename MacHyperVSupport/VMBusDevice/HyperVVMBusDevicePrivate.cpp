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
    
    return false;
  }
  
  return true;
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

IOReturn HyperVVMBusDevice::doRequestGated(HyperVVMBusDeviceRequest *request) {
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
        pktHeader = (VMBusPacketHeader*) request->multiPageBuffer;
        pktHeaderLength = request->multiPageBufferLength;
        
        request->multiPageBuffer->reserved = 0;
        request->multiPageBuffer->rangeCount = 1;
      } else {
        return kIOReturnBadArgument;
      }
      
    } else {
      pktHeader = &pktHeaderStack;
      pktHeaderLength = sizeof (pktHeaderStack);
    }
    pktHeader->type = request->sendPacketType;
    pktHeader->flags = request->responseRequired ? kVMBusPacketResponseRequired : 0;
    pktHeader->transactionId = 0xFFE55;
    
    UInt32 pktTotalLength = pktHeaderLength + request->sendDataLength;
    UInt32 pktTotalLengthAligned = HV_PACKETALIGN(pktTotalLength);
    
    pktHeader->headerLength = pktHeaderLength >> kVMBusPacketSizeShift;
    pktHeader->totalLength = pktTotalLength >> kVMBusPacketSizeShift;
    
    UInt32 writeIndexOld = txBuffer->writeIndex;
    UInt32 writeIndexNew = writeIndexOld;
    UInt64 writeIndexShifted = ((UInt64)writeIndexOld) << 32;
    
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
   // DBGLOG("Packet type %u, flags %u, header length %u, total length %u", pktHeader.type, pktHeader.flags, packetHeaderLength, packetTotalLength);
   // DBGLOG("RX read index %X, RX write index %X", rxBuffer->readIndex, rxBuffer->writeIndex);
    
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

UInt32 HyperVVMBusDevice::copyPacketDataFromRingBuffer(UInt32 readIndex, UInt32 readLength, void *data, UInt32 dataLength) {
  //
  // Check for wraparound.
  //
  if (dataLength > rxBufferSize - readIndex) {
    UInt32 fragmentLength = rxBufferSize - readIndex;
    DBGLOG("Overflow %u", fragmentLength);
    memcpy(data, &rxBuffer->buffer[readIndex], fragmentLength);
    memcpy((UInt8*)data + fragmentLength, rxBuffer->buffer, dataLength - fragmentLength);
  } else {
    memcpy(data, &rxBuffer->buffer[readIndex], dataLength);
  }
  
  return (readIndex + readLength) % rxBufferSize;
}

UInt32 HyperVVMBusDevice::copyPacketDataToRingBuffer(UInt32 writeIndex, void *data, UInt32 length) {
  //
  // Check for wraparound.
  //
  if (length > txBufferSize - writeIndex) {
    UInt32 fragmentLength = txBufferSize - writeIndex;
    DBGLOG("Overflow %u", fragmentLength);
    memcpy(&txBuffer->buffer[writeIndex], data, fragmentLength);
    memcpy(txBuffer->buffer, (UInt8*)data + fragmentLength, length - fragmentLength);
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
    DBGLOG("Overflow %u", fragmentLength);
    memset(&txBuffer->buffer[writeIndex], 0, fragmentLength);
    memset(txBuffer->buffer, 0, length - fragmentLength);
  } else {
    memset(&txBuffer->buffer[writeIndex], 0, length);
  }
  
  return (writeIndex + length) % txBufferSize;
}
