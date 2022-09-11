//
//  HyperVNetworkPrivate.cpp
//  Hyper-V network driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVNetwork.hpp"

void HyperVNetwork::handleTimer() {
  HVSYSLOG("outst %X r %X w %X bytes %X %X %X stalls %llu", outstandingSends, hvDevice->rxBufferReadCount, hvDevice->txBufferWriteCount, preCycle, midCycle, postCycle, stalls);
}

bool HyperVNetwork::wakePacketHandler(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  return pktHeader->type == kVMBusPacketTypeCompletion;
}

void HyperVNetwork::handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  //
  // Handle inbound packet.
  //
  totalbytes += pktHeaderLength + pktDataLength + 8;
  switch (pktHeader->type) {
    case kVMBusPacketTypeDataInband:
      break;
    case kVMBusPacketTypeDataUsingTransferPages:
      handleRNDISRanges((VMBusPacketTransferPages*)pktHeader, pktHeaderLength + pktDataLength);
      break;
      
    case kVMBusPacketTypeCompletion:
      handleCompletion(pktHeader, pktHeaderLength + pktDataLength);
      break;
    default:
      HVSYSLOG("Invalid packet type %X", pktHeader->type);
      break;
  }
}

void HyperVNetwork::handleRNDISRanges(VMBusPacketTransferPages *pktPages, UInt32 pktSize) {
  UInt32 pktHeaderSize = HV_GET_VMBUS_PACKETSIZE(pktPages->header.headerLength);
  
  HyperVNetworkMessage *netMsg = (HyperVNetworkMessage*) (((UInt8*)pktPages) + pktHeaderSize);
  
  //
  // Ensure packet is valid.
  //
  if (netMsg->messageType != kHyperVNetworkMessageTypeV1SendRNDISPacket ||
      pktPages->transferPagesetId != kHyperVNetworkReceiveBufferID) {
    HVSYSLOG("Invalid message of type 0x%X and pageset ID of 0x%X received", netMsg->messageType, pktPages->transferPagesetId);
    return;
  }
  HVDBGLOG("Received %u RNDIS ranges, range[0] count = %u, offset = 0x%X", pktPages->rangeCount, pktPages->ranges[0].count, pktPages->ranges[0].offset);
  
  //
  // Process each range which contains a packet.
  //
  for (int i = 0; i < pktPages->rangeCount; i++) {
    UInt8 *data = ((UInt8*) receiveBuffer.buffer) + pktPages->ranges[i].offset;
    UInt32 dataLength = pktPages->ranges[i].count;
    
    HVDBGLOG("Got range of %u bytes at 0x%X", dataLength, pktPages->ranges[i].offset);
    processRNDISPacket(data, dataLength);
  }
  
  HyperVNetworkMessage netMsg2;
  memset(&netMsg2, 0, sizeof (netMsg2));
  netMsg2.messageType = kHyperVNetworkMessageTypeV1SendRNDISPacketComplete;
  netMsg2.v1.sendRNDISPacketComplete.status = kHyperVNetworkMessageStatusSuccess;
  
  hvDevice->writeCompletionPacketWithTransactionId(&netMsg2, sizeof (netMsg2), pktPages->header.transactionId, false);
 // postCycle++;
}

void HyperVNetwork::handleCompletion(void *pktData, UInt32 pktLength) {
  VMBusPacketHeader *pktHeader = (VMBusPacketHeader*)pktData;
  UInt32 pktHeaderSize = HV_GET_VMBUS_PACKETSIZE(pktHeader->headerLength);
  
  HyperVNetworkMessage *netMsg;
    netMsg = (HyperVNetworkMessage*)(((UInt8*)pktData) + pktHeaderSize);
    if (netMsg->messageType == kHyperVNetworkMessageTypeV1SendRNDISPacketComplete) {
    //  completions++;
      //HVSYSLOG("Got index %X", pktHeader->transactionId & ~kHyperVNetworkSendTransIdBits);
     // if (completions % 100000 == 0)
     // HVSYSLOG("pkt completion status %X %X %u comple trans %u (free indexes %u)", netMsg->messageType, netMsg->v1.sendRNDISPacketComplete.status, completions, pktHeader->transactionId, getFreeSendIndexCount());
      
      if (netMsg->v1.sendRNDISPacketComplete.status != 1) {
        HVSYSLOG("Got a nonsuccess %u", netMsg->v1.sendRNDISPacketComplete.status);
      }
      //HyperVSendPacketThing *pktThing = (HyperVSendPacketThing*)pktHeader->transactionId;
      releaseSendIndex((UInt32)(pktHeader->transactionId & ~kHyperVNetworkSendTransIdBits));
    } else {
      HVSYSLOG("Unknown completion type 0x%X received", netMsg->messageType);
    }
}

bool HyperVNetwork::negotiateProtocol(HyperVNetworkProtocolVersion protocolVersion) {
  // Send requested version to Hyper-V.
  HyperVNetworkMessage netMsg;
  memset(&netMsg, 0, sizeof (netMsg));
  netMsg.messageType = kHyperVNetworkMessageTypeInit;
  netMsg.init.initVersion.maxProtocolVersion = protocolVersion;
  netMsg.init.initVersion.minProtocolVersion = protocolVersion;

  if (hvDevice->writeInbandPacket(&netMsg, sizeof (netMsg), true, &netMsg, sizeof (netMsg)) != kIOReturnSuccess) {
    HVSYSLOG("failed to send protocol negotiation message");
    return false;
  }

  if (netMsg.init.initComplete.status != kHyperVNetworkMessageStatusSuccess) {
    HVSYSLOG("error returned from Hyper-V: 0x%X", netMsg.init.initComplete.status);
  }

  HVDBGLOG("Can use protocol 0x%X, max MDL length %u",
         netMsg.init.initComplete.negotiatedProtocolVersion, netMsg.init.initComplete.maxMdlChainLength);
  return true;
}

bool HyperVNetwork::initBuffers() {
  
  // Allocate receive and send buffers.
  hvDevice->allocateDmaBuffer(&receiveBuffer, receiveBufferSize);
  if (hvDevice->createGPADLBuffer(&receiveBuffer, &receiveGpadlHandle) != kIOReturnSuccess) {
    HVSYSLOG("Failed to create GPADL for receive buffer");
    return false;
  }
  hvDevice->allocateDmaBuffer(&sendBuffer, sendBufferSize);
  if (hvDevice->createGPADLBuffer(&sendBuffer, &sendGpadlHandle) != kIOReturnSuccess) {
    HVSYSLOG("Failed to create GPADL for send buffer");
    return false;
  }
  HVDBGLOG("Receive GPADL: 0x%X, send GPADL: 0x%X", receiveGpadlHandle, sendGpadlHandle);
  
  // Send receive buffer GPADL handle to Hyper-V.
  HyperVNetworkMessage netMsg;
  memset(&netMsg, 0, sizeof (netMsg));
  netMsg.messageType = kHyperVNetworkMessageTypeV1SendReceiveBuffer;
  netMsg.v1.sendReceiveBuffer.gpadlHandle = receiveGpadlHandle;
  netMsg.v1.sendReceiveBuffer.id = kHyperVNetworkReceiveBufferID;
  
  if (hvDevice->writeInbandPacket(&netMsg, sizeof (netMsg), true, &netMsg, sizeof (netMsg)) != kIOReturnSuccess) {
    HVSYSLOG("Failed to send receive buffer configuration message");
    return false;
  }

  if (netMsg.v1.sendReceiveBufferComplete.status != kHyperVNetworkMessageStatusSuccess) {
    HVSYSLOG("Failed to configure receive buffer: 0x%X", netMsg.v1.sendReceiveBufferComplete.status);
    return false;
  }
  HVDBGLOG("Receive buffer configured at 0x%p with %u sections", receiveBuffer, netMsg.v1.sendReceiveBufferComplete.numSections);
  
  // Linux driver only allows 1 section.
  if (netMsg.v1.sendReceiveBufferComplete.numSections != 1 || netMsg.v1.sendReceiveBufferComplete.sections[0].offset != 0) {
    HVSYSLOG("Invalid receive buffer sections");
    return false;
  }
  
  // Send send buffer GPADL handle to Hyper-V.
  memset(&netMsg, 0, sizeof (netMsg));
  netMsg.messageType = kHyperVNetworkMessageTypeV1SendSendBuffer;
  netMsg.v1.sendSendBuffer.gpadlHandle = sendGpadlHandle;
  netMsg.v1.sendSendBuffer.id = kHyperVNetworkSendBufferID;

  if (hvDevice->writeInbandPacket(&netMsg, sizeof (netMsg), true, &netMsg, sizeof (netMsg)) != kIOReturnSuccess) {
    HVSYSLOG("Failed to send send buffer configuration message");
    return false;
  }

  if (netMsg.v1.sendSendBufferComplete.status != kHyperVNetworkMessageStatusSuccess) {
    HVSYSLOG("Failed to configure send buffer: 0x%X", netMsg.v1.sendSendBufferComplete.status);
    return false;
  }
  sendSectionSize = netMsg.v1.sendSendBufferComplete.sectionSize;
  sendSectionCount = sendBufferSize / sendSectionSize;
  sendIndexMapSize = ((sendSectionCount * sizeof (UInt32)) / 32) + sizeof (UInt32);
  sendIndexMap = (UInt32*)IOMalloc(sendIndexMapSize);
  bzero(sendIndexMap, sendIndexMapSize);

  HVDBGLOG("Send buffer configured at 0x%p-0x%p with section size of %u bytes and %u sections",
           sendBuffer.buffer, (((UInt8*) sendBuffer.buffer) + (sendSectionSize * (sendSectionCount - 1))), sendSectionSize, sendSectionCount);
  HVDBGLOG("Send index map size: %u bytes", sendIndexMapSize);
  
  return true;
}

bool HyperVNetwork::connectNetwork() {
  HVDBGLOG("start");
  
  // Negotiate max protocol version with Hyper-V.
  negotiateProtocol(kHyperVNetworkProtocolVersion1);
  netVersion = kHyperVNetworkProtocolVersion1;
  
  // Send NDIS version.
  UInt32 ndisVersion = netVersion > kHyperVNetworkProtocolVersion4 ?
    kHyperVNetworkNDISVersion6001E : kHyperVNetworkNDISVersion60001;
  
  HyperVNetworkMessage netMsg;
  memset(&netMsg, 0, sizeof (netMsg));
  netMsg.messageType = kHyperVNetworkMessageTypeV1SendNDISVersion;
  netMsg.v1.sendNDISVersion.major = (ndisVersion & 0xFFFF0000) >> 16;
  netMsg.v1.sendNDISVersion.minor = ndisVersion & 0x0000FFFF;
  
  if (hvDevice->writeInbandPacket(&netMsg, sizeof (netMsg), false) != kIOReturnSuccess) {
    HVSYSLOG("failed to send NDIS version");
    return false;
  }
  
  receiveBufferSize = netVersion > kHyperVNetworkProtocolVersion2 ?
    kHyperVNetworkReceiveBufferSize : kHyperVNetworkReceiveBufferSizeLegacy;
  sendBufferSize = kHyperVNetworkSendBufferSize;
  initBuffers();
  
  initializeRNDIS();
  
  readMACAddress();
  updateLinkState(NULL);
  
  //UInt32 filter = 0x9;
  //setRNDISOID(kHyperVNetworkRNDISOIDGeneralCurrentPacketFilter, &filter, sizeof (filter));
  
  return true;
}

void HyperVNetwork::addNetworkMedium(UInt32 index, UInt32 type, UInt32 speed) {
  IONetworkMedium *medium = IONetworkMedium::medium(type, speed * MBit, 0, index);
  if (medium != NULL) {
    IONetworkMedium::addMedium(mediumDict, medium);
    medium->release();
  }
}

void HyperVNetwork::createMediumDictionary() {
  //
  // Create medium dictionary with all possible speeds.
  //
  mediumDict = OSDictionary::withCapacity(1);
  
  addNetworkMedium(0, kIOMediumEthernetAuto, 0);
  
  publishMediumDictionary(mediumDict);
}

bool HyperVNetwork::readMACAddress() {
  UInt32 macSize = sizeof (ethAddress.bytes);
  if (!queryRNDISOID(kHyperVNetworkRNDISOIDEthernetPermanentAddress, (void *)ethAddress.bytes, &macSize)) {
    HVSYSLOG("Failed to get MAC address");
    return false;
  }
  
  HVDBGLOG("MAC address is %02X:%02X:%02X:%02X:%02X:%02X",
         ethAddress.bytes[0], ethAddress.bytes[1], ethAddress.bytes[2],
         ethAddress.bytes[3], ethAddress.bytes[4], ethAddress.bytes[5]);
  return true;
}

void HyperVNetwork::updateLinkState(HyperVNetworkRNDISMessageIndicateStatus *indicateStatus) {
  //
  // Pull initial link state from OID.
  //
  if (indicateStatus == NULL) {
    HyperVNetworkRNDISLinkState linkState;
    UInt32 linkStateSize = sizeof (linkState);
    if (!queryRNDISOID(kHyperVNetworkRNDISOIDGeneralMediaConnectStatus, &linkState, &linkStateSize)) {
      HVSYSLOG("Failed to get link state");
      return;
    }

    HVDBGLOG("Link state is initially %s", linkState == kHyperVNetworkRNDISLinkStateConnected ? "up" : "down");
    isLinkUp = linkState == kHyperVNetworkRNDISLinkStateConnected;
    if (isLinkUp) {
      setLinkStatus(kIONetworkLinkValid | kIONetworkLinkActive, 0);
    } else {
      setLinkStatus(kIONetworkLinkValid, 0);
    }
    return;
  }

  //
  // Handle media and link speed changes.
  //
  HVDBGLOG("Indicate status of 0x%X, buffer off 0x%X of %u bytes received",
         indicateStatus->status, indicateStatus->statusBufferOffset, indicateStatus->statusBufferLength);
  switch (indicateStatus->status) {
    case kHyperVNetworkRNDISStatusLinkSpeedChange:
      HVDBGLOG("Link has changed speeds");
      break;

    case kHyperVNetworkRNDISStatusMediaConnect:
      if (!isLinkUp) {
        HVDBGLOG("Link is coming up");
        setLinkStatus(kIONetworkLinkValid | kIONetworkLinkActive, 0);
        isLinkUp = true;
      }
      break;

    case kHyperVNetworkRNDISStatusMediaDisconnect:
      if (isLinkUp) {
        HVDBGLOG("Link is going down");
        setLinkStatus(kIONetworkLinkValid, 0);
        isLinkUp = false;
      }
      break;

    case kHyperVNetworkRNDISStatusNetworkChange:
      if (isLinkUp) {
        //
        // Do a link up and down to force a refresh in the OS.
        //
        HVDBGLOG("Link has changed networks");
        setLinkStatus(kIONetworkLinkValid, 0);
        setLinkStatus(kIONetworkLinkValid | kIONetworkLinkActive, 0);
      }
      break;

    default:
      break;
  }
}
