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
  
  HVDATADBGLOG("New RNDIS packet of type 0x%X and %u bytes", rndisPkt->header.type, rndisPkt->header.length);
  
  HyperVNetworkRNDISRequest *reqCurr = rndisRequests;
  HyperVNetworkRNDISRequest *reqPrev = NULL;
  
  switch (rndisPkt->header.type) {
    case kHyperVNetworkRNDISMessageTypeInitComplete:
    case kHyperVNetworkRNDISMessageTypeGetOIDComplete:
    case kHyperVNetworkRNDISMessageTypeSetOIDComplete:
    case kHyperVNetworkRNDISMessageTypeResetComplete:

      
      while (reqCurr != NULL) {
        HVDATADBGLOG("checking %u", reqCurr->message.initComplete.requestId);
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
      if (_isNetworkEnabled) {
        
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
  
  _ethInterface->inputPacket(newPacket, rndisPkt->dataPacket.dataLength);
  postCycle++;
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
  if (!_hvDevice->getHvController()->allocateDmaBuffer(&dmaBuffer, sizeof (HyperVNetworkRNDISRequest) + additionalLength)) {
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
  _hvDevice->getHvController()->freeDmaBuffer(&rndisRequest->dmaBuffer);
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
  pageBuffer.length = rndisRequest->message.header.length;
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

bool HyperVNetwork::initializeRNDIS() {
  HyperVNetworkRNDISRequest *rndisRequest = allocateRNDISRequest();
  rndisRequest->message.header.type   = kHyperVNetworkRNDISMessageTypeInit;
  rndisRequest->message.header.length = sizeof (HyperVNetworkRNDISMessageInitializeRequest) + 8;
  
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

IOReturn HyperVNetwork::getRNDISOID(HyperVNetworkRNDISOID oid, void *value, UInt32 *valueSize) {
  HyperVNetworkRNDISRequest *rndisRequest;
  bool                      result;
  IOReturn                  status;

  if (value == nullptr || valueSize == nullptr) {
    return kIOReturnBadArgument;
  }

  //
  // Allocate RNDIS request.
  //
  rndisRequest = allocateRNDISRequest();
  if (rndisRequest == nullptr) {
    return kIOReturnNoResources;
  }

  //
  // Get specified RNDIS OID.
  //
  rndisRequest->message.header.type                    = kHyperVNetworkRNDISMessageTypeGetOID;
  rndisRequest->message.header.length                  = sizeof (rndisRequest->message.header) + sizeof (rndisRequest->message.getOIDRequest);
  rndisRequest->message.getOIDRequest.oid              = oid;
  rndisRequest->message.getOIDRequest.infoBufferOffset = sizeof (rndisRequest->message.getOIDRequest);
  rndisRequest->message.getOIDRequest.infoBufferLength = 0;
  rndisRequest->message.getOIDRequest.deviceVcHandle   = 0;

  HVDBGLOG("Get OID 0x%X, expecting %u bytes", oid, *valueSize);
  result = sendRNDISRequest(rndisRequest);
  if (result && rndisRequest->message.getOIDComplete.status == kHyperVNetworkRNDISStatusSuccess) {
    HVDBGLOG("Get OID 0x%X successful, %u bytes of data at offset 0x%X", oid,
             rndisRequest->message.getOIDComplete.infoBufferLength, rndisRequest->message.getOIDComplete.infoBufferOffset);

    //
    // Copy OID data from RNDIS request to buffer.
    //
    if (*valueSize >= rndisRequest->message.getOIDComplete.infoBufferLength) {
      memcpy(value, (UInt8*)(&rndisRequest->message.getOIDComplete) + rndisRequest->message.getOIDComplete.infoBufferOffset,
             rndisRequest->message.getOIDComplete.infoBufferLength);
      status = kIOReturnSuccess;
    } else {
      HVDBGLOG("OID value of %u bytes is too large for buffer of %u bytes", rndisRequest->message.getOIDComplete.infoBufferLength, *valueSize);
      status = kIOReturnMessageTooLarge;
    }
    *valueSize = rndisRequest->message.getOIDComplete.infoBufferLength;

  } else if (result) {
    HVSYSLOG("Failed to get OID 0x%X with status 0x%X", oid, rndisRequest->message.getOIDComplete.status);
    status = kIOReturnIOError;

  } else {
    HVSYSLOG("Failed to get OID 0x%X", oid);
    status = kIOReturnIOError;
  }

  freeRNDISRequest(rndisRequest);
  return status;
}

IOReturn HyperVNetwork::setRNDISOID(HyperVNetworkRNDISOID oid, void *value, UInt32 valueSize) {
  HyperVNetworkRNDISRequest *rndisRequest;
  bool                      result;
  IOReturn                  status;

  if (value == nullptr || valueSize == 0) {
    return kIOReturnBadArgument;
  }

  //
  // Allocate RNDIS request.
  //
  rndisRequest = allocateRNDISRequest(valueSize);
  if (rndisRequest == nullptr) {
    return kIOReturnNoResources;
  }

  //
  // Set specified RNDIS OID.
  //
  rndisRequest->message.header.type                    = kHyperVNetworkRNDISMessageTypeSetOID;
  rndisRequest->message.header.length                  = sizeof (rndisRequest->message.header) + sizeof (rndisRequest->message.setOIDRequest) + valueSize;
  rndisRequest->message.setOIDRequest.oid              = oid;
  rndisRequest->message.setOIDRequest.infoBufferOffset = sizeof (rndisRequest->message.setOIDRequest);
  rndisRequest->message.setOIDRequest.infoBufferLength = valueSize;
  rndisRequest->message.setOIDRequest.deviceVcHandle   = 0;
  
  //
  // Copy OID data from buffer to RNDIS request.
  //
  memcpy((UInt8*)(&rndisRequest->message.setOIDRequest) + rndisRequest->message.setOIDRequest.infoBufferOffset, value, valueSize);

  HVDBGLOG("Set OID 0x%X, %u bytes of data at offset 0x%X", oid,
           rndisRequest->message.setOIDRequest.infoBufferLength, rndisRequest->message.setOIDRequest.infoBufferOffset);
  result = sendRNDISRequest(rndisRequest);
  if (result && rndisRequest->message.setOIDComplete.status == kHyperVNetworkRNDISStatusSuccess) {
    HVDBGLOG("Set OID 0x%X successful, %u bytes of data", oid, valueSize);

    status = kIOReturnSuccess;

  } else if (result) {
    HVSYSLOG("Failed to set OID 0x%X with status 0x%X", oid, rndisRequest->message.setOIDComplete.status);
    status = kIOReturnIOError;

  } else {
    HVSYSLOG("Failed to set OID 0x%X", oid);
    status = kIOReturnIOError;
  }

  freeRNDISRequest(rndisRequest);
  return status;
}
