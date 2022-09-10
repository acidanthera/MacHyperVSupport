//
//  HyperVVMBusDevice.cpp
//  Hyper-V VMBus device nub
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVVMBusDevice.hpp"

OSDefineMetaClassAndStructors(HyperVVMBusDevice, super);

bool HyperVVMBusDevice::attach(IOService *provider) {
  bool superAttached = false;
  bool attached      = false;
  
  char     channelLocation[10];
  OSString *typeIdString;
  OSNumber *channelNumber;
  OSData   *instanceBytes;
  
  UInt8 builtInBytes = 0;
  OSData *builtInData;
  
  //
  // Get VMBus provider.
  //
  _vmbusProvider = OSDynamicCast(HyperVVMBus, provider);
  if (_vmbusProvider == nullptr) {
    HVSYSLOG("Provider is not HyperVVMBus");
    return false;
  }
  _vmbusProvider->retain();
  HVCheckDebugArgs();
  
  do {
    superAttached = super::attach(provider);
    if (!superAttached) {
      HVSYSLOG("super::attach() returned false");
      break;
    }
    
    //
    // Initialize work loop and command gate.
    //
    _workLoop = IOWorkLoop::workLoop();
    if (_workLoop == nullptr) {
      HVSYSLOG("Failed to initialize work loop");
      break;
    }
    
    _commandGate = IOCommandGate::commandGate(this);
    if (_commandGate == nullptr) {
      HVSYSLOG("Failed to initialize command gate");
      break;
    }
    _workLoop->addEventSource(_commandGate);
    
    //
    // Get channel number and GUID properties.
    //
    typeIdString  = OSDynamicCast(OSString, getProperty(kHyperVVMBusDeviceChannelTypeKey));
    channelNumber = OSDynamicCast(OSNumber, getProperty(kHyperVVMBusDeviceChannelIDKey));
    instanceBytes = OSDynamicCast(OSData, getProperty(kHyperVVMBusDeviceChannelInstanceKey));
    if (typeIdString == nullptr || channelNumber == nullptr || instanceBytes == nullptr) {
      HVSYSLOG("Failed to get channel properties");
      break;
    }
    
    //
    // Copy channel number and GUIDs.
    // uuid_string_t size includes null terminator.
    //
    strncpy(_typeId, typeIdString->getCStringNoCopy(), sizeof (_typeId) - 1);
    _channelId = channelNumber->unsigned32BitValue();
    HVDBGLOG("Attaching nub type %s for channel %u", _typeId, _channelId);
    memcpy(_instanceId, instanceBytes->getBytesNoCopy(), instanceBytes->getLength());
    
    //
    // Set location to ensure unique names in I/O Registry.
    //
    snprintf(channelLocation, sizeof (channelLocation), "%x", (unsigned int) _channelId);
    setLocation(channelLocation);
    
    //
    // The built-in property is required for some devices, like networking.
    //
    builtInData = OSData::withBytes(&builtInBytes, sizeof (builtInBytes));
    if (builtInData == nullptr) {
      HVSYSLOG("Failed to initialize built-in property");
      break;
    }
    setProperty("built-in", builtInData);
    builtInData->release();

    vmbusRequestsLock = IOLockAlloc();
    vmbusTransLock = IOLockAlloc();
    
    threadZeroRequest.lock = IOLockAlloc();
    prepareSleepThread();
    
    attached = true;
  } while (false);

  if (!attached) {
    if (!superAttached) {
      super::detach(provider);
    }
    
    if (_commandGate != nullptr) {
      _workLoop->removeEventSource(_commandGate);
    }
    OSSafeReleaseNULL(_commandGate);
    OSSafeReleaseNULL(_workLoop);
    OSSafeReleaseNULL(_vmbusProvider);
  }
  
  return attached;
}

void HyperVVMBusDevice::detach(IOService *provider) {
  //
  // Close and free channel.
  //
  if (_channelIsOpen) {
    closeChannel();
  }
  _vmbusProvider->freeVMBusChannel(_channelId);
  
  IOLockFree(vmbusRequestsLock);
  IOLockFree(vmbusTransLock);
  IOLockFree(threadZeroRequest.lock);
  
  super::detach(provider);
}

bool HyperVVMBusDevice::matchPropertyTable(OSDictionary *table, SInt32 *score) {
  if (!super::matchPropertyTable(table, score)) {
    HVDBGLOG("super::matchPropertyTable returned false");
    return false;
  }
  
  //
  // Get device type ID.
  //
  OSString *hvTypeString = OSDynamicCast(OSString, table->getObject(kHyperVVMBusDeviceChannelTypeKey));
  if (hvTypeString == nullptr) {
    HVDBGLOG("Hyper-V device type ID not found or not a string");
    return false;
  }
  
  if (strcmp(_typeId, hvTypeString->getCStringNoCopy()) != 0) {
    return false;
  }
  
  HVDBGLOG("Matched type ID %s", _typeId);
  return true;
}

IOWorkLoop* HyperVVMBusDevice::getWorkLoop() const {
  return _workLoop;
}

IOReturn HyperVVMBusDevice::installPacketActions(OSObject *target, PacketReadyAction packetReadyAction, WakePacketAction wakePacketAction,
                                                 UInt32 initialResponseBufferLength, bool registerInterrupt) {
  if (target == nullptr || packetReadyAction == nullptr) {
    return kIOReturnBadArgument;
  }
  if (_packetActionTarget != nullptr) {
    return kIOReturnExclusiveAccess;
  }
  
  _receivePacketBufferLength = initialResponseBufferLength;
  _receivePacketBuffer       = (UInt8*) IOMalloc(_receivePacketBufferLength);
  
  _packetActionTarget = target;
  _packetReadyAction  = packetReadyAction;
  _wakePacketAction   = wakePacketAction;
  if (registerInterrupt) {
    _interruptSource = IOInterruptEventSource::interruptEventSource(this,
                                                                    OSMemberFunctionCast(IOInterruptEventAction, this, &HyperVVMBusDevice::handleInterrupt),
                                                                    this, 0);
    if (_interruptSource == nullptr) {
      HVSYSLOG("Failed to configure interrupt for channel %u", _channelId);
      IOFree(_receivePacketBuffer, _receivePacketBufferLength);
      return kIOReturnNoResources;
    }
    _workLoop->addEventSource(_interruptSource);
    _interruptSource->enable();
  }

  HVDBGLOG("Data ready action handler installed (register interrupt: %u)", registerInterrupt);
  return kIOReturnSuccess;
}

IOReturn HyperVVMBusDevice::openVMBusChannel(UInt32 txSize, UInt32 rxSize, UInt64 maxAutoTransId) {
  IOReturn status;
  
  if (txSize == 0 || rxSize == 0) {
    return kIOReturnBadArgument;
  }
  
  if (_channelIsOpen) {
    return kIOReturnStillOpen;
  }
  HVDBGLOG("Attempting to open channel %u (TX size: %u, RX size: %u, max trans ID: 0x%llX)", _channelId, txSize, rxSize, maxAutoTransId);
  
  //
  // Open channel through VMBus provider.
  // The ability to have a maximum transaction ID is supported for some devices
  // that require both rolling transaction IDs and specific transaction IDs at the same time.
  //
  // This call is gated to prevent interrupt handler from firing partway through channel open, as some
  // devices will start sending data immediately after opening.
  //
  _maxAutoTransId = maxAutoTransId;
  status = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBusDevice::openVMBusChannelGated), &txSize, &rxSize);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to open VMBus channel %u with status: 0x%X", _channelId, status);
    return status;
  }
  HVDBGLOG("Channel %u is now open", _channelId);
  
  return kIOReturnSuccess;
}

bool HyperVVMBusDevice::openChannel(UInt32 txSize, UInt32 rxSize, UInt64 maxAutoTransId) {
  if (_channelIsOpen) {
    return true;
  }
  
  HVDBGLOG("Opening channel for %u", _channelId);
  _txBufferSize = txSize;
  _rxBufferSize = rxSize;
  
  //
  // Open channel.
  //
  _maxAutoTransId = maxAutoTransId;
  if (_vmbusProvider->openVMBusChannel(_channelId, _txBufferSize, &_txBuffer, _rxBufferSize, &_rxBuffer) != kIOReturnSuccess) {
    return false;
  }
  
  _channelIsOpen = true;
  HVDBGLOG("Opened channel for %u", _channelId);
  return true;
}

void HyperVVMBusDevice::closeChannel() {
  //
  // Close channel and stop interrupts.
  //
  _vmbusProvider->closeVMBusChannel(_channelId);
  _channelIsOpen = false;
}

bool HyperVVMBusDevice::createGpadlBuffer(UInt32 bufferSize, UInt32 *gpadlHandle, void **buffer) {
  HyperVDMABuffer buf;
  allocateDmaBuffer(&buf, bufferSize);
  
  if (_vmbusProvider->initVMBusChannelGPADL(_channelId, &buf, gpadlHandle) != kIOReturnSuccess) {
    return false;
  }
  
  *buffer = buf.buffer;
  return true;
}

bool HyperVVMBusDevice::allocateDmaBuffer(HyperVDMABuffer *dmaBuf, size_t size) {
  return _vmbusProvider->allocateDmaBuffer(dmaBuf, size);
}

void HyperVVMBusDevice::freeDmaBuffer(HyperVDMABuffer *dmaBuf) {
  _vmbusProvider->freeDmaBuffer(dmaBuf);
}

bool HyperVVMBusDevice::nextPacketAvailable(VMBusPacketType *type, UInt32 *packetHeaderLength, UInt32 *packetTotalLength) {
  return _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBusDevice::nextPacketAvailableGated),
                                type, packetHeaderLength, packetTotalLength) == kIOReturnSuccess;
}

bool HyperVVMBusDevice::nextInbandPacketAvailable(UInt32 *packetDataLength) {
  VMBusPacketType pktType;
  UInt32 pktHeaderLength;
  UInt32 pktTotalLength;
  
  if (packetDataLength == NULL) {
    return false;
  }

  bool result = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBusDevice::nextPacketAvailableGated),
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
  if (vmbusTransId > _maxAutoTransId) {
    // Some devices have issues with 0 as a transaction ID.
    vmbusTransId = 1;
  }
  IOLockUnlock(vmbusTransLock);
  return value;
}

IOReturn HyperVVMBusDevice::readRawPacket(void *buffer, UInt32 bufferLength) {
  return _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBusDevice::readRawPacketGated),
                                NULL, NULL, buffer, &bufferLength);
}

IOReturn HyperVVMBusDevice::readInbandCompletionPacket(void *buffer, UInt32 bufferLength, UInt64 *transactionId) {
  VMBusPacketHeader pktHeader;
  UInt32 pktHeaderSize = sizeof (pktHeader);
  
  IOReturn status = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBusDevice::readRawPacketGated),
                                           &pktHeader, &pktHeaderSize, buffer, &bufferLength);
  if (status == kIOReturnSuccess) {
    if (pktHeader.type != kVMBusPacketTypeDataInband && pktHeader.type != kVMBusPacketTypeCompletion) {
      HVMSGLOG("INBAND COMP attempted to read non-inband or non-completion packet");
      return kIOReturnUnsupported;
    }
    
    if (transactionId != NULL) {
      *transactionId = pktHeader.transactionId;
    }
  }
  return status;
}

IOReturn HyperVVMBusDevice::writeRawPacket(void *buffer, UInt32 bufferLength) {
  return _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBusDevice::writeRawPacketGated),
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
  
  HVMSGLOG("SP Packet type %u, flags %u, trans %llu, header length %u, total length %u, page count %u",
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

  IOReturn status = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBusDevice::writeRawPacketGated),
                                &pagePacket, &pagePacketLength, buffer, &bufferLength);
  
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
  
  HVMSGLOG("MP Packet type %u, flags %u, trans %llu, header length %u, total length %u",
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

  IOReturn status = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBusDevice::writeRawPacketGated),
                                pagePacket, &pagePacketLength, buffer, &bufferLength);
  
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

IOReturn HyperVVMBusDevice::writeCompletionPacketWithTransactionId(void *buffer, UInt32 bufferLength, UInt64 transactionId, bool responseRequired) {
  return writePacketInternal(buffer, bufferLength, kVMBusPacketTypeCompletion, transactionId, responseRequired, NULL, 0);
}

bool HyperVVMBusDevice::getPendingTransaction(UInt64 transactionId, void **buffer, UInt32 *bufferLength) {
  IOLockLock(vmbusRequestsLock);

  HyperVVMBusDeviceRequest *current = vmbusRequests;
  while (current != NULL) {
    if (current->transactionId == transactionId) {
      HVMSGLOG("Found transaction %u", transactionId);

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
      HVMSGLOG("Waking transaction %u", transactionId);

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

void HyperVVMBusDevice::doSleepThread() {
  sleepPacketRequest(&threadZeroRequest);
  prepareSleepThread();
}

#if DEBUG
void HyperVVMBusDevice::installTimerDebugPrintAction(OSObject *target, TimerDebugAction action) {
  _timerDebugTarget = target;
  _timerDebugAction = action;
}
#endif
