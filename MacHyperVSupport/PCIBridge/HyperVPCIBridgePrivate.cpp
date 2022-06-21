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
  HVDBGLOG("Incoming PCI message type 0x%X", pciMsgHeader->type);
  
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

bool HyperVPCIBridge::allocatePCIConfigWindow() {
  // Get HyperVModuleDevice instance used for allocating MMIO regions for Hyper-V.
  OSDictionary *vmodMatching = IOService::serviceMatching("HyperVModuleDevice");
  if (vmodMatching == NULL) {
    HVSYSLOG("Failed to create HyperVModuleDevice matching dictionary");
    return false;
  }
  
  HVDBGLOG("Waiting for HyperVModuleDevice");
  IOService *vmodService = waitForMatchingService(vmodMatching);
  vmodMatching->release();
  if (vmodService == NULL) {
    HVSYSLOG("Failed to locate HyperVModuleDevice");
    return false;
  }
  HVDBGLOG("Got instance of HyperVModuleDevice");
  hvModuleDevice = OSDynamicCast(HyperVModuleDevice, vmodService);
  hvModuleDevice->retain();
  
  // Allocate PCI config window.
  if (!hvModuleDevice->rangeAllocator->allocate(kHyperVPCIBridgeWindowSize, &pciConfigSpace, PAGE_SIZE)) {
    return false;
  }
  pciConfigMemoryDescriptor = IOMemoryDescriptor::withPhysicalAddress(pciConfigSpace, kHyperVPCIBridgeWindowSize, kIOMemoryDirectionInOut);
  pciConfigMemoryMap = pciConfigMemoryDescriptor->map();
  HVDBGLOG("PCI config window located @ phys 0x%llX", pciConfigSpace);
  
  return true;
}

bool HyperVPCIBridge::enterPCID0() {
  // Instruct Hyper-V to enable the PCI bridge using the allocated PCI config window.
  HyperVPCIBridgeMessagePCIBusD0Entry pktD0;
  pktD0.header.type = kHyperVPCIBridgeMessageTypeBusD0Entry;
  pktD0.reserved    = 0;
  pktD0.mmioBase    = pciConfigSpace;
  
  UInt32 pciStatus;
  if (hvDevice->writeInbandPacket(&pktD0, sizeof (pktD0), true, &pciStatus, sizeof (pciStatus)) != kIOReturnSuccess) {
    return false;
  }
  
  if (pciStatus != 0) {
    HVSYSLOG("Enter D0 returned error status 0x%X", pciStatus);
    return false;
  }
  
  HVDBGLOG("PCI bridge has entered D0 state using config space @ phys 0x%X", pciConfigSpace);
  return true;
}

UInt32 HyperVPCIBridge::readPCIConfig(UInt32 offset, UInt8 size) {
  UInt32 result;
  
  HVDBGLOG("Reading size %u from offset 0x%X", size, offset);
  
  // Simulate special registers.
  if (offset + size <= kIOPCIConfigCommand) {
    memcpy(&result, ((UInt8*)&pciFunctions[0].vendorId) + offset, size);
  } else if (offset >= kIOPCIConfigRevisionID && offset + size <= kIOPCIConfigCacheLineSize) {
    memcpy(&result, ((UInt8*)&pciFunctions[0].revision) + offset - kIOPCIConfigRevisionID, size);
  } else if (offset >= kIOPCIConfigSubSystemVendorID && offset + size <= kIOPCIConfigExpansionROMBase) {
    memcpy(&result, ((UInt8*)&pciFunctions[0].subVendorId) + offset - kIOPCIConfigSubSystemVendorID, size);
  } else if (offset >= kIOPCIConfigExpansionROMBase && offset + size <= kIOPCIConfigCapabilitiesPtr) {
    // Not implemented.
    result = 0;
  } else if (offset >= kIOPCIConfigInterruptLine && offset + size <= kIOPCIConfigInterruptPin) {
    // Not supported.
    result = 0;
  } else if (offset + size <= PAGE_SIZE) {
    volatile UInt8 *pciAddress = (UInt8*)pciConfigMemoryMap->getAddress() + kHyperVPCIConfigPageOffset + offset;
    
    IOInterruptState ints = IOSimpleLockLockDisableInterrupt(pciLock);
    switch (size) {
      case sizeof (UInt8):
        result = *pciAddress;
        break;
        
      case sizeof (UInt16):
        result = OSReadLittleInt16(pciAddress, 0);
        break;
        
      default:
        result = OSReadLittleInt32(pciAddress, 0);
        break;
    }
    IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
  } else {
    HVDBGLOG("Attempted to read beyond PCI config space");
    result = 0xFFFFFFFF;
  }
  
  return result;
}

void HyperVPCIBridge::writePCIConfig(UInt32 offset, UInt8 size, UInt32 value) {
  if (offset >= kIOPCIConfigSubSystemVendorID && offset + size <= kIOPCIConfigCapabilitiesPtr) {
    // Read-only sections.
    HVDBGLOG("Attempted to write value 0x%X to read-only offset 0x%X", value, offset);
  } else if (offset >= kIOPCIConfigCommand && offset + size <= PAGE_SIZE) {
    HVDBGLOG("Writing value 0x%X of size %u to offset 0x%X", value, size, offset);
    volatile UInt8 *pciAddress = (UInt8*)pciConfigMemoryMap->getAddress() + kHyperVPCIConfigPageOffset + offset;
    
    IOInterruptState ints = IOSimpleLockLockDisableInterrupt(pciLock);
    switch (size) {
      case sizeof (UInt8):
        *pciAddress = (UInt8)value;
        break;
        
      case sizeof (UInt16):
        OSWriteLittleInt16(pciAddress, 0, (UInt16)value);
        break;
        
      default:
        OSWriteLittleInt32(pciAddress, 0, value);
        break;
    }
    IOSimpleLockUnlockEnableInterrupt(pciLock, ints);
  } else {
    HVDBGLOG("Attempted to write value 0x%X to out-of-bounds offset 0x%X", value, offset);
  }
}
