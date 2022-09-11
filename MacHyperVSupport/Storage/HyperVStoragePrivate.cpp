//
//  HyperVStoragePrivate.cpp
//  Hyper-V storage driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVStorage.hpp"

IOReturn HyperVStorage::executeCommand(HyperVStoragePacket *packet, bool checkCompletion) {
  //
  // Send packet and get response.
  //
  IOReturn status = hvDevice->writeInbandPacket(packet, sizeof (HyperVStoragePacket) - packetSizeDelta, true, packet, sizeof (HyperVStoragePacket));
  if (status != kIOReturnSuccess) {
    return status;
  }
  
  //
  // Check status bits if requested.
  //
  if (checkCompletion &&
      packet->operation != kHyperVStoragePacketOperationCompleteIO &&
      packet->status != 0) {
    return kIOReturnIOError;
  }
  
  return kIOReturnSuccess;
}

bool HyperVStorage::wakePacketHandler(UInt8 *packet, UInt32 packetLength) {
  return true;
}

void HyperVStorage::handlePacket(UInt8 *packet, UInt32 packetLength) {
  VMBusPacketHeader *pktHeader = (VMBusPacketHeader*) packet;
  HyperVStoragePacket *storPkt = (HyperVStoragePacket*) &packet[HV_GET_VMBUS_PACKETSIZE(pktHeader->headerLength)];
  
  
  switch (storPkt->operation) {
    case kHyperVStoragePacketOperationCompleteIO:
      completeIO(storPkt);
      break;
      
    case kHyperVStoragePacketOperationEnumerateBus:
    case kHyperVStoragePacketOperationRemoveDevice:
      panic("SCSI device hotplug is not supported\n");
      break;
      
    default:
      HVSYSLOG("unknown type of a packet %X", storPkt->operation);
      break;
  }
}

void HyperVStorage::setHBAInfo() {
  OSString *propString;
  char verString[10];
  
  //
  // Populate vendor/product.
  //
  propString = OSString::withCString(kHyperVStorageVendor);
  if (propString != NULL) {
    SetHBAProperty(kIOPropertyVendorNameKey, propString);
    propString->release();
  }
  
  propString = OSString::withCString(kHyperVStorageProduct);
  if (propString != NULL) {
    SetHBAProperty(kIOPropertyProductNameKey, propString);
    propString->release();
  }
  
  //
  // Populate protocol version.
  //
  snprintf(verString, sizeof (verString), "%u.%u",
           HYPERV_STORAGE_PROTCOL_VERSION_MAJOR(protocolVersion),
           HYPERV_STORAGE_PROTCOL_VERSION_MINOR(protocolVersion));
  propString = OSString::withCString(verString);
  if (propString != NULL) {
    SetHBAProperty(kIOPropertyProductRevisionLevelKey, propString);
    propString->release();
  }
}

void HyperVStorage::completeIO(HyperVStoragePacket *packet) {
  if (packet->scsiRequest.scsiStatus == kSCSITaskStatus_CHECK_CONDITION) {
    HVDBGLOG("Doing a sense");
    SetAutoSenseData(currentTask, (SCSI_Sense_Data*)packet->scsiRequest.senseData, kSenseDefaultSize);
  }
  
 // DBGLOG("Response %X", packet.scsiRequest.senseData[0]);
  
  if (packet->scsiRequest.dataIn != kHyperVStorageSCSIRequestTypeUnknown) {
    completeDataTransfer(currentTask, packet);
  }
  SCSIParallelTaskIdentifier task = currentTask;
  if (task != NULL) {
    currentTask = NULL;
    CompleteParallelTask(task, (SCSITaskStatus)packet->scsiRequest.scsiStatus, kSCSIServiceResponse_TASK_COMPLETE);
  }
}

bool HyperVStorage::prepareDataTransfer(SCSIParallelTaskIdentifier parallelRequest, VMBusPacketMultiPageBuffer **pagePacket, UInt32 *pagePacketLength) {
  //
  // Get DMA command and page buffer for this task.
  //
  IODMACommand *dmaCmd = GetDMACommand(parallelRequest);
  *pagePacket = (VMBusPacketMultiPageBuffer*) GetHBADataPointer(parallelRequest);
  if (dmaCmd == NULL || *pagePacket == NULL) {
    return false;
  }
  
  //
  // Get segments to be transferred and prepare the DMA transfer.
  //
  UInt64 bufferLength = GetRequestedDataTransferCount(parallelRequest);
  UInt64 offsetSeg = 0;
  UInt32 numSegs = maxPageSegments;
  
  IOReturn status = dmaCmd->prepare(GetDataBufferOffset(parallelRequest), bufferLength);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Error %X while preparing the IODMACommand for buffer of %u bytes", status, bufferLength);
    return false;
  }
  
  status = dmaCmd->gen64IOVMSegments(&offsetSeg, segs64, &numSegs);
  if (status != kIOReturnSuccess) {
    dmaCmd->complete();
    HVSYSLOG("Error %X while generating segments for buffer of %u bytes", status, bufferLength);
    return false;
  }
  
  //
  // Populate PFNs in page buffer.
  //
  (*pagePacket)->range.length = (UInt32) bufferLength;
  (*pagePacket)->range.offset = 0;
  
  for (int i = 0; i < numSegs; i++) {
    if (i != 0 && i != (numSegs - 1)) {
      if (segs64[i].fLength != PAGE_SIZE && segs64[i].fLength != 0) {
        HVDBGLOG("Seg invalid %u: 0x%X %u bytes", i, segs64[i].fIOVMAddr, segs64[i].fLength);
      }
    }
    
    (*pagePacket)->range.pfns[i] = segs64[i].fIOVMAddr >> PAGE_SHIFT;
  }
  
  *pagePacketLength = sizeof (VMBusPacketMultiPageBuffer) + (sizeof (UInt64) * numSegs);
  return true;
}

void HyperVStorage::completeDataTransfer(SCSIParallelTaskIdentifier parallelRequest, HyperVStoragePacket *packet) {
  //
  // Get DMA command.
  //
  IODMACommand *dmaCmd = GetDMACommand(parallelRequest);
  if (dmaCmd == NULL) {
    return;
  }
  
  //
  // Complete DMA transfer.
  //
  dmaCmd->complete();
  if (packet->status == kHyperVStoragePacketSuccess) {
    SetRealizedDataTransferCount(parallelRequest, packet->scsiRequest.dataTransferLength);
  } else {
    SetRealizedDataTransferCount(parallelRequest, 0);
  }
}
