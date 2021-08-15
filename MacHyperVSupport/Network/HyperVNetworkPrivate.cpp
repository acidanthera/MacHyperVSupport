//
//  HyperVNetworkPrivate.cpp
//  Hyper-V network driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVNetwork.hpp"

void HyperVNetwork::handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count) {
  DBGLOG("Interrupt");
  
  HyperVNetworkMessage netMsg;
  memset(&netMsg, 0, sizeof (netMsg));

  HyperVVMBusDeviceRequest request;
  request.sendData = NULL;
  request.responseRequired = false;
  request.sendDataLength = 0;
  request.responseData = &netMsg;
  request.responseDataLength = sizeof (netMsg);
  request.sendPacketType = kVMBusPacketTypeDataInband;
  
  hvDevice->doRequest(&request);
  DBGLOG("type %X", netMsg.messageType);
}

bool HyperVNetwork::allocateDmaBuffer(HyperVDMABuffer *dmaBuf, size_t size) {
  IOBufferMemoryDescriptor  *bufDesc;
  
  //
  // Create DMA buffer with required specifications and get physical address.
  //
  bufDesc = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
                                                             kIODirectionInOut | kIOMemoryPhysicallyContiguous | kIOMapInhibitCache | kIOMemoryMapperNone,
                                                             size, 0xFFFFFFFFFFFFF000ULL);
  if (bufDesc == NULL) {
    SYSLOG("Failed to allocate DMA buffer memory of %u bytes", size);
    return false;
  }
  bufDesc->prepare();
  
  dmaBuf->bufDesc  = bufDesc;
  dmaBuf->physAddr = bufDesc->getPhysicalAddress();
  dmaBuf->buffer   = bufDesc->getBytesNoCopy();
  dmaBuf->size     = size;
  
  memset(dmaBuf->buffer, 0, dmaBuf->size);
  DBGLOG("Mapped buffer of %u bytes to 0x%llX", dmaBuf->size, dmaBuf->physAddr);
  return true;
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
  if (!hvDevice->createGpadlBuffer(receiveBufferSize, &receiveGpadlHandle, &receiveBuffer)) {
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
  
  HyperVDMABuffer reqBuf;
  
  allocateDmaBuffer(&reqBuf, PAGE_SIZE);
  
  HyperVNetworkRNDISMessage *req = (HyperVNetworkRNDISMessage*)reqBuf.buffer;
  req->msgType = 2;
  req->msgLength = sizeof(HyperVNetworkRNDISMessageInitializeRequest) + 8;
  
  req->initRequest.majorVersion = 1;
  req->initRequest.minorVersion = 0;
  req->initRequest.maxTransferSize = 0x400;
  
  VMBusSinglePageBuffer pb2;
  pb2.length = req->msgLength;
  pb2.offset = 0;
  pb2.pfn = reqBuf.physAddr >> PAGE_SHIFT;
  
  memset(&netMsg, 0, sizeof (netMsg));
  netMsg.messageType = kHyperVNetworkMessageTypeV1SendRNDISPacket;
  netMsg.v1.sendRNDISPacket.channelType = 1;
  netMsg.v1.sendRNDISPacket.sendBufferSectionIndex = -1;
  netMsg.v1.sendRNDISPacket.sendBufferSectionSize = 0;
  
  UInt32 respSize = sizeof (netMsg);
  
  hvDevice->sendMessageSinglePageBuffers(&netMsg, sizeof (netMsg), 0, &pb2, 1, true, &netMsg, &respSize);
  
  DBGLOG("status rndis %X %X", netMsg.messageType, netMsg.v1.sendRNDISPacketComplete.status);
  
  return true;
}
