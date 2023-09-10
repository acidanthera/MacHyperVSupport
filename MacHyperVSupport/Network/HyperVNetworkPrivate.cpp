//
//  HyperVNetworkPrivate.cpp
//  Hyper-V network driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVNetwork.hpp"

void HyperVNetwork::handleTimer() {
  HVSYSLOG("Outstanding sends %u bytes %X %X %X stalls %llu", _sendIndexesOutstanding, preCycle, midCycle, postCycle, stalls);
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
  HVDATADBGLOG("Received %u RNDIS ranges, range[0] count = %u, offset = 0x%X", pktPages->rangeCount, pktPages->ranges[0].count, pktPages->ranges[0].offset);
  
  //
  // Process each range which contains a packet.
  //
  for (int i = 0; i < pktPages->rangeCount; i++) {
    UInt8 *data = ((UInt8*) _receiveBuffer.buffer) + pktPages->ranges[i].offset;
    UInt32 dataLength = pktPages->ranges[i].count;
    
    HVDATADBGLOG("Got range of %u bytes at 0x%X", dataLength, pktPages->ranges[i].offset);
    processRNDISPacket(data, dataLength);
  }
  
  HyperVNetworkMessage netMsg2;
  memset(&netMsg2, 0, sizeof (netMsg2));
  netMsg2.messageType = kHyperVNetworkMessageTypeV1SendRNDISPacketComplete;
  netMsg2.v1.sendRNDISPacketComplete.status = kHyperVNetworkMessageStatusSuccess;
  
  _hvDevice->writeCompletionPacketWithTransactionId(&netMsg2, sizeof (netMsg2), pktPages->header.transactionId, false);
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

  if (_hvDevice->writeInbandPacket(&netMsg, sizeof (netMsg), true, &netMsg, sizeof (netMsg)) != kIOReturnSuccess) {
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

IOReturn HyperVNetwork::initSendReceiveBuffers() {
  IOReturn             status;
  HyperVNetworkMessage netMsg;
  
  //
  // Older versions of the protocol have a lower recieve buffer size limit.
  //
  _receiveBufferSize = (_netVersion > kHyperVNetworkProtocolVersion2) ?
    kHyperVNetworkReceiveBufferSize : kHyperVNetworkReceiveBufferSizeLegacy;
  _sendBufferSize    = kHyperVNetworkSendBufferSize;

  //
  // Allocate receive and send buffers and create GPADLs for them.
  //
  if (!_hvDevice->getHvController()->allocateDmaBuffer(&_receiveBuffer, _receiveBufferSize)) {
    HVSYSLOG("Failed to allocate receive buffer");
    freeSendReceiveBuffers();
    return kIOReturnNoResources;
  }
  if (_hvDevice->createGPADLBuffer(&_receiveBuffer, &_receiveGpadlHandle) != kIOReturnSuccess) {
    HVSYSLOG("Failed to create GPADL for receive buffer");
    freeSendReceiveBuffers();
    return kIOReturnIOError;
  }
  if (!_hvDevice->getHvController()->allocateDmaBuffer(&_sendBuffer, _sendBufferSize)) {
    HVSYSLOG("Failed to allocate send buffer");
    freeSendReceiveBuffers();
    return kIOReturnNoResources;
  }
  if (_hvDevice->createGPADLBuffer(&_sendBuffer, &_sendGpadlHandle) != kIOReturnSuccess) {
    HVSYSLOG("Failed to create GPADL for send buffer");
    freeSendReceiveBuffers();
    return kIOReturnIOError;
  }
  HVDBGLOG("Receive GPADL: 0x%X, send GPADL: 0x%X", _receiveGpadlHandle, _sendGpadlHandle);

  //
  // Configure Hyper-V Network with receive buffer GPADL.
  //
  bzero(&netMsg, sizeof (netMsg));
  netMsg.messageType                      = kHyperVNetworkMessageTypeV1SendReceiveBuffer;
  netMsg.v1.sendReceiveBuffer.gpadlHandle = _receiveGpadlHandle;
  netMsg.v1.sendReceiveBuffer.id          = kHyperVNetworkReceiveBufferID;

  status = _hvDevice->writeInbandPacket(&netMsg, sizeof (netMsg), true, &netMsg, sizeof (netMsg));
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to send receive buffer configuration with status 0x%X", status);
    freeSendReceiveBuffers();
    return status;
  }

  if (netMsg.v1.sendReceiveBufferComplete.status != kHyperVNetworkMessageStatusSuccess) {
    HVSYSLOG("Failed to configure receive buffer with status 0x%X", netMsg.v1.sendReceiveBufferComplete.status);
    freeSendReceiveBuffers();
    return kIOReturnIOError;
  }

  //
  // Only allow 1 receive section.
  // This is the same as what the Linux driver supports.
  //
  if (netMsg.v1.sendReceiveBufferComplete.numSections != 1 || netMsg.v1.sendReceiveBufferComplete.sections[0].offset != 0) {
    HVSYSLOG("Invalid receive buffer sections: %u", netMsg.v1.sendReceiveBufferComplete.numSections);
    freeSendReceiveBuffers();
    return kIOReturnUnsupported;
  }
  HVDBGLOG("Receive buffer configured at 0x%p", _receiveBuffer);

  //
  // Configure Hyper-V Network with send buffer GPADL.
  //
  bzero(&netMsg, sizeof (netMsg));
  netMsg.messageType                   = kHyperVNetworkMessageTypeV1SendSendBuffer;
  netMsg.v1.sendSendBuffer.gpadlHandle = _sendGpadlHandle;
  netMsg.v1.sendSendBuffer.id          = kHyperVNetworkSendBufferID;

  status = _hvDevice->writeInbandPacket(&netMsg, sizeof (netMsg), true, &netMsg, sizeof (netMsg));
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to send send buffer configuration with status 0x%X", status);
    freeSendReceiveBuffers();
    return status;
  }

  if (netMsg.v1.sendSendBufferComplete.status != kHyperVNetworkMessageStatusSuccess) {
    HVSYSLOG("Failed to configure send buffer with status 0x%X", netMsg.v1.sendSendBufferComplete.status);
    freeSendReceiveBuffers();
    return kIOReturnIOError;
  }

  //
  // Create bitmap for send section tracking.
  //
  _sendSectionSize        = netMsg.v1.sendSendBufferComplete.sectionSize;
  _sendSectionCount       = _sendBufferSize / _sendSectionSize;
  _sendIndexMapSize       = ((_sendSectionCount * sizeof (UInt32)) / 32) + sizeof (UInt32);
  _sendIndexMap           = (UInt32 *)IOMalloc(_sendIndexMapSize);
  _sendIndexesOutstanding = 0;
  if (_sendIndexMap == nullptr) {
    HVSYSLOG("Failed to allocate send index map");
    freeSendReceiveBuffers();
    return kIOReturnNoResources;
  }
  bzero(_sendIndexMap, _sendIndexMapSize);

  HVDBGLOG("Send buffer configured at 0x%p-0x%p with section size of %u bytes and %u sections",
           _sendBuffer.buffer, _sendBuffer.buffer + (_sendSectionSize * (_sendSectionCount - 1)),
           _sendSectionSize, _sendSectionCount);
  HVDBGLOG("Send index map size: %u bytes", _sendIndexMapSize);
  return kIOReturnSuccess;
}

void HyperVNetwork::freeSendReceiveBuffers() {
  //
  // Release and free receive buffer.
  //
  if (_receiveGpadlHandle != kHyperVGpadlNullHandle) {
    _hvDevice->freeGPADLBuffer(_receiveGpadlHandle);
    _receiveGpadlHandle = kHyperVGpadlNullHandle;
  }
  _hvDevice->getHvController()->freeDmaBuffer(&_receiveBuffer);

  //
  // Release and free send buffer.
  //
  if (_sendGpadlHandle != kHyperVGpadlNullHandle) {
    _hvDevice->freeGPADLBuffer(_sendGpadlHandle);
    _sendGpadlHandle = kHyperVGpadlNullHandle;
  }
  _hvDevice->getHvController()->freeDmaBuffer(&_sendBuffer);

  //
  // Free send section tracking bitmap.
  //
  if (_sendIndexMap != nullptr) {
    IOFree(_sendIndexMap, _sendIndexMapSize);
    _sendIndexMap = nullptr;
  }
}

UInt32 HyperVNetwork::getNextSendIndex() {
  for (UInt32 i = 0; i < _sendSectionCount; i++) {
    if (!sync_test_and_set_bit(i, _sendIndexMap)) {
      OSIncrementAtomic(&_sendIndexesOutstanding);
      return i;
    }
  }
  return kHyperVNetworkRNDISSendSectionIndexInvalid;
}

UInt32 HyperVNetwork::getFreeSendIndexCount() {
  UInt32 freeSendIndexCount = 0;
  for (UInt32 i = 0; i < _sendSectionCount; i++) {
    if ((_sendIndexMap[i / 32] & (i % 32)) == 0) {
      freeSendIndexCount++;
    }
  }
  return freeSendIndexCount;
}

void HyperVNetwork::releaseSendIndex(UInt32 sendIndex) {
  sync_change_bit(sendIndex, _sendIndexMap);
  OSDecrementAtomic(&_sendIndexesOutstanding);
}

bool HyperVNetwork::connectNetwork() {
  IOReturn status;
  
  // Negotiate max protocol version with Hyper-V.
  negotiateProtocol(kHyperVNetworkProtocolVersion1);
  _netVersion = kHyperVNetworkProtocolVersion1;
  
  // Send NDIS version.
  UInt32 ndisVersion = _netVersion > kHyperVNetworkProtocolVersion4 ?
    kHyperVNetworkNDISVersion6001E : kHyperVNetworkNDISVersion60001;
  
  HyperVNetworkMessage netMsg;
  memset(&netMsg, 0, sizeof (netMsg));
  netMsg.messageType = kHyperVNetworkMessageTypeV1SendNDISVersion;
  netMsg.v1.sendNDISVersion.major = (ndisVersion & 0xFFFF0000) >> 16;
  netMsg.v1.sendNDISVersion.minor = ndisVersion & 0x0000FFFF;
  
  if (_hvDevice->writeInbandPacket(&netMsg, sizeof (netMsg), false) != kIOReturnSuccess) {
    HVSYSLOG("failed to send NDIS version");
    return false;
  }
  
  status = initSendReceiveBuffers();
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to initialize send/receive buffers with status 0x%X", status);
    return false;
  }
  
  initializeRNDIS();
  
  createMediumDictionary();
  readMACAddress();
  updateLinkState(NULL);
  
  //UInt32 filter = 0x9;
  //setRNDISOID(kHyperVNetworkRNDISOIDGeneralCurrentPacketFilter, &filter, sizeof (filter));
  
  return true;
}

void HyperVNetwork::createMediumDictionary() {
  OSDictionary *mediumDict;
  
  //
  // Create medium dictionary.
  // Speed/duplex is irrelvant for Hyper-V, use basic auto settings.
  //
  mediumDict = OSDictionary::withCapacity(1);
  if (mediumDict == nullptr) {
    return;
  }
  
  _networkMedium = IONetworkMedium::medium(kIOMediumEthernetAuto | kIOMediumOptionFullDuplex, 0);
  if (_networkMedium == nullptr) {
    mediumDict->release();
    return;
  }
  
  IONetworkMedium::addMedium(mediumDict, _networkMedium);

  publishMediumDictionary(mediumDict);
  setCurrentMedium(_networkMedium);
  
  mediumDict->release();
}

bool HyperVNetwork::readMACAddress() {
  UInt32 macSize = sizeof (_ethAddress.bytes);
  if (getRNDISOID(kHyperVNetworkRNDISOIDEthernetPermanentAddress, (void *)_ethAddress.bytes, &macSize) != kIOReturnSuccess) {
    HVSYSLOG("Failed to get MAC address");
    return false;
  }
  
  HVDBGLOG("MAC address is %02X:%02X:%02X:%02X:%02X:%02X",
           _ethAddress.bytes[0], _ethAddress.bytes[1], _ethAddress.bytes[2],
           _ethAddress.bytes[3], _ethAddress.bytes[4], _ethAddress.bytes[5]);
  return true;
}

void HyperVNetwork::updateLinkState(HyperVNetworkRNDISMessageIndicateStatus *indicateStatus) {
  //
  // Pull initial link state from OID.
  //
  if (indicateStatus == NULL) {
    HyperVNetworkRNDISLinkState linkState;
    UInt32 linkStateSize = sizeof (linkState);
    if (getRNDISOID(kHyperVNetworkRNDISOIDGeneralMediaConnectStatus, &linkState, &linkStateSize) != kIOReturnSuccess) {
      HVSYSLOG("Failed to get link state");
      return;
    }

    HVDBGLOG("Link state is initially %s", linkState == kHyperVNetworkRNDISLinkStateConnected ? "up" : "down");
    _isLinkUp = linkState == kHyperVNetworkRNDISLinkStateConnected;
    if (_isLinkUp) {
      setLinkStatus(kIONetworkLinkValid | kIONetworkLinkActive, _networkMedium);
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
      // Ignore.
      break;

    case kHyperVNetworkRNDISStatusMediaConnect:
      if (!_isLinkUp) {
        HVDBGLOG("Link is coming up");
        setLinkStatus(kIONetworkLinkValid | kIONetworkLinkActive, _networkMedium);
        _isLinkUp = true;
      }
      break;

    case kHyperVNetworkRNDISStatusMediaDisconnect:
      if (_isLinkUp) {
        HVDBGLOG("Link is going down");
        setLinkStatus(kIONetworkLinkValid, 0);
        _isLinkUp = false;
      }
      break;

    case kHyperVNetworkRNDISStatusNetworkChange:
      if (_isLinkUp) {
        //
        // Do a link up and down to force a refresh in the OS.
        //
        HVDBGLOG("Link has changed networks");
        setLinkStatus(kIONetworkLinkValid, 0);
        setLinkStatus(kIONetworkLinkValid | kIONetworkLinkActive, _networkMedium);
      }
      break;

    default:
      break;
  }
}
