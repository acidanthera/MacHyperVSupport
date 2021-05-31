//
//  VMBusChannel.cpp
//  Hyper-V VMBus channel logic
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVVMBusController.hpp"
#include "HyperVVMBusInternal.hpp"

bool HyperVVMBusController::configureVMBusChannelGpadl(VMBusChannel *channel) {
  UInt32 channelId = channel->offerMessage.channelId;
  
  //
  // Get the next available GPADL handle.
  //
  if (!IOSimpleLockTryLock(nextGpadlHandleLock)) {
    SYSLOG("Failed to acquire GPADL handle lock");
    return false;
  }

  channel->gpadlHandle = nextGpadlHandle;
  nextGpadlHandle++;
  IOSimpleLockUnlock(nextGpadlHandleLock);
  
  //
  // Maximum number of pages allowed is 8190 (8192 - 2 for TX and RX headers).
  //
  UInt32 pageCount = (UInt32)(channel->dataBuffer.size >> PAGE_SHIFT);
  if (pageCount > kHyperVMaxGpadlPages) {
    SYSLOG("%u is above the maximum supported number of GPADL pages");
    return false;
  }
  
  DBGLOG("Configuring GPADL handle 0x%X for channel %u of %llu pages", channel->gpadlHandle, channelId, pageCount);
  
  //
  // For larger GPADL requests, a GPADL header and one or more GPADL body messages are required.
  // Otherwise we can use just the GPADL header.
  //
  UInt32 pfnSize = kHyperVMessageDataSizeMax -
    sizeof (VMBusChannelMessageGPADLHeader) - sizeof (HyperVGPARange);
  UInt32 pageHeaderCount = pfnSize / sizeof (UInt64);
  
  DBGLOG("Total GPADL PFNs required: %u, multiple messages required: %u", pageCount, pageCount > pageHeaderCount);
  if (pageCount > pageHeaderCount) {
    //
    // Create GPADL header message.
    //
    UInt32 messageSize = sizeof (VMBusChannelMessageGPADLHeader) +
      sizeof (HyperVGPARange) + pageHeaderCount * sizeof (UInt64);

    VMBusChannelMessageGPADLHeader *gpadlHeader = (VMBusChannelMessageGPADLHeader*) IOMalloc(messageSize);
    if (gpadlHeader == NULL) {
      SYSLOG("Failed to allocate GPADL header message");
      return false;
    }
    memset(gpadlHeader, 0, messageSize);
    
    //
    // Header will contain the first batch of GPADL PFNs.
    //
    gpadlHeader->header.type          = kVMBusChannelMessageTypeGPADLHeader;
    gpadlHeader->channelId            = channelId;
    gpadlHeader->gpadl                = channel->gpadlHandle;
    gpadlHeader->rangeCount           = kHyperVGpadlRangeCount;
    gpadlHeader->rangeBufferLength    = sizeof (HyperVGPARange) + pageCount * sizeof (UInt64); // Max page count is 8190
    gpadlHeader->range[0].byteOffset  = 0;
    gpadlHeader->range[0].byteCount   = (UInt32)channel->dataBuffer.size;

    UInt64 physPageIndex = channel->dataBuffer.physAddr >> PAGE_SHIFT;
    for (UInt32 i = 0; i < pageHeaderCount; i++) {
      gpadlHeader->range[0].pfnArray[i] = physPageIndex;
      physPageIndex++;
    }
    
    //
    // Send GPADL header message.
    //
    bool result = sendVMBusMessageWithSize((VMBusChannelMessage*) gpadlHeader, messageSize);
    IOFree(gpadlHeader, messageSize);
    if (!result) {
      SYSLOG("Failed to send GPADL header message for channel %u", channelId);
      return false;
    }
    
    //
    // Send rest of GPADL pages as body messages.
    //
    UInt32 pagesRemaining = pageCount - pageHeaderCount;
    
    UInt32 pagesBodyCount;
    while (pagesRemaining > 0) {
      if (pagesRemaining > kHyperVMaxGpadlBodyPfns) {
        pagesBodyCount = kHyperVMaxGpadlBodyPfns;
      } else {
        pagesBodyCount = pagesRemaining;
      }
      
      UInt32 messageSize = (UInt32) (sizeof (VMBusChannelMessageGPADLBody) + pagesBodyCount * sizeof (UInt64));
      VMBusChannelMessageGPADLBody *gpadlBody = (VMBusChannelMessageGPADLBody*) IOMalloc(messageSize);
      if (gpadlBody == NULL) {
        SYSLOG("Failed to allocate GPADL body message for channel %u", channelId);
        return false;
      }
      memset(gpadlBody, 0, messageSize);
      
      gpadlBody->header.type  = kVMBusChannelMessageTypeGPADLBody;
      gpadlBody->gpadl        = channel->gpadlHandle;
      for (UInt32 i = 0; i < pagesBodyCount; i++) {
        gpadlBody->pfn[i]     = physPageIndex;
        physPageIndex++;
      }
      
      DBGLOG("Processed %u body pages for for channel %u, %u remaining", pagesBodyCount, channelId, pagesRemaining);
      pagesRemaining -= pagesBodyCount;
      
      //
      // Send body message.
      // For the last one, we want to wait for the creation response.
      //
      if (pagesRemaining == 0) {
        VMBusChannelMessageGPADLCreated gpadlCreated;
        result = sendVMBusMessageWithSize((VMBusChannelMessage*) gpadlBody, messageSize, kVMBusChannelMessageTypeGPADLCreated, (VMBusChannelMessage*) &gpadlCreated);
        DBGLOG("GPADL creation response 0x%X for channel %u", gpadlCreated.status, channelId);

        if (!result && gpadlCreated.status != kHyperVStatusSuccess) {
          SYSLOG("Failed to create GPADL for channel %u", channelId);
          
          IOFree(gpadlBody, messageSize);
          return false;
        }
      } else {
        result = sendVMBusMessageWithSize((VMBusChannelMessage*) gpadlBody, messageSize);
      }

      IOFree(gpadlBody, messageSize);
      if (!result) {
        SYSLOG("Failed to send GPADL body message for channel %u", channelId);
        return false;
      }
    }

  } else {
    //
    // Header message alone is sufficient.
    //
    UInt32 messageSize = sizeof (VMBusChannelMessageGPADLHeader) +
      sizeof (HyperVGPARange) + pageCount * sizeof (UInt64);

    VMBusChannelMessageGPADLHeader *gpadlHeader = (VMBusChannelMessageGPADLHeader*) IOMalloc(messageSize);
    if (gpadlHeader == NULL) {
      SYSLOG("Failed to allocate GPADL header message for channel %u", channelId);
      return false;
    }
    memset(gpadlHeader, 0, messageSize);
    
    //
    // Header will contain all of the GPADL PFNs.
    //
    gpadlHeader->header.type          = kVMBusChannelMessageTypeGPADLHeader;
    gpadlHeader->channelId            = channelId;
    gpadlHeader->gpadl                = channel->gpadlHandle;
    gpadlHeader->rangeCount           = kHyperVGpadlRangeCount;
    gpadlHeader->rangeBufferLength    = sizeof (HyperVGPARange) + pageCount * sizeof (UInt64);
    gpadlHeader->range[0].byteOffset  = 0;
    gpadlHeader->range[0].byteCount   = (UInt32)channel->dataBuffer.size;

    UInt64 physPageIndex = channel->dataBuffer.physAddr >> PAGE_SHIFT;
    for (UInt32 i = 0; i < pageCount; i++) {
      gpadlHeader->range[0].pfnArray[i] = physPageIndex;
      physPageIndex++;
    }
    
    //
    // Send GPADL header message, waiting for response.
    //
    VMBusChannelMessageGPADLCreated gpadlCreated;
    bool result = sendVMBusMessageWithSize((VMBusChannelMessage*) gpadlHeader,
                                           messageSize, kVMBusChannelMessageTypeGPADLCreated, (VMBusChannelMessage*) &gpadlCreated);
    IOFree(gpadlHeader, messageSize);
    
    DBGLOG("GPADL creation response 0x%X for channel %u", gpadlCreated.status, channelId);
    
    if (!result) {
      SYSLOG("Failed to send GPADL header message");
      return false;
    } else if (gpadlCreated.status != kHyperVStatusSuccess) {
      SYSLOG("Failed to create GPADL for channel %u", channel->offerMessage.channelId);
      return false;
    }
  }
  
  //
  // Configure TX and RX buffer pointers.
  //
  channel->txBuffer = (VMBusRingBuffer*) channel->dataBuffer.buffer;
  channel->rxBuffer = (VMBusRingBuffer*) (((UInt8*)channel->dataBuffer.buffer) + PAGE_SIZE * channel->rxPageIndex);
  channel->status   = kVMBusChannelStatusGpadlConfigured;
  return true;
}

bool HyperVVMBusController::configureVMBusChannel(VMBusChannel *channel) {
  VMBusChannelMessageChannelOpen openMsg;
  memset(&openMsg, 0, sizeof (openMsg));
  
  openMsg.header.type                     = kVMBusChannelMessageTypeChannelOpen;
  openMsg.openId                          = channel->offerMessage.channelId;
  openMsg.channelId                       = channel->offerMessage.channelId;
  openMsg.ringBufferGpadlHandle           = channel->gpadlHandle;
  openMsg.downstreamRingBufferPageOffset  = channel->rxPageIndex;
  openMsg.targetCpu                       = 0;
  
  //
  // Send channel open message and wait for response.
  //
  VMBusChannelMessageChannelOpenResponse openResponseMsg;
  channel->status = kVMBusChannelStatusOpen;
  if (!sendVMBusMessage((VMBusChannelMessage*) &openMsg, kVMBusChannelMessageTypeChannelOpenResponse, (VMBusChannelMessage*) &openResponseMsg)) {
    return false;
  }
  DBGLOG("Channel %u open result: 0x%X", channel->offerMessage.channelId, openResponseMsg.status);
  
  if (openResponseMsg.status != kHyperVStatusSuccess) {
    channel->status = kVMBusChannelStatusClosed;
    return false;
  }

  return true;
}

bool HyperVVMBusController::initVMBusChannel(UInt32 channelId, UInt32 txBufferSize, VMBusRingBuffer **txBuffer, UInt32 rxBufferSize, VMBusRingBuffer **rxBuffer) {
  if (channelId > kHyperVMaxChannels) {
    return false;
  }
  
  //
  // TX and RX buffers need to be page aligned.
  //
  if (txBufferSize & PAGE_MASK || rxBufferSize & PAGE_MASK) {
    return false;
  }
  
  VMBusChannel *channel = &vmbusChannels[channelId];
  DBGLOG("Channel flags %u %u %u", channelId, channel->offerMessage.flags, channel->offerMessage.monitorAllocated);
  
  //
  // The actual buffers will be +1 page each to account for state info.
  //
  txBufferSize += PAGE_SIZE;
  rxBufferSize += PAGE_SIZE;
  UInt32 totalBufferSize = txBufferSize + rxBufferSize;
  UInt32 totalPageCount = totalBufferSize >> PAGE_SHIFT;
  UInt32 txPageCount = txBufferSize >> PAGE_SHIFT;
  channel->rxPageIndex = totalPageCount - txPageCount;
  
  //
  // Allocate channel ring buffers.
  //
  allocateDmaBuffer(&channel->dataBuffer, totalBufferSize);
  allocateDmaBuffer(&channel->eventBuffer, PAGE_SIZE);
  
  //
  // Configure GPADL for channel.
  //
  if (!configureVMBusChannelGpadl(channel)) {
    return false;
  }
  
  *txBuffer = channel->txBuffer;
  *rxBuffer = channel->rxBuffer;
  
  return true;
}

bool HyperVVMBusController::openVMBusChannel(UInt32 channelId) {
  if (channelId > kHyperVMaxChannels) {
    return false;
  }
  
  //
  // Configure VMBus channel for operation.
  //
  // Incoming interrupts may begin before this function returns.
  //
  VMBusChannel *channel = &vmbusChannels[channelId];
  return configureVMBusChannel(channel);
}

void HyperVVMBusController::signalVMBusChannel(UInt32 channelId) {
  VMBusChannel *channel = &vmbusChannels[channelId];
  
  //
  // Set bit for channel.
  //
//  vmbusTxEventFlags[VMBUS_CHANNEL_EVENT_INDEX(channelId)] |= VMBUS_CHANNEL_EVENT_MASK(channelId);
  
  //
  // Signal event for specified connection.
  //
  hypercallSignalEvent(channel->offerMessage.connectionId);
}

void HyperVVMBusController::closeVMBusChannel(UInt32 channelId) {
  VMBusChannel *channel = &vmbusChannels[channelId];

  bool channelIsOpen = channel->status == kVMBusChannelStatusOpen;
  bool result = true;
  
  //
  // Prevent any further interrupts from reaching the VMBus device nub.
  //
  channel->status = kVMBusChannelStatusClosed;
  
  //
  // Close channel.
  //
  if (channelIsOpen) {
    VMBusChannelMessageChannelClose closeMsg;
    closeMsg.header.type      = kVMBusChannelMessageTypeChannelClose;
    closeMsg.header.reserved  = 0;
    closeMsg.channelId        = channelId;
    
    result = sendVMBusMessage((VMBusChannelMessage*) &closeMsg);
    if (!result) {
      SYSLOG("Failed to send channel close message for channel %u", channelId);
    }
    DBGLOG("Channel %u is now closed", channelId);
  }
  
  //
  // Teardown GPADL.
  //
  VMBusChannelMessageGPADLTeardown gpadlTeardownMsg;
  gpadlTeardownMsg.header.type      = kVMBusChannelMessageTypeGPADLTeardown;
  gpadlTeardownMsg.header.reserved  = 0;
  gpadlTeardownMsg.channelId        = channelId;
  gpadlTeardownMsg.gpadl            = channel->gpadlHandle;
  
  VMBusChannelMessageGPADLTeardownResponse gpadlTeardownResponseMsg;
  result = sendVMBusMessage((VMBusChannelMessage*) &gpadlTeardownMsg,
                            kVMBusChannelMessageTypeGPADLTeardownResponse, (VMBusChannelMessage*) &gpadlTeardownResponseMsg);
  if (!result) {
    SYSLOG("Failed to send GPADL teardown message");
  }
  DBGLOG("GPADL torn down for channel %u", channelId);
  
  //
  // Free ring buffers.
  //
  channel->rxPageIndex = 0;
  channel->txBuffer = NULL;
  channel->rxBuffer = NULL;
  
  //
  // Allocate channel ring buffers.
  //
  freeDmaBuffer(&channel->dataBuffer);
  freeDmaBuffer(&channel->eventBuffer);
}

void HyperVVMBusController::freeVMBusChannel(UInt32 channelId) {
  VMBusChannel *channel = &vmbusChannels[channelId];
  
  //
  // Free channel ID to be reused later on by Hyper-V.
  //
  VMBusChannelMessageChannelFree freeMsg;
  freeMsg.header.type      = kVMBusChannelMessageTypeChannelFree;
  freeMsg.header.reserved  = 0;
  freeMsg.channelId        = channelId;
  
  bool result = sendVMBusMessage((VMBusChannelMessage*) &freeMsg);
  if (!result) {
    SYSLOG("Failed to send channel free message for channel %u", channelId);
  }
  DBGLOG("Channel %u is now freed", channelId);
  channel->status = kVMBusChannelStatusNotPresent;
}
