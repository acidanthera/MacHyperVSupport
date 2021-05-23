//
//  HyperVStoragePrivate.cpp
//  Hyper-V storage driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVStorage.hpp"

IOReturn HyperVStorage::executeCommand(HyperVStoragePacket *packet, bool checkCompletion) {
  //
  // Create request for inband data with a direct response.
  //
  HyperVVMBusDeviceRequest request;
  request.sendData = packet;
  request.sendDataLength = sizeof (*packet) - packetSizeDelta;
  request.sendPacketType = kVMBusPacketTypeDataInband;
  request.responseRequired = true;
  
  request.responseData = packet;
  request.responseDataLength = sizeof (*packet);
  
  //
  // Send packet and get response.
  //
  IOReturn status = hvDevice->doRequest(&request);
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
  
  HyperVVMBusDeviceRequest request;
  request.sendData = NULL;
  request.responseData = &packet;
  request.responseDataLength = sizeof (packet);
  
 // UInt32 dataSize = sizeof(packet);
  
 // hvDevice->readPacket(&packet, &dataSize);
  hvDevice->doRequest(&request);
  
  switch (packet.operation) {
    case kHyperVStoragePacketOperationCompleteIO:
      completeIO(&packet);
      break;
      
    case kHyperVStoragePacketOperationEnumerateBus:
    case kHyperVStoragePacketOperationRemoveDevice:
      panic("SCSI device hotplug is not supported\n");
      break;
      
    default:
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

bool HyperVStorage::prepareDataTransfer(SCSIParallelTaskIdentifier parallelRequest, HyperVVMBusDeviceRequest *request) {
  //
  // Get DMA command and page buffer for this task.
  //
  IODMACommand *dmaCmd = GetDMACommand(parallelRequest);
  request->multiPageBuffer = (VMBusPacketMultiPageBuffer*) GetHBADataPointer(parallelRequest);
  if (dmaCmd == NULL || request->multiPageBuffer == NULL) {
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
  request->multiPageBuffer->range.length = (UInt32) bufferLength;
  request->multiPageBuffer->range.offset = 0;
  
  for (int i = 0; i < numSegs; i++) {
    if (i != 0 && i != (numSegs - 1)) {
      if (segs64[i].fLength != PAGE_SIZE && segs64[i].fLength != 0) {
        DBGLOG("Seg invalid %u: 0x%X %u bytes", i, segs64[i].fIOVMAddr, segs64[i].fLength);
      }
    }
    
    request->multiPageBuffer->range.pfns[i] = segs64[i].fIOVMAddr >> PAGE_SHIFT;
  }
  
  request->multiPageBufferLength = sizeof (VMBusPacketMultiPageBuffer) + (sizeof (UInt64) * numSegs);
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
