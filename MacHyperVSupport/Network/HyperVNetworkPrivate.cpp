//
//  HyperVNetworkPrivate.cpp
//  Hyper-V network driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVNetwork.hpp"

void HyperVNetwork::handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count) {
  VMBusPacketType type;
  UInt32 headersize;
  UInt32 totalsize;
  
  void *responseBuffer;
  UInt32 responseLength;
  
  HyperVNetworkMessage *pktComp;
  
  while (true) {
    if (!hvDevice->nextPacketAvailable(&type, &headersize, &totalsize)) {
     // DBGLOG("last one");
      break;
    }
    
    UInt8 *buf = (UInt8*)IOMalloc(totalsize);
    hvDevice->readRawPacket((void*)buf, totalsize);
    
    switch (type) {
      case kVMBusPacketTypeDataInband:
        break;
      case kVMBusPacketTypeDataUsingTransferPages:
        handleRNDISRanges((VMBusPacketTransferPages*) buf, headersize, totalsize);
        break;
        
      case kVMBusPacketTypeCompletion:
        
        if (hvDevice->getPendingTransaction(((VMBusPacketHeader*)buf)->transactionId, &responseBuffer, &responseLength)) {
          memcpy(responseBuffer, (UInt8*)buf + headersize, responseLength);
          hvDevice->wakeTransaction(((VMBusPacketHeader*)buf)->transactionId);
        } else {
          pktComp = (HyperVNetworkMessage*) (buf + headersize);
          DBGLOG("pkt completion status %X %X", pktComp->messageType, pktComp->v1.sendRNDISPacketComplete.status);
          
          if (pktComp->messageType == kHyperVNetworkMessageTypeV1SendRNDISPacketComplete) {
            releaseSendIndex((UInt32)(((VMBusPacketHeader*)buf)->transactionId & ~kHyperVNetworkSendTransIdBits));
          }
        }
        break;
      default:
        break;
    }
    
    IOFree(buf, totalsize);
  }
}

void HyperVNetwork::handleRNDISRanges(VMBusPacketTransferPages *pktPages, UInt32 headerSize, UInt32 pktSize) {
  HyperVNetworkMessage *netMsg = (HyperVNetworkMessage*) ((UInt8*)pktPages + headerSize);
  
  //
  // Ensure packet is valid.
  //
  if (netMsg->messageType != kHyperVNetworkMessageTypeV1SendRNDISPacket ||
      pktPages->transferPagesetId != kHyperVNetworkReceiveBufferID) {
    SYSLOG("Invalid message of type 0x%X and pageset ID of 0x%X received", netMsg->messageType, pktPages->transferPagesetId);
    return;
  }
  DBGLOG("Received %u RNDIS ranges, range[0] count = %u, offset = 0x%X", pktPages->rangeCount, pktPages->ranges[0].count, pktPages->ranges[0].offset);
  
  //
  // Process each range which contains a packet.
  //
  for (int i = 0; i < pktPages->rangeCount; i++) {
    UInt8 *data = receiveBuffer + pktPages->ranges[i].offset;
    UInt32 dataLength = pktPages->ranges[i].count;
    
    DBGLOG("Got range of %u bytes at 0x%X", dataLength, pktPages->ranges[i].offset);
    processRNDISPacket(data, dataLength);
  }
  
  HyperVNetworkMessage netMsg2;
  memset(&netMsg2, 0, sizeof (netMsg2));
  netMsg2.messageType = kHyperVNetworkMessageTypeV1SendRNDISPacketComplete;
  netMsg2.v1.sendRNDISPacketComplete.status = kHyperVNetworkMessageStatusSuccess;
  
  hvDevice->writeCompletionPacketWithTransactionId(&netMsg2, sizeof (netMsg2), pktPages->header.transactionId, false);
}

bool HyperVNetwork::negotiateProtocol(HyperVNetworkProtocolVersion protocolVersion) {
  // Send requested version to Hyper-V.
  HyperVNetworkMessage netMsg;
  memset(&netMsg, 0, sizeof (netMsg));
  netMsg.messageType = kHyperVNetworkMessageTypeInit;
  netMsg.init.initVersion.maxProtocolVersion = protocolVersion;
  netMsg.init.initVersion.minProtocolVersion = protocolVersion;

  if (hvDevice->writeInbandPacket(&netMsg, sizeof (netMsg), true, &netMsg, sizeof (netMsg)) != kIOReturnSuccess) {
    SYSLOG("failed to send protocol negotiation message");
    return false;
  }

  if (netMsg.init.initComplete.status != kHyperVNetworkMessageStatusSuccess) {
    SYSLOG("error returned from Hyper-V: 0x%X", netMsg.init.initComplete.status);
  }

  DBGLOG("Can use protocol 0x%X, max MDL length %u",
         netMsg.init.initComplete.negotiatedProtocolVersion, netMsg.init.initComplete.maxMdlChainLength);
  return true;
}

bool HyperVNetwork::initBuffers() {
  
  // Allocate receive and send buffers.
  if (!hvDevice->createGpadlBuffer(receiveBufferSize, &receiveGpadlHandle, (void**)&receiveBuffer)) {
    SYSLOG("Failed to create GPADL for receive buffer");
    return false;
  }
  if (!hvDevice->createGpadlBuffer(sendBufferSize, &sendGpadlHandle, (void**)&sendBuffer)) {
    SYSLOG("Failed to create GPADL for send buffer");
    return false;
  }
  DBGLOG("Receive GPADL: 0x%X, send GPADL: 0x%X", receiveGpadlHandle, sendGpadlHandle);
  
  // Send receive buffer GPADL handle to Hyper-V.
  HyperVNetworkMessage netMsg;
  memset(&netMsg, 0, sizeof (netMsg));
  netMsg.messageType = kHyperVNetworkMessageTypeV1SendReceiveBuffer;
  netMsg.v1.sendReceiveBuffer.gpadlHandle = receiveGpadlHandle;
  netMsg.v1.sendReceiveBuffer.id = kHyperVNetworkReceiveBufferID;
  
  if (hvDevice->writeInbandPacket(&netMsg, sizeof (netMsg), true, &netMsg, sizeof (netMsg)) != kIOReturnSuccess) {
    SYSLOG("Failed to send receive buffer configuration message");
    return false;
  }

  if (netMsg.v1.sendReceiveBufferComplete.status != kHyperVNetworkMessageStatusSuccess) {
    SYSLOG("Failed to configure receive buffer: 0x%X", netMsg.v1.sendReceiveBufferComplete.status);
    return false;
  }
  DBGLOG("Receive buffer configured with %u sections", netMsg.v1.sendReceiveBufferComplete.numSections);
  
  // Linux driver only allows 1 section.
  if (netMsg.v1.sendReceiveBufferComplete.numSections != 1 || netMsg.v1.sendReceiveBufferComplete.sections[0].offset != 0) {
    SYSLOG("Invalid receive buffer sections");
    return false;
  }
  
  // Send send buffer GPADL handle to Hyper-V.
  memset(&netMsg, 0, sizeof (netMsg));
  netMsg.messageType = kHyperVNetworkMessageTypeV1SendSendBuffer;
  netMsg.v1.sendSendBuffer.gpadlHandle = sendGpadlHandle;
  netMsg.v1.sendSendBuffer.id = kHyperVNetworkSendBufferID;

  if (hvDevice->writeInbandPacket(&netMsg, sizeof (netMsg), true, &netMsg, sizeof (netMsg)) != kIOReturnSuccess) {
    SYSLOG("Failed to send send buffer configuration message");
    return false;
  }

  if (netMsg.v1.sendSendBufferComplete.status != kHyperVNetworkMessageStatusSuccess) {
    SYSLOG("Failed to configure send buffer: 0x%X", netMsg.v1.sendSendBufferComplete.status);
    return false;
  }
  sendSectionSize = netMsg.v1.sendSendBufferComplete.sectionSize;
  sendSectionCount = sendBufferSize / sendSectionSize;
  sendIndexMapSize = (sendSectionCount + sizeof (UInt64) - 1) / sizeof (UInt64);
  sendIndexMap = (UInt64*)IOMalloc(sendIndexMapSize);
  memset(sendIndexMap, 0, sendIndexMapSize);
  DBGLOG("send index map size %u", sendIndexMapSize);
  
  DBGLOG("Send buffer configured with section size of %u bytes and %u sections", sendSectionSize, sendSectionCount);
  
  return true;
}

bool HyperVNetwork::connectNetwork() {
  DBGLOG("start");
  
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
    SYSLOG("failed to send NDIS version");
    return false;
  }
  
  receiveBufferSize = netVersion > kHyperVNetworkProtocolVersion2 ?
    kHyperVNetworkReceiveBufferSize : kHyperVNetworkReceiveBufferSizeLegacy;
  sendBufferSize = kHyperVNetworkSendBufferSize;
  initBuffers();
  
  initializeRNDIS();
  
  readMACAddress();
  updateLinkState(NULL);
  
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
    SYSLOG("Failed to get MAC address");
    return false;
  }
  
  DBGLOG("MAC address is %02X:%02X:%02X:%02X:%02X:%02X",
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
      SYSLOG("Failed to get link state");
      return;
    }

    DBGLOG("Link state is initially %s", linkState == kHyperVNetworkRNDISLinkStateConnected ? "up" : "down");
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
  DBGLOG("Indicate status of 0x%X, buffer off 0x%X of %u bytes received",
         indicateStatus->status, indicateStatus->statusBufferOffset, indicateStatus->statusBufferLength);
  switch (indicateStatus->status) {
    case kHyperVNetworkRNDISStatusLinkSpeedChange:
      DBGLOG("Link has changed speeds");
      break;

    case kHyperVNetworkRNDISStatusMediaConnect:
      if (!isLinkUp) {
        DBGLOG("Link is coming up");
        setLinkStatus(kIONetworkLinkValid | kIONetworkLinkActive, 0);
        isLinkUp = true;
      }
      break;

    case kHyperVNetworkRNDISStatusMediaDisconnect:
      if (isLinkUp) {
        DBGLOG("Link is going down");
        setLinkStatus(kIONetworkLinkValid, 0);
        isLinkUp = false;
      }
      break;

    case kHyperVNetworkRNDISStatusNetworkChange:
      if (isLinkUp) {
        //
        // Do a link up and down to force a refresh in the OS.
        //
        DBGLOG("Link has changed networks");
        setLinkStatus(kIONetworkLinkValid, 0);
        setLinkStatus(kIONetworkLinkValid | kIONetworkLinkActive, 0);
      }
      break;

    default:
      break;
  }
}
