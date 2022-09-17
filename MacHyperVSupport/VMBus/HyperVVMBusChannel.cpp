//
//  HyperVVMBusChannel.cpp
//  Hyper-V VMBus controller
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVVMBus.hpp"

VMBusChannelStatus HyperVVMBus::getVMBusChannelStatus(UInt32 channelId) {
  if (channelId == 0 || channelId > kVMBusMaxChannels) {
    HVDBGLOG("One or more incorrect arguments provided");
    return kVMBusChannelStatusNotPresent;
  }
  return vmbusChannels[channelId].status;
}

IOReturn HyperVVMBus::openVMBusChannel(UInt32 channelId, UInt32 txBufferSize, VMBusRingBuffer **txBuffer, UInt32 rxBufferSize, VMBusRingBuffer **rxBuffer) {
  IOReturn     status;
  VMBusChannel *channel;
  
  UInt32 totalBufferSize;
  UInt32 totalPageCount;
  UInt32 txPageCount;
  UInt16 rxPageIndex;
  
  VMBusChannelMessageChannelOpen openMsg;
  
  //
  // TX and RX buffer sizes must be page-aligned.
  //
  if (channelId == 0 || channelId > kVMBusMaxChannels
      || txBufferSize == 0 || txBuffer == nullptr
      || rxBufferSize == 0 || rxBuffer == nullptr) {
    HVDBGLOG("One or more incorrect arguments provided");
    return kIOReturnBadArgument;
  }
  if (txBufferSize & PAGE_MASK || rxBufferSize & PAGE_MASK) {
    HVDBGLOG("TX/RX ring buffer sizes must be page-aligned");
    return kIOReturnNotAligned;
  }
  
  channel = &vmbusChannels[channelId];
  if (channel->status == kVMBusChannelStatusOpen) {
    HVDBGLOG("Channel %u is already open", channelId);
    return kIOReturnStillOpen;
  } else if (channel->status == kVMBusChannelStatusNotPresent) {
    HVDBGLOG("Channel %u is not present", channelId);
    return kIOReturnNotAttached;
  }
  HVDBGLOG("Channel %u flags 0x%X, monitor %u", channelId, channel->offerMessage.flags, channel->offerMessage.monitorAllocated);
  
  //
  // Calculate final size of TX and RX ring buffers.
  // These will be +1 pages to account for the state header before the actual ring buffer data.
  //
  txBufferSize += PAGE_SIZE;
  rxBufferSize += PAGE_SIZE;
  totalBufferSize = txBufferSize + rxBufferSize;
  totalPageCount  = totalBufferSize >> PAGE_SHIFT;
  txPageCount     = txBufferSize >> PAGE_SHIFT;
  rxPageIndex     = totalPageCount - txPageCount;
  
  //
  // Allocate channel ring buffers.
  // TX and RX ring buffers are allocated and provided to Hyper-V as a single large buffer.
  //
  allocateDmaBuffer(&channel->dataBuffer, totalBufferSize);
  allocateDmaBuffer(&channel->eventBuffer, PAGE_SIZE);
  
  //
  // Configure GPADL for channel.
  //
  status = initVMBusChannelGPADL(channelId, &channel->dataBuffer, &channel->dataGpadlHandle);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to initialize GPADL for channel %u ring buffer with status 0x%X", channelId, status);
    freeDmaBuffer(&channel->dataBuffer);
    freeDmaBuffer(&channel->eventBuffer);
    return status;
  }
  
  //
  // Configure TX and RX buffer pointers for channel state tracking.
  //
  channel->txBuffer    = (VMBusRingBuffer*) channel->dataBuffer.buffer;
  channel->rxBuffer    = (VMBusRingBuffer*) (((UInt8*)channel->dataBuffer.buffer) + (PAGE_SIZE * rxPageIndex));
  channel->rxPageIndex = rxPageIndex;
  
  //
  // Create channel open message.
  //
  bzero(&openMsg, sizeof (openMsg));
  openMsg.header.type                     = kVMBusChannelMessageTypeChannelOpen;
  openMsg.openId                          = channelId;
  openMsg.channelId                       = channelId;
  openMsg.ringBufferGpadlHandle           = channel->dataGpadlHandle;
  openMsg.downstreamRingBufferPageOffset  = channel->rxPageIndex;
  openMsg.targetCpu                       = 0;
  
  //
  // Send channel open message to Hyper-V and wait for response.
  //
  VMBusChannelMessageChannelOpenResponse openResponseMsg;
  if (!sendVMBusMessage((VMBusChannelMessage*) &openMsg, kVMBusChannelMessageTypeChannelOpenResponse, (VMBusChannelMessage*) &openResponseMsg)) {
    freeDmaBuffer(&channel->dataBuffer);
    freeDmaBuffer(&channel->eventBuffer);
    return kIOReturnIOError;
  }
  HVDBGLOG("Channel %u open result: 0x%X", channelId, openResponseMsg.status);
  
  if (openResponseMsg.status != kHyperVStatusSuccess) {
    return kIOReturnIOError;
  }
  
  channel->status = kVMBusChannelStatusOpen;
  *txBuffer = channel->txBuffer;
  *rxBuffer = channel->rxBuffer;
  
  HVDBGLOG("Channel %u configured (TX size: %u bytes, RX size: %u bytes)", channelId, txBufferSize, rxBufferSize);
  return kIOReturnSuccess;
}

IOReturn HyperVVMBus::closeVMBusChannel(UInt32 channelId) {
  bool         result;
  VMBusChannel *channel;
  
  VMBusChannelMessageChannelClose closeMsg;
  
  if (channelId == 0 || channelId > kVMBusMaxChannels) {
    HVDBGLOG("One or more incorrect arguments provided");
    return kIOReturnBadArgument;
  }
  channel = &vmbusChannels[channelId];
  
  if (channel->status == kVMBusChannelStatusClosed) {
    HVDBGLOG("Channel %u is already closed", channelId);
    return kIOReturnSuccess;
  } else if (channel->status == kVMBusChannelStatusNotPresent) {
    HVDBGLOG("Channel %u is not present", channelId);
    return kIOReturnNotAttached;
  }
  
  //
  // Close channel.
  //
  channel->status = kVMBusChannelStatusClosed;
  closeMsg.header.type      = kVMBusChannelMessageTypeChannelClose;
  closeMsg.header.reserved  = 0;
  closeMsg.channelId        = channelId;
  
  result = sendVMBusMessage((VMBusChannelMessage*) &closeMsg);
  if (!result) {
    HVSYSLOG("Failed to send channel close message for channel %u", channelId);
  }

  freeVMBusChannelGPADL(channelId, channel->dataGpadlHandle);
  
  //
  // Free channel buffers.
  //
  channel->txBuffer    = nullptr;
  channel->rxBuffer    = nullptr;
  channel->rxPageIndex = 0;
  freeDmaBuffer(&channel->dataBuffer);
  freeDmaBuffer(&channel->eventBuffer);
  
  HVDBGLOG("Channel %u is now closed", channelId);
  return kIOReturnSuccess;
}

IOReturn HyperVVMBus::initVMBusChannelGPADL(UInt32 channelId, HyperVDMABuffer *dmaBuffer, UInt32 *gpadlHandle) {
  bool result;
  
  UInt32 pageCount;
  UInt32 pfnSize;
  UInt32 pageHeaderCount;
  UInt32 messageSize;
  UInt64 physPageIndex;
  UInt32 pagesRemaining;
  UInt32 pagesBodyCount;
  bool needsMultipleMessages;
  
  VMBusChannelMessageGPADLHeader  *gpadlHeader;
  VMBusChannelMessageGPADLBody    *gpadlBody;
  VMBusChannelMessageGPADLCreated gpadlCreated;
  
  //
  // DMA buffer size must be page-aligned.
  //
  if (channelId == 0 || channelId > kVMBusMaxChannels
      || dmaBuffer == nullptr || gpadlHandle == nullptr) {
    HVDBGLOG("One or more incorrect arguments provided");
    return kIOReturnBadArgument;
  }
  if (dmaBuffer->size & PAGE_MASK) {
    HVDBGLOG("Buffer size must be page-aligned");
    return kIOReturnNotAligned;
  }
  
  //
  // Maximum number of pages allowed is 8190 (8192 - 2 for TX and RX headers).
  //
  pageCount = (UInt32)(dmaBuffer->size >> PAGE_SHIFT);
  if (pageCount > kHyperVMaxGpadlPages) {
    HVDBGLOG("%u is above the maximum supported number of GPADL pages");
    return kIOReturnBadArgument;
  }
  
  //
  // Get the next available GPADL handle.
  //
  *gpadlHandle = OSIncrementAtomic(&_nextGpadlHandle);
  
  //
  // For larger GPADL requests, a GPADL header and one or more GPADL body messages are required.
  // Otherwise we can use just the GPADL header.
  //
  pfnSize = kHyperVMessageDataSizeMax - sizeof (VMBusChannelMessageGPADLHeader) - sizeof (HyperVGPARange);
  pageHeaderCount = pfnSize / sizeof (UInt64);
  needsMultipleMessages = pageCount > pageHeaderCount;
  HVDBGLOG("Configuring GPADL handle 0x%X for channel %u of %u pages, multiple messages: %u",
           *gpadlHandle, channelId, pageCount, needsMultipleMessages);
  
  //
  // Create GPADL header message.
  //
  messageSize = sizeof (VMBusChannelMessageGPADLHeader) + sizeof (HyperVGPARange) + (pageHeaderCount * sizeof (UInt64));
  gpadlHeader = (VMBusChannelMessageGPADLHeader*) IOMalloc(messageSize);
  if (gpadlHeader == nullptr) {
    HVSYSLOG("Failed to allocate GPADL header message for channel %u", channelId);
    return kIOReturnNoResources;
  }
  bzero(gpadlHeader, messageSize);
  
  //
  // Header will contain the first batch of GPADL PFNs.
  //
  gpadlHeader->header.type         = kVMBusChannelMessageTypeGPADLHeader;
  gpadlHeader->channelId           = channelId;
  gpadlHeader->gpadl               = *gpadlHandle;
  gpadlHeader->rangeCount          = kHyperVGpadlRangeCount;
  gpadlHeader->rangeBufferLength   = sizeof (HyperVGPARange) + (pageCount * sizeof (UInt64)); // Max page count is 8190.
  gpadlHeader->range[0].byteOffset = 0;
  gpadlHeader->range[0].byteCount  = (UInt32)dmaBuffer->size;

  physPageIndex = dmaBuffer->physAddr >> PAGE_SHIFT;
  for (UInt32 i = 0; i < pageHeaderCount; i++) {
    gpadlHeader->range[0].pfnArray[i] = physPageIndex;
    physPageIndex++;
  }
  
  //
  // Send GPADL header message.
  // If there are multiple messages required, wait for response after the last one.
  //
  if (needsMultipleMessages) {
    result = sendVMBusMessageWithSize((VMBusChannelMessage*) gpadlHeader, messageSize);
  } else {
    result = sendVMBusMessageWithSize((VMBusChannelMessage*) gpadlHeader, messageSize,
                                      kVMBusChannelMessageTypeGPADLCreated, (VMBusChannelMessage*) &gpadlCreated);
  }
  IOFree(gpadlHeader, messageSize);
  if (!result) {
    HVSYSLOG("Failed to send GPADL header message for channel %u", channelId);
    return kIOReturnIOError; // TODO:
  }
  
  if (needsMultipleMessages) {
    //
    // Send rest of GPADL pages as body messages.
    //
    pagesRemaining = pageCount - pageHeaderCount;
    while (pagesRemaining > 0) {
      if (pagesRemaining > kHyperVMaxGpadlBodyPfns) {
        pagesBodyCount = kHyperVMaxGpadlBodyPfns;
      } else {
        pagesBodyCount = pagesRemaining;
      }
      
      messageSize = (UInt32) (sizeof (VMBusChannelMessageGPADLBody) + (pagesBodyCount * sizeof (UInt64)));
      gpadlBody = (VMBusChannelMessageGPADLBody*) IOMalloc(messageSize);
      if (gpadlBody == nullptr) {
        HVSYSLOG("Failed to allocate GPADL body message for channel %u", channelId);
        return kIOReturnNoResources;
      }
      bzero(gpadlBody, messageSize);
      
      gpadlBody->header.type = kVMBusChannelMessageTypeGPADLBody;
      gpadlBody->gpadl       = *gpadlHandle;
      for (UInt32 i = 0; i < pagesBodyCount; i++) {
        gpadlBody->pfn[i] = physPageIndex;
        physPageIndex++;
      }
      
      HVDBGLOG("Processed %u body pages for for channel %u, %u remaining", pagesBodyCount, channelId, pagesRemaining);
      pagesRemaining -= pagesBodyCount;
      
      //
      // Send body message.
      // For the last one, we want to wait for the creation response.
      //
      if (pagesRemaining == 0) {
        result = sendVMBusMessageWithSize((VMBusChannelMessage*) gpadlBody, messageSize,
                                          kVMBusChannelMessageTypeGPADLCreated, (VMBusChannelMessage*) &gpadlCreated);
      } else {
        result = sendVMBusMessageWithSize((VMBusChannelMessage*) gpadlBody, messageSize);
      }

      IOFree(gpadlBody, messageSize);
      if (!result) {
        HVSYSLOG("Failed to send GPADL body message for channel %u", channelId);
        return kIOReturnIOError;
      }
    }
  }
  
  HVDBGLOG("GPADL creation response for channel %u: 0x%X", channelId, gpadlCreated.status);
  if (gpadlCreated.status != kHyperVStatusSuccess) {
    HVSYSLOG("Failed to create GPADL for channel %u", channelId);
    return kIOReturnIOError;
  }
  
  return kIOReturnSuccess;
}

IOReturn HyperVVMBus::freeVMBusChannelGPADL(UInt32 channelId, UInt32 gpadlHandle) {
  bool                                     result;
  VMBusChannelMessageGPADLTeardown         gpadlTeardownMsg;
  VMBusChannelMessageGPADLTeardownResponse gpadlTeardownResponseMsg;
  
  if (channelId == 0 || channelId > kVMBusMaxChannels) {
    HVDBGLOG("One or more incorrect arguments provided");
    return kIOReturnBadArgument;
  }
  
  //
  // Send teardown GPADL message to Hyper-V.
  //
  gpadlTeardownMsg.header.type     = kVMBusChannelMessageTypeGPADLTeardown;
  gpadlTeardownMsg.header.reserved = 0;
  gpadlTeardownMsg.channelId       = channelId;
  gpadlTeardownMsg.gpadl           = gpadlHandle;
  
  result = sendVMBusMessage((VMBusChannelMessage*) &gpadlTeardownMsg,
                            kVMBusChannelMessageTypeGPADLTeardownResponse, (VMBusChannelMessage*) &gpadlTeardownResponseMsg);
  if (!result) {
    HVSYSLOG("Failed to send GPADL teardown message");
  }
  
  HVDBGLOG("GPADL freed for channel %u", channelId);
  return kIOReturnSuccess;
}

void HyperVVMBus::signalVMBusChannel(UInt32 channelId) {
  VMBusChannel *channel = &vmbusChannels[channelId];
  
  //
  // Signal Hyper-V the specified channel has data waiting on the TX ring.
  // Set bit for channel if legacy event flags are being used.
  //
  if (useLegacyEventFlags) {
    sync_set_bit(channelId, vmbusTxEventFlags->flags32);
  }
  hvController->hypercallSignalEvent(channel->offerMessage.connectionId);
}
