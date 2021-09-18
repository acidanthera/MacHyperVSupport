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
        DBGLOG("checking %u", reqCurr->message.initComplete.requestId);
        if (reqCurr->message.initComplete.requestId == rndisPkt->initComplete.requestId) {
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
      if (isEnabled)
        processIncoming(data, dataLength);
      break;
      
    case kHyperVNetworkRNDISMessageTypeIndicate:
      updateLinkState(&rndisPkt->indicateStatus);
      break;
      
    default:
      break;
  }
  
  return true;
}

void HyperVNetwork::processIncoming(UInt8 *data, UInt32 dataLength) {
  HyperVNetworkRNDISMessage *rndisPkt = (HyperVNetworkRNDISMessage*)data;
  UInt8 *pktData = data + 8 + rndisPkt->dataPacket.dataOffset;
  
  mbuf_t newPacket = allocatePacket(rndisPkt->dataPacket.dataLength);
  memcpy(mbuf_data(newPacket), pktData, rndisPkt->dataPacket.dataLength);
  
  ethInterface->inputPacket(newPacket, rndisPkt->dataPacket.dataLength);
}

UInt32 HyperVNetwork::getNextSendIndex() {
  for (UInt32 i = 0; i < sendSectionCount; i++) {
    DBGLOG("idx %u %X", i, sendIndexMap[i / 8]);
    if ((sendIndexMap[i / 8] & (1 << (i % 8))) == 0) {
      sendIndexMap[i / 8] |= (1 << (i % 8));
      return i;
    }
  }

  return kHyperVNetworkRNDISSendSectionIndexInvalid;
}

void HyperVNetwork::releaseSendIndex(UInt32 sendIndex) {
  sendIndexMap[sendIndex / 8] &= ~(1 << (sendIndex % 8));
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
  DBGLOG("Mapped RNDIS request buffer 0x%llX to phys 0x%llX", rndisRequest, rndisRequest->messagePhysicalAddress);
  
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

bool HyperVNetwork::sendRNDISRequest(HyperVNetworkRNDISRequest *rndisRequest, bool waitResponse) {
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
  
  rndisRequest->isSleeping = true;
  rndisRequest->message.initRequest.requestId = getNextRNDISTransId();
  
  //
  // Add to linked list.
  //
  IOLockLock(rndisLock);
  if (rndisRequests == NULL)
    rndisRequests = rndisRequest;
  else
    rndisRequests->next = rndisRequest;
  IOLockUnlock(rndisLock);
  
  hvDevice->writeGPADirectSinglePagePacket(&netMsg, sizeof (netMsg), true, &pageBuffer, 1, &netMsg, sizeof (netMsg));
  
  IOLockLock(rndisRequest->lock);
  while (rndisRequest->isSleeping) {
    IOLockSleep(rndisRequest->lock, &rndisRequest->isSleeping, THREAD_INTERRUPTIBLE);
  }
  IOLockUnlock(rndisRequest->lock);
  
  DBGLOG("woke");
  
  return true;
}

bool HyperVNetwork::sendRNDISDataPacket(mbuf_t packet) {
  size_t packetLength = mbuf_pkthdr_len(packet);
  
  UInt32 sendIndex = getNextSendIndex();
  UInt8 *rndisBuffer = sendBuffer + (sendSectionSize * sendIndex);
  HyperVNetworkRNDISMessage *rndisMsg = (HyperVNetworkRNDISMessage*)rndisBuffer;
  memset(rndisMsg, 0, sizeof (HyperVNetworkRNDISMessage));
  
  rndisMsg->msgType = kHyperVNetworkRNDISMessageTypePacket;
  rndisMsg->dataPacket.dataOffset = sizeof (HyperVNetworkRNDISMessageDataPacket);
  rndisMsg->dataPacket.dataLength = (UInt32)packetLength;
  rndisMsg->msgLength = sizeof (HyperVNetworkRNDISMessageDataPacket) + 8 + rndisMsg->dataPacket.dataLength;
  
  rndisBuffer += rndisMsg->dataPacket.dataOffset + 8;
  for (mbuf_t pktCurrent = packet; pktCurrent != NULL; pktCurrent = mbuf_next(pktCurrent)) {
    size_t pktCurrentLength = mbuf_len(pktCurrent);
    memcpy(rndisBuffer, mbuf_data(pktCurrent), pktCurrentLength);
    rndisBuffer += pktCurrentLength;
  }
  freePacket(packet);
  
  //
  // Create packet for sending the RNDIS data packet.
  //
  HyperVNetworkMessage netMsg;
  memset(&netMsg, 0, sizeof (netMsg));
  netMsg.messageType = kHyperVNetworkMessageTypeV1SendRNDISPacket;
  netMsg.v1.sendRNDISPacket.channelType = kHyperVNetworkRNDISChannelTypeData;
  netMsg.v1.sendRNDISPacket.sendBufferSectionIndex = sendIndex;
  netMsg.v1.sendRNDISPacket.sendBufferSectionSize = rndisMsg->msgLength;
  
  DBGLOG("Packet at index %u, size %u bytes", sendIndex, rndisMsg->msgLength);
  hvDevice->writeInbandPacketWithTransactionId(&netMsg, sizeof (netMsg), sendIndex | kHyperVNetworkSendTransIdBits, true);
  
  return true;
}

bool HyperVNetwork::initializeRNDIS() {
  HyperVNetworkRNDISRequest *rndisRequest = allocateRNDISRequest();
  rndisRequest->message.msgType   = kHyperVNetworkRNDISMessageTypeInit;
  rndisRequest->message.msgLength = sizeof(HyperVNetworkRNDISMessageInitializeRequest) + 8;
  
  rndisRequest->message.initRequest.majorVersion    = kHyperVNetworkRNDISVersionMajor;
  rndisRequest->message.initRequest.minorVersion    = kHyperVNetworkRNDISVersionMinor;
  rndisRequest->message.initRequest.maxTransferSize = kHyperVNetworkRNDISMaxTransferSize;
  
  bool result = sendRNDISRequest(rndisRequest);
  if (result) {
    DBGLOG("RNDIS initializated with status 0x%X, max packets per msg %u, max transfer size 0x%X, packet alignment 0x%X",
           rndisRequest->message.initComplete.status, rndisRequest->message.initComplete.maxPacketsPerMessage,
           rndisRequest->message.initComplete.maxTransferSize, rndisRequest->message.initComplete.packetAlignmentFactor);
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
  
  DBGLOG("OID query 0x%X request offset 0x%X, length 0x%X", oid,
         rndisRequest->message.queryRequest.infoBufferOffset, rndisRequest->message.queryRequest.infoBufferLength);
  
  bool result = sendRNDISRequest(rndisRequest);
  if (result) {
    DBGLOG("OID query 0x%X response status 0x%X, offset 0x%X, length 0x%X", oid,
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


