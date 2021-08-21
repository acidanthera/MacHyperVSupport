//
//  HyperVNetworkPrivate.cpp
//  Hyper-V network driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVNetwork.hpp"

void HyperVNetwork::handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count) {
  DBGLOG("Interrupt");
  
  VMBusPacketType type;
  UInt32 headersize;
  UInt32 totalsize;
  
  while (true) {
    if (!hvDevice->nextPacketAvailable(&type, &headersize, &totalsize)) {
      DBGLOG("last one");
      break;
    }
    DBGLOG("Packet type %X, header size %X, total size %X", type, headersize, totalsize);
    
    void *buf = IOMalloc(totalsize);
    hvDevice->readRawPacket(buf, totalsize);
    
    switch (type) {
      case kVMBusPacketTypeDataInband:
        break;
      case kVMBusPacketTypeDataUsingTransferPages:
        handleRNDISRanges((VMBusPacketTransferPages*) buf, headersize, totalsize);
        break;
        
      case kVMBusPacketTypeCompletion:
        
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
  
  hvDevice->sendMessage(&netMsg2, sizeof (netMsg2), kVMBusPacketTypeDataInband, pktPages->header.transactionId);
  DBGLOG("sent completion");
}

bool HyperVNetwork::negotiateProtocol(HyperVNetworkProtocolVersion protocolVersion) {
  // Send requested version to Hyper-V.
  HyperVNetworkMessage netMsg;
  memset(&netMsg, 0, sizeof (netMsg));
  netMsg.messageType = kHyperVNetworkMessageTypeInit;
  netMsg.init.initVersion.maxProtocolVersion = protocolVersion;
  netMsg.init.initVersion.minProtocolVersion = protocolVersion;

  UInt32 msgSize = sizeof (netMsg);
  if (hvDevice->sendMessage(&netMsg, sizeof (netMsg), kVMBusPacketTypeDataInband, 0, true, &netMsg, &msgSize) != kIOReturnSuccess) {
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
  if (!hvDevice->createGpadlBuffer(sendBufferSize, &sendGpadlHandle, &sendBuffer)) {
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
  
  UInt32 respLength = sizeof (netMsg);
  if (hvDevice->sendMessage(&netMsg, sizeof (netMsg), kVMBusPacketTypeDataInband, 0, true, &netMsg, &respLength) != kIOReturnSuccess) {
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

  respLength = sizeof (netMsg);
  if (hvDevice->sendMessage(&netMsg, sizeof (netMsg), kVMBusPacketTypeDataInband, 0, true, &netMsg, &respLength) != kIOReturnSuccess) {
    SYSLOG("Failed to send send buffer configuration message");
    return false;
  }

  if (netMsg.v1.sendSendBufferComplete.status != kHyperVNetworkMessageStatusSuccess) {
    SYSLOG("Failed to configure send buffer: 0x%X", netMsg.v1.sendSendBufferComplete.status);
    return false;
  }
  sendSectionSize = netMsg.v1.sendSendBufferComplete.sectionSize;
  sendSectionCount = sendBufferSize / sendSectionSize;
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
  
  if (hvDevice->sendMessage(&netMsg, sizeof (netMsg), kVMBusPacketTypeDataInband, 0) != kIOReturnSuccess) {
    SYSLOG("failed to send NDIS version");
    return false;
  }
  
  receiveBufferSize = netVersion > kHyperVNetworkProtocolVersion2 ?
    kHyperVNetworkReceiveBufferSize : kHyperVNetworkReceiveBufferSizeLegacy;
  sendBufferSize = kHyperVNetworkSendBufferSize;
  initBuffers();
  
  initializeRNDIS();
  
  UInt8 mac[6];
  UInt32 size = sizeof(mac);
  
  queryRNDISOID(kHyperVNetworkRNDISOIDEthernetPermanentAddress, mac, &size);
  DBGLOG("RNDIS MAC %X:%X:%X:%X:%X:%X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  return true;
}
