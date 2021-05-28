//
//  VMBusChannel.cpp
//  Hyper-V VMBus channel logic
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVVMBusController.hpp"
#include "HyperVVMBusInternal.hpp"

bool HyperVVMBusController::configureVMBusChannelGpadl(VMBusChannel *channel) {
  //
  // Get the next available GPADL handle.
  //
  IOLockLock(nextGpadlHandleLock);
  channel->gpadlHandle = nextGpadlHandle;
  nextGpadlHandle++;
  IOLockUnlock(nextGpadlHandleLock);
  
  UInt32 pageCount = (UInt32)(channel->dataBuffer.size >> PAGE_SHIFT);
  
  DBGLOG("Configuring GPADL handle 0x%X for channel %u of %llu pages", channel->gpadlHandle, channel->offerMessage.channelId, pageCount);
  
  //
  // For larger GPADL requests, one or more GPADL body messages are required.
  //
  UInt32 pfnSize = kHyperVMessageDataSizeMax -
    sizeof (VMBusChannelMessageGPADLHeader) - sizeof (HyperVGPARange);
  UInt32 pfnCount = pfnSize / sizeof (UInt64);
  
  
  DBGLOG("Total PFNs required: %u", pageCount);
  if (pageCount > pfnCount) {
    UInt32 messageSize = sizeof (VMBusChannelMessageGPADLHeader) +
      sizeof (HyperVGPARange) + pfnCount * sizeof (UInt64);
    VMBusChannelMessageGPADLHeader *gpadlHeader = (VMBusChannelMessageGPADLHeader*) IOMalloc(messageSize);
    if (gpadlHeader == NULL) {
      return false;
    }
    memset(gpadlHeader, 0, messageSize);
    
    gpadlHeader->header.type = kVMBusChannelMessageTypeGPADLHeader;
    gpadlHeader->channelId = channel->offerMessage.channelId;
    gpadlHeader->gpadl = channel->gpadlHandle;
    
    gpadlHeader->rangeCount = 1;
    gpadlHeader->rangeBufferLength = sizeof (HyperVGPARange) + pageCount * sizeof (UInt64); // Max page count is 8190
    gpadlHeader->range[0].byteOffset = 0;
    gpadlHeader->range[0].byteCount = (UInt32)channel->dataBuffer.size;
    UInt64 physPageIndex = channel->dataBuffer.physAddr >> PAGE_SHIFT;
    for (UInt32 i = 0; i < pfnCount; i++) {
      gpadlHeader->range[0].pfnArray[i] = physPageIndex;
     // DBGLOG("GPADL PFN %u is %llu", i, physPageIndex);
      physPageIndex++;
    }
    
    //VMBusChannelMessageGPADLCreated gpadlCreated;
    bool result = sendVMBusMessageWithSize((VMBusChannelMessage*) gpadlHeader, messageSize);
    IOFree(gpadlHeader, messageSize);
    
    UInt64 pfnSum = pfnCount;
    UInt64 pfnLeft = pageCount - pfnCount;
    
    UInt64 pfnSize = kHyperVMessageDataSizeMax - sizeof (VMBusChannelMessageGPADLBody);
    pfnCount = pfnSize / sizeof (UInt64);
    
    UInt64 pfnCurr;
    while (pfnLeft) {
      if (pfnLeft > pfnCount) {
        pfnCurr = pfnCount;
      } else {
        pfnCurr = pfnLeft;
      }
      
      UInt32 messageSize = sizeof (VMBusChannelMessageGPADLBody) + pfnCurr * sizeof (UInt64);
      VMBusChannelMessageGPADLBody *gpadlBody = (VMBusChannelMessageGPADLBody*) IOMalloc(messageSize);
      if (gpadlBody == NULL) {
        return false;
      }
      memset(gpadlBody, 0, messageSize);
      
      gpadlBody->header.type = kVMBusChannelMessageTypeGPADLBody;
      gpadlBody->gpadl = channel->gpadlHandle;
      for (UInt32 i = 0; i < pfnCurr; i++) {
        gpadlBody->pfn[i] = physPageIndex;
    //    DBGLOG("GPADL body PFN %u is %llu", i, physPageIndex);
        physPageIndex++;
      }
      

      
      
      
      
      DBGLOG("Processed %u pfns, %u left", pfnCurr, pfnLeft);
      pfnSum += pfnCurr;
      pfnLeft -= pfnCurr;
      
      if (pfnLeft == 0) {
        DBGLOG("Last one");
        VMBusChannelMessageGPADLCreated gpadlCreated;
        sendVMBusMessageWithSize((VMBusChannelMessage*) gpadlBody, messageSize, kVMBusChannelMessageTypeGPADLCreated, (VMBusChannelMessage*) &gpadlCreated);
        DBGLOG("GPADL creation response 0x%X for channel %u", gpadlCreated.status, channel->offerMessage.channelId);
      } else {
        sendVMBusMessageWithSize((VMBusChannelMessage*) gpadlBody, messageSize);
      }
      IOFree(gpadlBody, messageSize);
    }
  } else {
    //
    // Header message alone is sufficient.
    //
    UInt32 messageSize = sizeof (VMBusChannelMessageGPADLHeader) +
      sizeof (HyperVGPARange) + pageCount * sizeof (UInt64);
    VMBusChannelMessageGPADLHeader *gpadlHeader = (VMBusChannelMessageGPADLHeader*) IOMalloc(messageSize);
    if (gpadlHeader == NULL) {
      return false;
    }
    memset(gpadlHeader, 0, messageSize);
    
    //
    // Populate GPADL body and PFNs.
    //
    gpadlHeader->header.type = kVMBusChannelMessageTypeGPADLHeader;
    gpadlHeader->channelId = channel->offerMessage.channelId;
    gpadlHeader->gpadl = channel->gpadlHandle;
    
    gpadlHeader->rangeCount = 1;
    gpadlHeader->rangeBufferLength = sizeof (HyperVGPARange) + pageCount * sizeof (UInt64);
    gpadlHeader->range[0].byteOffset = 0;
    gpadlHeader->range[0].byteCount = (UInt32)channel->dataBuffer.size;
    UInt64 physPageIndex = channel->dataBuffer.physAddr >> PAGE_SHIFT;
    for (UInt32 i = 0; i < pageCount; i++) {
      gpadlHeader->range[0].pfnArray[i] = physPageIndex;
      DBGLOG("GPADL PFN %u is %llu", i, physPageIndex);
      physPageIndex++;
    }
    
    //
    // Send GPADL header and wait for GPADL created message.
    //
    VMBusChannelMessageGPADLCreated gpadlCreated;
    bool result = sendVMBusMessageWithSize((VMBusChannelMessage*) gpadlHeader, messageSize, kVMBusChannelMessageTypeGPADLCreated, (VMBusChannelMessage*) &gpadlCreated);
    IOFree(gpadlHeader, messageSize);
    
    DBGLOG("GPADL creation response 0x%X for channel %u", gpadlCreated.status, channel->offerMessage.channelId);
    
    if (!result) {
      return false;
    }
    
    
  }
  
  //
  // Configure TX and RX buffer pointers.
  //
  channel->txBuffer = (VMBusRingBuffer*) channel->dataBuffer.buffer;
  channel->rxBuffer = (VMBusRingBuffer*) (((UInt8*)channel->dataBuffer.buffer) + PAGE_SIZE * channel->rxPageIndex);
  channel->status = kVMBusChannelStatusGpadlConfigured;
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
  return configureVMBusChannel (channel);
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
