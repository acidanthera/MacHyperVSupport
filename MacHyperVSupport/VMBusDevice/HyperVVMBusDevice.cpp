//
//  HyperVVMBusDevice.cpp
//  Hyper-V VMBus device nub
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVVMBusDevice.hpp"
#include "HyperVVMBusDeviceInternal.hpp"

OSDefineMetaClassAndStructors(HyperVVMBusDevice, super);

bool HyperVVMBusDevice::attach(IOService *provider) {
  char channelLocation[10];
  
  if (!super::attach(provider)) {
    return false;
  }
  
  channelIsOpen = false;
  
  //
  // Get channel number.
  //
  OSNumber *channelNumber = OSDynamicCast(OSNumber, getProperty(kHyperVVMBusDeviceChannelIDKey));
  vmbusProvider = OSDynamicCast(HyperVVMBusController, getProvider());
  if (channelNumber == NULL || vmbusProvider == NULL) {
    return false;
  }
  channelId = channelNumber->unsigned32BitValue();
  HVDBGLOG("Attaching nub for channel %u", channelId);
  
  //
  // Set location to ensure unique names in I/O Registry.
  //
  snprintf(channelLocation, sizeof (channelLocation), "%x", channelId);
  setLocation(channelLocation);
  
  //
  // built-in required for some devices, like networking.
  //
  UInt8 builtInBytes = 0;
  OSData *builtInData = OSData::withBytes(&builtInBytes, sizeof (builtInBytes));
  if (builtInData != NULL) {
    setProperty("built-in", builtInData);
    builtInData->release();
  }

  vmbusRequestsLock = IOLockAlloc();
  vmbusTransLock = IOLockAlloc();
  
  return true;
}

void HyperVVMBusDevice::detach(IOService *provider) {
  //
  // Close and free channel.
  //
  if (channelIsOpen) {
    closeChannel();
  }
  vmbusProvider->freeVMBusChannel(channelId);
  
  IOLockFree(vmbusRequestsLock);
  IOLockFree(vmbusTransLock);
  
  super::detach(provider);
}

bool HyperVVMBusDevice::openChannel(UInt32 txSize, UInt32 rxSize, UInt64 maxAutoTransId) {
  if (channelIsOpen) {
    return true;
  }
  
  HVDBGLOG("Opening channel for %u", channelId);
  txBufferSize = txSize;
  rxBufferSize = rxSize;
  
  if (!setupCommandGate()) {
    return false;
  }
  
  //
  // Open channel.
  //
  vmbusMaxAutoTransId = maxAutoTransId;
  if (!vmbusProvider->initVMBusChannel(channelId, txBufferSize, &txBuffer, rxBufferSize, &rxBuffer)) {
    teardownCommandGate();
    return false;
  }
  
  if (!vmbusProvider->openVMBusChannel(channelId)) {
    vmbusProvider->closeVMBusChannel(channelId);
    teardownCommandGate();
    return false;
  }
  
  channelIsOpen = true;
  HVDBGLOG("Opened channel for %u", channelId);
  return true;
}

void HyperVVMBusDevice::closeChannel() {
  //
  // Close channel and stop interrupts.
  //
  vmbusProvider->closeVMBusChannel(channelId);
  teardownCommandGate();
  channelIsOpen = false;
}

bool HyperVVMBusDevice::createGpadlBuffer(UInt32 bufferSize, UInt32 *gpadlHandle, void **buffer) {
  return vmbusProvider->initVMBusChannelGpadl(channelId, bufferSize, gpadlHandle, buffer);
}

bool HyperVVMBusDevice::nextPacketAvailable(VMBusPacketType *type, UInt32 *packetHeaderLength, UInt32 *packetTotalLength) {
  return commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBusDevice::nextPacketAvailableGated),
                                type, packetHeaderLength, packetTotalLength) == kIOReturnSuccess;
}

bool HyperVVMBusDevice::nextInbandPacketAvailable(UInt32 *packetDataLength) {
  VMBusPacketType pktType;
  UInt32 pktHeaderLength;
  UInt32 pktTotalLength;
  
  if (packetDataLength == NULL) {
    return false;
  }

  bool result = commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBusDevice::nextPacketAvailableGated),
                                       &pktType, &pktHeaderLength, &pktTotalLength) == kIOReturnSuccess;
  
  if (result) {
    if (pktType == kVMBusPacketTypeDataInband) {
      *packetDataLength = pktTotalLength - pktHeaderLength;
    } else {
      result = false;
    }
  }
  return result;
}

UInt64 HyperVVMBusDevice::getNextTransId() {
  IOLockLock(vmbusTransLock);
  UInt64 value = vmbusTransId;
  vmbusTransId++;
  if (vmbusTransId > vmbusMaxAutoTransId) {
    vmbusTransId = 0;
  }
  IOLockUnlock(vmbusTransLock);
  return value;
}

IOReturn HyperVVMBusDevice::readRawPacket(void *buffer, UInt32 bufferLength) {
  return commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBusDevice::readRawPacketGated),
                                NULL, NULL, buffer, &bufferLength);
}

IOReturn HyperVVMBusDevice::readInbandCompletionPacket(void *buffer, UInt32 bufferLength, UInt64 *transactionId) {
  VMBusPacketHeader pktHeader;
  UInt32 pktHeaderSize = sizeof (pktHeader);
  
  IOReturn status = commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBusDevice::readRawPacketGated),
                                           &pktHeader, &pktHeaderSize, buffer, &bufferLength);
  if (status == kIOReturnSuccess) {
    if (pktHeader.type != kVMBusPacketTypeDataInband && pktHeader.type != kVMBusPacketTypeCompletion) {
      MSGDBG("INBAND COMP attempted to read non-inband or non-completion packet");
      return kIOReturnUnsupported;
    }
    
    if (transactionId != NULL) {
      *transactionId = pktHeader.transactionId;
    }
  }
  return status;
}

IOReturn HyperVVMBusDevice::writeRawPacket(void *buffer, UInt32 bufferLength) {
  return commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBusDevice::writeRawPacketGated),
                                NULL, NULL, buffer, &bufferLength);
}

IOReturn HyperVVMBusDevice::writeInbandPacket(void *buffer, UInt32 bufferLength, bool responseRequired,
                                              void *responseBuffer, UInt32 responseBufferLength) {
  return writeInbandPacketWithTransactionId(buffer, bufferLength, getNextTransId(), responseRequired, responseBuffer, responseBufferLength);
}

IOReturn HyperVVMBusDevice::writeInbandPacketWithTransactionId(void *buffer, UInt32 bufferLength, UInt64 transactionId,
                                                               bool responseRequired, void *responseBuffer, UInt32 responseBufferLength) {
  return writePacketInternal(buffer, bufferLength, kVMBusPacketTypeDataInband, transactionId, responseRequired, responseBuffer, responseBufferLength);
}

IOReturn HyperVVMBusDevice::writeGPADirectSinglePagePacket(void *buffer, UInt32 bufferLength, bool responseRequired,
                                                           VMBusSinglePageBuffer pageBuffers[], UInt32 pageBufferCount,
                                                           void *responseBuffer, UInt32 responseBufferLength) {
  if (pageBufferCount > kVMBusMaxPageBufferCount) {
    return kIOReturnNoResources;
  }

  //
  // Create packet for single page buffers.
  //
  UInt64 transactionId = getNextTransId();
  VMBusPacketSinglePageBuffer pagePacket;
  UInt32 pagePacketLength = sizeof (VMBusPacketSinglePageBuffer) -
    ((kVMBusMaxPageBufferCount - pageBufferCount) * sizeof (VMBusSinglePageBuffer));

  pagePacket.header.type          = kVMBusPacketTypeDataUsingGPADirect;
  pagePacket.header.headerLength  = pagePacketLength >> kVMBusPacketSizeShift;
  pagePacket.header.totalLength   = (pagePacketLength + bufferLength) >> kVMBusPacketSizeShift;
  pagePacket.header.flags         = responseRequired ? kVMBusPacketResponseRequired : 0;
  pagePacket.header.transactionId = transactionId;

  pagePacket.reserved             = 0;
  pagePacket.rangeCount           = pageBufferCount;
  for (int i = 0; i < pagePacket.rangeCount; i++) {
    pagePacket.ranges[i].length = pageBuffers[i].length;
    pagePacket.ranges[i].offset = pageBuffers[i].offset;
    pagePacket.ranges[i].pfn    = pageBuffers[i].pfn;
  }
  
  MSGDBG("SP Packet type %u, flags %u, trans %llu, header length %u, total length %u, page count %u",
         pagePacket.header.type, pagePacket.header.flags, pagePacket.header.transactionId,
         pagePacket.header.headerLength, pagePacket.header.totalLength, pageBufferCount);
  
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
                                &pagePacket, &pagePacketLength, buffer, &bufferLength);
  
  if (responseBuffer != NULL) {
    if (status == kIOReturnSuccess) {
      sleepPacketRequest(&req);
    } else {
      wakeTransaction(transactionId);
    }
  }
  return status;
}

IOReturn HyperVVMBusDevice::writeGPADirectMultiPagePacket(void *buffer, UInt32 bufferLength, bool responseRequired,
                                                          VMBusPacketMultiPageBuffer *pagePacket, UInt32 pagePacketLength,
                                                          void *responseBuffer, UInt32 responseBufferLength) {
  //
  // For multi-page buffers, the packet header itself is passed to this function.
  // Ensure general header fields are set.
  //
  UInt64 transactionId = getNextTransId();

  pagePacket->header.type           = kVMBusPacketTypeDataUsingGPADirect;
  pagePacket->header.headerLength   = pagePacketLength >> kVMBusPacketSizeShift;
  pagePacket->header.totalLength    = (pagePacketLength + bufferLength) >> kVMBusPacketSizeShift;
  pagePacket->header.flags          = responseRequired ? kVMBusPacketResponseRequired : 0;
  pagePacket->header.transactionId  = transactionId;

  pagePacket->reserved              = 0;
  pagePacket->rangeCount            = 1;
  
  MSGDBG("MP Packet type %u, flags %u, trans %llu, header length %u, total length %u",
         pagePacket->header.type, pagePacket->header.flags, pagePacket->header.transactionId,
         pagePacket->header.headerLength, pagePacket->header.totalLength);
  
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
                                pagePacket, &pagePacketLength, buffer, &bufferLength);
  
  if (responseBuffer != NULL) {
    if (status == kIOReturnSuccess) {
      sleepPacketRequest(&req);
    } else {
      wakeTransaction(transactionId);
    }
  }
  return status;
}

IOReturn HyperVVMBusDevice::writeCompletionPacketWithTransactionId(void *buffer, UInt32 bufferLength, UInt64 transactionId, bool responseRequired) {
  return writePacketInternal(buffer, bufferLength, kVMBusPacketTypeCompletion, transactionId, responseRequired, NULL, 0);
}

bool HyperVVMBusDevice::getPendingTransaction(UInt64 transactionId, void **buffer, UInt32 *bufferLength) {
  IOLockLock(vmbusRequestsLock);

  HyperVVMBusDeviceRequest *current = vmbusRequests;
  while (current != NULL) {
    if (current->transactionId == transactionId) {
      MSGDBG("Found transaction %u", transactionId);

      *buffer       = current->responseData;
      *bufferLength = current->responseDataLength;
      IOLockUnlock(vmbusRequestsLock);
      return true;
    }
    current = current->next;
  }

  IOLockUnlock(vmbusRequestsLock);
  return false;
}

void HyperVVMBusDevice::wakeTransaction(UInt64 transactionId) {
  IOLockLock(vmbusRequestsLock);

  HyperVVMBusDeviceRequest *current  = vmbusRequests;
  HyperVVMBusDeviceRequest *previous = NULL;
  while (current != NULL) {
    if (current->transactionId == transactionId) {
      MSGDBG("Waking transaction %u", transactionId);

      //
      // Remove from linked list.
      //
      if (previous != NULL) {
        previous->next = current->next;
      } else {
        vmbusRequests = current->next;
      }
      IOLockUnlock(vmbusRequestsLock);

      //
      // Wake sleeping thread.
      //
      IOLockLock(current->lock);
      current->isSleeping = false;
      IOLockUnlock(current->lock);
      IOLockWakeup(current->lock, &current->isSleeping, true);
      return;
    }
    previous  = current;
    current   = current->next;
  }
  IOLockUnlock(vmbusRequestsLock);
}
