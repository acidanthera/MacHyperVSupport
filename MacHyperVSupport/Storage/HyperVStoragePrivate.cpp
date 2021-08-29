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
  IOReturn status = hvDevice->writeInbandPacket(packet, sizeof (*packet) - packetSizeDelta, true, packet, sizeof (*packet));
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

void HyperVStorage::handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count) {
 // DBGLOG("Interrupt!");
  
  HyperVStoragePacket packet;

  VMBusPacketType type;
  UInt32 headersize;
  UInt32 totalsize;
  
  void *responseBuffer;
  UInt32 responseLength;
  
  while (true) {
    if (!hvDevice->nextPacketAvailable(&type, &headersize, &totalsize)) {
      break;
    }
    
    UInt64 transactionId;
    hvDevice->readInbandCompletionPacket(&packet, sizeof (packet), &transactionId);
    
    switch (packet.operation) {
      case kHyperVStoragePacketOperationCompleteIO:
        if (hvDevice->getPendingTransaction(transactionId, &responseBuffer, &responseLength)) {
          memcpy(responseBuffer, &packet, sizeof (packet));
          hvDevice->wakeTransaction(transactionId);
        } else {
          completeIO(&packet);
        }
        break;
        
      case kHyperVStoragePacketOperationEnumerateBus:
      case kHyperVStoragePacketOperationRemoveDevice:
        panic("SCSI device hotplug is not supported\n");
        break;
        
      default:
        break;
    }
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
    DBGLOG("Doing a sense");
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
    SYSLOG("Error %X while preparing the IODMACommand for buffer of %u bytes", status, bufferLength);
    return false;
  }
  
  status = dmaCmd->gen64IOVMSegments(&offsetSeg, segs64, &numSegs);
  if (status != kIOReturnSuccess) {
    dmaCmd->complete();
    SYSLOG("Error %X while generating segments for buffer of %u bytes", status, bufferLength);
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
        DBGLOG("Seg invalid %u: 0x%X %u bytes", i, segs64[i].fIOVMAddr, segs64[i].fLength);
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
