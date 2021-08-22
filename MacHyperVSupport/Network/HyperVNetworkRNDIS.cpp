//
//  HyperVNetworkPrivate.cpp
//  Hyper-V network driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVNetwork.hpp"

bool HyperVNetwork::processRNDISPacket(UInt8 *data, UInt32 dataLength) {
  HyperVNetworkRNDISMessage *rndisPkt = (HyperVNetworkRNDISMessage*)data;
  
  DBGLOG("New RNDIS packet of type 0x%X and %u bytes", rndisPkt->msgType, rndisPkt->msgLength);
  
  HyperVNetworkRNDISRequest *reqCurr = rndisRequests;
  HyperVNetworkRNDISRequest *reqPrev = NULL;
  
  switch (rndisPkt->msgType) {
    case kHyperVNetworkRNDISMessageTypeInitComplete:
    case kHyperVNetworkRNDISMessageTypeQueryComplete:
    case kHyperVNetworkRNDISMessageTypeSetComplete:
    case kHyperVNetworkRNDISMessageTypeResetComplete:

      
      while (reqCurr != NULL) {
        DBGLOG("checking %u", reqCurr->message.requestId);
        if (reqCurr->message.requestId == rndisPkt->initComplete.requestId) {
          //
          // Copy response data.
          //
          memcpy(&reqCurr->message, rndisPkt, dataLength);
          
          //
          // Remove from linked list.
          //
          IOLockLock(rndisLock);
          if (reqPrev == NULL) {
            rndisRequests = reqCurr->next;
          } else {
            reqPrev->next = reqCurr->next;
          }
          IOLockUnlock(rndisLock);
          
          //
          // Wakeup sleeping thread.
          //
          IOLockLock(reqCurr->lock);
          reqCurr->isSleeping = false;
          IOLockUnlock(reqCurr->lock);
          IOLockWakeup(reqCurr->lock, &reqCurr->isSleeping, true);
          
          return true;
        }
        
        reqPrev = reqCurr;
        reqCurr = reqCurr->next;
      }
      break;
      
    case kHyperVNetworkRNDISMessageTypePacket:
      for (int i = 0; i < dataLength; i++) {
        IOLog(" %X", data[i]);
      }
      IOLog("\n");
      break;
      
    default:
      break;
  }
  
  return true;
}

HyperVNetworkRNDISRequest* HyperVNetwork::allocateRNDISRequest() {
  HyperVNetworkRNDISRequest *rndisRequest;
  IOBufferMemoryDescriptor  *bufDesc;
  IOLock                    *lock;
  
  //
  // Allocate lock for waiting.
  //
  lock = IOLockAlloc();
  if (lock == NULL) {
    SYSLOG("Failed to allocate lock for RNDIS request");
    return NULL;
  }
  
  //
  // Create DMA buffer with required specifications and get physical address.
  //
  bufDesc = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
                                                             kIODirectionInOut | kIOMemoryPhysicallyContiguous | kIOMapInhibitCache | kIOMemoryMapperNone,
                                                             sizeof (HyperVNetworkRNDISRequest), 0xFFFFFFFFFFFFF000ULL);
  if (bufDesc == NULL) {
    SYSLOG("Failed to allocate buffer memory for RNDIS request");
    IOLockFree(lock);
    return NULL;
  }
  bufDesc->prepare();
  
  rndisRequest = (HyperVNetworkRNDISRequest*)bufDesc->getBytesNoCopy();
  memset(rndisRequest, 0, sizeof (HyperVNetworkRNDISRequest));
  
  rndisRequest->lock = lock;
  rndisRequest->isSleeping = false;
  rndisRequest->memDescriptor = bufDesc;
  rndisRequest->messagePhysicalAddress = bufDesc->getPhysicalAddress();
  DBGLOG("Mapped RNDIS request buffer to phys 0x%llX", rndisRequest->messagePhysicalAddress);
  
  return rndisRequest;
}

void HyperVNetwork::freeRNDISRequest(HyperVNetworkRNDISRequest *rndisRequest) {
  IOLockFree(rndisRequest->lock);
  rndisRequest->memDescriptor->complete();
  rndisRequest->memDescriptor->release();
}

UInt32 HyperVNetwork::getNextRNDISTransId() {
  IOLockLock(rndisLock);
  UInt32 value = rndisTransId;
  rndisTransId++;
  IOLockUnlock(rndisLock);
  return value;
}

bool HyperVNetwork::sendRNDISMessage(HyperVNetworkRNDISRequest *rndisRequest, bool waitResponse) {
  //
  // Create page buffer set.
  //
  VMBusSinglePageBuffer pageBuffer;
  pageBuffer.length = rndisRequest->message.msgLength;
  pageBuffer.offset = 0;
  pageBuffer.pfn = rndisRequest->messagePhysicalAddress >> PAGE_SHIFT;
  
  //
  // Create packet for sending the RNDIS request.
  //
  HyperVNetworkMessage netMsg;
  memset(&netMsg, 0, sizeof (netMsg));
  netMsg.messageType = kHyperVNetworkMessageTypeV1SendRNDISPacket;
  netMsg.v1.sendRNDISPacket.channelType = kHyperVNetworkRNDISChannelTypeControl;
  netMsg.v1.sendRNDISPacket.sendBufferSectionIndex = -1;
  netMsg.v1.sendRNDISPacket.sendBufferSectionSize = 0;
  
  //UInt32 respSize = sizeof (netMsg);
  rndisRequest->isSleeping = true;
  rndisRequest->message.requestId = getNextRNDISTransId();
  
  HyperVVMBusDeviceRequestNew vmRequest;
  vmRequest.lock = IOLockAlloc();
  vmRequest.isSleeping = true;
  vmRequest.next = NULL;
  vmRequest.sendData = &netMsg;
  vmRequest.sendDataLength = sizeof (netMsg);
  vmRequest.responseData = &netMsg;
  vmRequest.responseDataLength = sizeof (netMsg);
  
  //
  // Add to linked list.
  //
  IOLockLock(rndisLock);
  if (rndisRequests == NULL)
    rndisRequests = rndisRequest;
  else
    rndisRequests->next = rndisRequest;
  
  vmbusRequests = &vmRequest;
  IOLockUnlock(rndisLock);
  
  hvDevice->writeGPADirectSinglePagePacket(&netMsg, sizeof (netMsg), true, 0, &pageBuffer, 1);
  
  IOLockLock(vmRequest.lock);
  while (vmRequest.isSleeping) {
    IOLockSleep(vmRequest.lock, &vmRequest.isSleeping, THREAD_INTERRUPTIBLE);
  }
  IOLockUnlock(vmRequest.lock);
  DBGLOG("woke after vmbus");
  
  //hvDevice->sendMessageSinglePageBuffers(&netMsg, sizeof (netMsg), 0, &pageBuffer, 1, true, &netMsg, &respSize);
  
  IOLockLock(rndisRequest->lock);
  while (rndisRequest->isSleeping) {
    IOLockSleep(rndisRequest->lock, &rndisRequest->isSleeping, THREAD_INTERRUPTIBLE);
  }
  IOLockUnlock(rndisRequest->lock);
  
  DBGLOG("woke");
  
  return true;
}

bool HyperVNetwork::initializeRNDIS() {
  HyperVNetworkRNDISRequest *rndisRequest = allocateRNDISRequest();
  rndisRequest->message.msgType   = kHyperVNetworkRNDISMessageTypeInit;
  rndisRequest->message.msgLength = sizeof(HyperVNetworkRNDISMessageInitializeRequest) + 8;
  
  rndisRequest->message.initRequest.majorVersion    = kHyperVNetworkRNDISVersionMajor;
  rndisRequest->message.initRequest.minorVersion    = kHyperVNetworkRNDISVersionMinor;
  rndisRequest->message.initRequest.maxTransferSize = kHyperVNetworkRNDISMaxTransferSize;
  
  bool result = sendRNDISMessage(rndisRequest);
  if (result) {
    DBGLOG("RNDIS initializated with status 0x%X", rndisRequest->message.initComplete.status);
    result = rndisRequest->message.initComplete.status == kHyperVNetworkRNDISStatusSuccess;
  } else {
    SYSLOG("Failed to send RNDIS initialization request");
  }
  
  freeRNDISRequest(rndisRequest);
  return result;
}

bool HyperVNetwork::queryRNDISOID(HyperVNetworkRNDISOID oid, void *value, UInt32 *valueSize) {
  if (value == NULL || valueSize == NULL) {
    return false;
  }
  
  HyperVNetworkRNDISRequest *rndisRequest = allocateRNDISRequest();
  rndisRequest->message.msgType   = kHyperVNetworkRNDISMessageTypeQuery;
  rndisRequest->message.msgLength = sizeof(HyperVNetworkRNDISMessageQueryRequest) + 8;
  
  rndisRequest->message.queryRequest.oid              = oid;
  rndisRequest->message.queryRequest.infoBufferOffset = sizeof(HyperVNetworkRNDISMessageQueryRequest);
  rndisRequest->message.queryRequest.infoBufferLength = 0;
  rndisRequest->message.queryRequest.deviceVcHandle   = 0;
  
  DBGLOG("OID query request offset 0x%X, length 0x%X",
         rndisRequest->message.queryRequest.infoBufferOffset, rndisRequest->message.queryRequest.infoBufferLength);
  
  bool result = sendRNDISMessage(rndisRequest);
  if (result) {
    DBGLOG("OID query response status 0x%X, offset 0x%X, length 0x%X",
           rndisRequest->message.queryComplete.status,
           rndisRequest->message.queryComplete.infoBufferOffset, rndisRequest->message.queryComplete.infoBufferLength);
    
    memcpy(value, (UInt8*)(&rndisRequest->message.queryComplete) + rndisRequest->message.queryComplete.infoBufferOffset, rndisRequest->message.queryComplete.infoBufferLength);
    *valueSize = rndisRequest->message.queryComplete.infoBufferLength;
  } else {
    SYSLOG("Failed to send OID 0x%X query", oid);
  }
  
  freeRNDISRequest(rndisRequest);
  return result;
}


