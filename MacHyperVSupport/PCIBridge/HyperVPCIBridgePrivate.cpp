//
//  HyperVPCIBridgePrivate.cpp
//  Hyper-V PCI passthrough device support
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVPCIBridge.hpp"

static HyperVPCIBridgeProtocolVersion usableVersions[] = {
  kHyperVPCIBridgeProtocolVersion1
};

void HyperVPCIBridge::handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count) {
  HVDBGLOG("interrupt");
  VMBusPacketType type;
  UInt32 headersize;
  UInt32 totalsize;
  
  void *responseBuffer;
  UInt32 responseLength;
  
  //HyperVNetworkMessage *pktComp;
  
  while (true) {
    if (!hvDevice->nextPacketAvailable(&type, &headersize, &totalsize)) {
      HVDBGLOG("last one");
      break;
    }
    
    UInt8 *buf = (UInt8*)IOMalloc(totalsize);
    hvDevice->readRawPacket((void*)buf, totalsize);
    HVDBGLOG("got pkt %u %u", type, totalsize);
    
    switch (type) {
      case kVMBusPacketTypeDataInband:
        handleIncomingPCIMessage((HyperVPCIBridgeIncomingMessageHeader*)buf, totalsize);
        
        hvDevice->wakeTransaction(0);
        break;
     // case kVMBusPacketTypeDataUsingTransferPages:
       // handleRNDISRanges((VMBusPacketTransferPages*) buf, headersize, totalsize);
       // break;
        
      case kVMBusPacketTypeCompletion:
        
        if (hvDevice->getPendingTransaction(((VMBusPacketHeader*)buf)->transactionId, &responseBuffer, &responseLength)) {
          memcpy(responseBuffer, (UInt8*)buf + headersize, responseLength);
          hvDevice->wakeTransaction(((VMBusPacketHeader*)buf)->transactionId);
        } else {
          //pktComp = (HyperVNetworkMessage*) (buf + headersize);
        //  HVDBGLOG("pkt completion status %X %X", pktComp->messageType, pktComp->v1.sendRNDISPacketComplete.status);
          
        ///  if (pktComp->messageType == kHyperVNetworkMessageTypeV1SendRNDISPacketComplete) {
        //    releaseSendIndex((UInt32)(((VMBusPacketHeader*)buf)->transactionId & ~kHyperVNetworkSendTransIdBits));
        //  }
        }
        break;
      default:
        break;
    }
    
    IOFree(buf, totalsize);
  }
}

void HyperVPCIBridge::handleIncomingPCIMessage(HyperVPCIBridgeIncomingMessageHeader *pciMsgHeader, UInt32 msgSize) {
  HyperVPCIBridgeIncomingMessageBusRelations *busRelations;
  
  switch (pciMsgHeader->type) {
    //
    // Handle bus relations message v1.
    //
    case kHyperVPCIBridgeMessageTypeBusRelations:
      if (msgSize < sizeof (HyperVPCIBridgeIncomingMessageBusRelations)) {
        HVSYSLOG("Bus relations message size %u < %u", msgSize, sizeof (HyperVPCIBridgeIncomingMessageBusRelations));
        return;
      }
      busRelations = (HyperVPCIBridgeIncomingMessageBusRelations*) pciMsgHeader;
      if (msgSize < sizeof (HyperVPCIBridgeIncomingMessageBusRelations) + (busRelations->functionCount * sizeof (HyperVPCIFunctionDescription))) {
        HVSYSLOG("Bus relations message size %u < %u", msgSize,
                 sizeof (HyperVPCIBridgeIncomingMessageBusRelations) + (busRelations->functionCount * sizeof (HyperVPCIFunctionDescription)));
        return;
      }
      
      //
      // Store function list for later use.
      //
      if (pciFunctions != nullptr) {
        IOFree(pciFunctions, pciFunctionCount * sizeof (HyperVPCIFunctionDescription));
        pciFunctions = nullptr;
        pciFunctionCount = 0;
      }
      
      pciFunctionCount = busRelations->functionCount;
      pciFunctions = (HyperVPCIFunctionDescription*) IOMalloc(pciFunctionCount * sizeof (HyperVPCIFunctionDescription));
      for (int i = 0; i < pciFunctionCount; i++) {
        memcpy(&pciFunctions[i], &busRelations->functions[i], sizeof (HyperVPCIFunctionDescription));
      }
      
      if (debugEnabled) {
        HVDBGLOG("Found %u functions", pciFunctionCount);
        for (int i = 0; i < pciFunctionCount; i++) {
          HVDBGLOG("Function %u with ID %04X:%04X:", i, pciFunctions[i].vendorId, pciFunctions[i].deviceId);
          HVDBGLOG("  Subsystem ID %04X:%04X, Revision 0x%X", pciFunctions[i].subVendorId, pciFunctions[i].subDeviceId, pciFunctions[i].revision);
          HVDBGLOG("  Class 0x%X, Subclass 0x%X, Prog int 0x%X", pciFunctions[i].baseClass, pciFunctions[i].subClass, pciFunctions[i].progInterface);
          HVDBGLOG("  Slot 0x%X, Serial 0x%X", pciFunctions[i].slot.slot, pciFunctions[i].serialNumber);
        }
      }
      break;
      
    default:
      break;
  }
}

bool HyperVPCIBridge::negotiateProtocolVersion() {
  HyperVPCIBridgeMessageProtocolVersionRequest pktVersion;
  pktVersion.header.type = kHyperVPCIBridgeMessageTypeQueryProtocolVersion;
  
  // Attempt to find newest version host can support.
  for (int i = 0; i < ARRAY_SIZE(usableVersions); i++) {
    pktVersion.version = usableVersions[i];
    HVDBGLOG("Attempting to use version 0x%X", pktVersion.version);
    
    UInt32 pciStatus;
    if (hvDevice->writeInbandPacket(&pktVersion, sizeof (pktVersion), true, &pciStatus, sizeof (pciStatus)) != kIOReturnSuccess) {
      return false;
    }
    HVDBGLOG("PCI return status is 0x%X", pciStatus);
    if (pciStatus == 0) {
      // Version is acceptable.
      currentPciVersion = pktVersion.version;
      HVDBGLOG("Using version 0x%X", currentPciVersion);
      return true;
    }
  }
  
  return false;
}

bool HyperVPCIBridge::queryBusRelations() {
  // Ask host to send list of PCI functions.
  HyperVPCIBridgeMessageHeader pktQueryRelations;
  pktQueryRelations.type = kHyperVPCIBridgeMessageTypeQueryBusRelations;
  if (hvDevice->writeInbandPacket(&pktQueryRelations, sizeof (pktQueryRelations), false) != kIOReturnSuccess) {
    return false;
  }
  
  // Response is not in the form of a normal completion, we'll
  // need to wait for an inband bus relations packet instead.
  hvDevice->doSleepThread();
  return pciFunctionCount != 0;
}
