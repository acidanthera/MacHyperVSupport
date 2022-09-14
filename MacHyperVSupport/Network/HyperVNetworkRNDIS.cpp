//
//  HyperVNetworkPrivate.cpp
//  Hyper-V network driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVNetwork.hpp"

bool HyperVNetwork::processRNDISPacket(UInt8 *data, UInt32 dataLength) {
 // preCycle++;
  HyperVNetworkRNDISMessage *rndisPkt = (HyperVNetworkRNDISMessage*)data;
  
  HVDBGLOG("New RNDIS packet of type 0x%X and %u bytes", rndisPkt->msgType, rndisPkt->msgLength);
  
  HyperVNetworkRNDISRequest *reqCurr = rndisRequests;
  HyperVNetworkRNDISRequest *reqPrev = NULL;
  
  switch (rndisPkt->msgType) {
    case kHyperVNetworkRNDISMessageTypeInitComplete:
    case kHyperVNetworkRNDISMessageTypeQueryComplete:
    case kHyperVNetworkRNDISMessageTypeSetComplete:
    case kHyperVNetworkRNDISMessageTypeResetComplete:

      
      while (reqCurr != NULL) {
        HVDBGLOG("checking %u", reqCurr->message.initComplete.requestId);
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
        //  midCycle++;
          return true;
        }
        
        reqPrev = reqCurr;
        reqCurr = reqCurr->next;
      }
      break;
      
    case kHyperVNetworkRNDISMessageTypePacket:
      if (isEnabled) {
        
        processIncoming(data, dataLength);
        
      }
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
  
  preCycle++;
  mbuf_t newPacket = allocatePacket(rndisPkt->dataPacket.dataLength);
  if (newPacket == nullptr) {
    panic("zero packet mbuf");
  }
  midCycle++;
  //memcpy(mbuf_data(newPacket), pktData, rndisPkt->dataPacket.dataLength);
  mbuf_copyback(newPacket, 0, rndisPkt->dataPacket.dataLength, pktData, MBUF_WAITOK);
  
  ethInterface->inputPacket(newPacket, rndisPkt->dataPacket.dataLength);
  postCycle++;
}

UInt32 HyperVNetwork::getNextSendIndex() {
  for (UInt32 i = 0; i < sendSectionCount; i++) {
    if (!sync_test_and_set_bit(i, sendIndexMap)) {
      OSIncrementAtomic(&outstandingSends);
      return i;
    }
  }

  return kHyperVNetworkRNDISSendSectionIndexInvalid;
}

UInt32 HyperVNetwork::getFreeSendIndexCount() {
  UInt32 freeSendIndexCount = 0;
  for (UInt32 i = 0; i < sendSectionCount; i++) {
    if ((sendIndexMap[i / 32] & (i % 32)) == 0) {
      freeSendIndexCount++;
    }
  }
  return freeSendIndexCount;
}

void HyperVNetwork::releaseSendIndex(UInt32 sendIndex) {
  sync_change_bit(sendIndex, sendIndexMap);
  OSDecrementAtomic(&outstandingSends);
}

HyperVNetworkRNDISRequest* HyperVNetwork::allocateRNDISRequest(size_t additionalLength) {
  HyperVDMABuffer           dmaBuffer;
  HyperVNetworkRNDISRequest *rndisRequest;
  IOLock                    *lock;
  
  //
  // Allocate lock for waiting.
  //
  lock = IOLockAlloc();
  if (lock == NULL) {
    HVSYSLOG("Failed to allocate lock for RNDIS request");
    return NULL;
  }
  
  //
  // Create DMA buffer with required specifications and get physical address.
  //
  if (!_hvDevice->allocateDmaBuffer(&dmaBuffer, sizeof (HyperVNetworkRNDISRequest) + additionalLength)) {
    HVSYSLOG("Failed to allocate buffer memory for RNDIS request");
    IOLockFree(lock);
  }
  
  rndisRequest = (HyperVNetworkRNDISRequest*)dmaBuffer.buffer;
  memset(rndisRequest, 0, sizeof (HyperVNetworkRNDISRequest) + additionalLength);
  
  rndisRequest->lock = lock;
  rndisRequest->isSleeping = false;
  memcpy(&rndisRequest->dmaBuffer, &dmaBuffer, sizeof (rndisRequest->dmaBuffer));
  HVDBGLOG("Mapped RNDIS request buffer 0x%llX to phys 0x%llX", rndisRequest, rndisRequest->dmaBuffer.physAddr);
  
  return rndisRequest;
}

void HyperVNetwork::freeRNDISRequest(HyperVNetworkRNDISRequest *rndisRequest) {
  IOLockFree(rndisRequest->lock);
  _hvDevice->freeDmaBuffer(&rndisRequest->dmaBuffer);
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
  pageBuffer.pfn = rndisRequest->dmaBuffer.physAddr >> PAGE_SHIFT;
  
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
  
  _hvDevice->writeGPADirectSinglePagePacket(&netMsg, sizeof (netMsg), true, &pageBuffer, 1, &netMsg, sizeof (netMsg));
  
  IOLockLock(rndisRequest->lock);
  while (rndisRequest->isSleeping) {
    IOLockSleep(rndisRequest->lock, &rndisRequest->isSleeping, THREAD_INTERRUPTIBLE);
  }
  IOLockUnlock(rndisRequest->lock);
  
  HVDBGLOG("woke");
  
  return true;
}

bool HyperVNetwork::sendRNDISDataPacket(mbuf_t packet) {
  size_t packetLength = mbuf_pkthdr_len(packet);
  
  if (packetLength == 0 || packetLength > sendSectionSize) {
    HVSYSLOG("Too big! %u bytes", sendSectionSize);
  }
  
  UInt32 sendIndex = getNextSendIndex();
  if (sendIndex == kHyperVNetworkRNDISSendSectionIndexInvalid) {
    return false;
  }
  UInt8 *rndisBuffer = ((UInt8*)sendBuffer.buffer) + (sendSectionSize * sendIndex);
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
  
  //
  // Create packet for sending the RNDIS data packet.
  //
  HyperVNetworkMessage netMsg;
  memset(&netMsg, 0, sizeof (netMsg));
  netMsg.messageType = kHyperVNetworkMessageTypeV1SendRNDISPacket;
  netMsg.v1.sendRNDISPacket.channelType = kHyperVNetworkRNDISChannelTypeData;
  netMsg.v1.sendRNDISPacket.sendBufferSectionIndex = sendIndex;
  netMsg.v1.sendRNDISPacket.sendBufferSectionSize = rndisMsg->msgLength;
  
  HVDBGLOG("Packet at index %u, size %u bytes", sendIndex, rndisMsg->msgLength);
  if (_hvDevice->writeInbandPacketWithTransactionId(&netMsg, sizeof (netMsg), sendIndex | kHyperVNetworkSendTransIdBits, true) != kIOReturnSuccess) {
    HVSYSLOG("failure %p %p", packet, sendBuffer);
    return false;
  }
  
  freePacket(packet);
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
    HVDBGLOG("RNDIS initializated with status 0x%X, max packets per msg %u, max transfer size 0x%X, packet alignment 0x%X",
             rndisRequest->message.initComplete.status, rndisRequest->message.initComplete.maxPacketsPerMessage,
             rndisRequest->message.initComplete.maxTransferSize, rndisRequest->message.initComplete.packetAlignmentFactor);
    result = rndisRequest->message.initComplete.status == kHyperVNetworkRNDISStatusSuccess;
  } else {
    HVSYSLOG("Failed to send RNDIS initialization request");
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
  
  HVDBGLOG("OID query 0x%X request offset 0x%X, length 0x%X", oid,
           rndisRequest->message.queryRequest.infoBufferOffset, rndisRequest->message.queryRequest.infoBufferLength);
  
  bool result = sendRNDISRequest(rndisRequest);
  if (result) {
    HVDBGLOG("OID query 0x%X response status 0x%X, offset 0x%X, length 0x%X", oid,
             rndisRequest->message.queryComplete.status,
             rndisRequest->message.queryComplete.infoBufferOffset, rndisRequest->message.queryComplete.infoBufferLength);
    
    memcpy(value, (UInt8*)(&rndisRequest->message.queryComplete) + rndisRequest->message.queryComplete.infoBufferOffset, rndisRequest->message.queryComplete.infoBufferLength);
    *valueSize = rndisRequest->message.queryComplete.infoBufferLength;
  } else {
    HVSYSLOG("Failed to send OID 0x%X query", oid);
  }
  
  freeRNDISRequest(rndisRequest);
  return result;
}

bool HyperVNetwork::setRNDISOID(HyperVNetworkRNDISOID oid, void *value, UInt32 valueSize) {
  if (value == nullptr || valueSize == 0) {
    return false;
  }
  
  HyperVNetworkRNDISRequest *rndisRequest = allocateRNDISRequest(valueSize);
  rndisRequest->message.msgType   = kHyperVNetworkRNDISMessageTypeSet;
  rndisRequest->message.msgLength = sizeof(HyperVNetworkRNDISMessageQueryRequest) + 8 + valueSize;
  
  rndisRequest->message.setRequest.oid              = oid;
  rndisRequest->message.setRequest.infoBufferOffset = sizeof(HyperVNetworkRNDISMessageSetRequest);
  rndisRequest->message.setRequest.infoBufferLength = valueSize;
  rndisRequest->message.setRequest.deviceVcHandle   = 0;
  memcpy((UInt8*)(&rndisRequest->message.setRequest) + rndisRequest->message.setRequest.infoBufferOffset, value, rndisRequest->message.setRequest.infoBufferLength);
  
  HVDBGLOG("OID set 0x%X request offset 0x%X, length 0x%X", oid,
           rndisRequest->message.setRequest.infoBufferOffset, rndisRequest->message.setRequest.infoBufferLength);
  
  bool result = sendRNDISRequest(rndisRequest);
  if (result) {
    HVDBGLOG("OID set 0x%X response status 0x%X", oid, rndisRequest->message.setComplete.status);
  } else {
    HVSYSLOG("Failed to send OID 0x%X set", oid);
  }
  
  freeRNDISRequest(rndisRequest);
  return result;
}


