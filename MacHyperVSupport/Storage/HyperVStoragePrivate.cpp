//
//  HyperVStoragePrivate.cpp
//  Hyper-V storage driver
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#include "HyperVStorage.hpp"

bool HyperVStorage::wakePacketHandler(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  return true;
}

void HyperVStorage::handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  HyperVStoragePacket *storPkt = (HyperVStoragePacket*) pktData;

  switch (storPkt->operation) {
    case kHyperVStoragePacketOperationCompleteIO:
      handleIOCompletion(pktHeader->transactionId, storPkt);
      break;

    case kHyperVStoragePacketOperationEnumerateBus:
    case kHyperVStoragePacketOperationRemoveDevice:
      startDiskEnumeration();
      break;

    default:
      HVSYSLOG("Unknown storage packet of type 0x%X received", storPkt->operation);
      break;
  }
}

void HyperVStorage::handleIOCompletion(UInt64 transactionId, HyperVStoragePacket *packet) {
  SCSIParallelTaskIdentifier parallelRequest = (SCSIParallelTaskIdentifier) transactionId;

  HVDATADBGLOG("Completing request %p", parallelRequest);
  if (packet->scsiRequest.srbStatus != 1) {
    HVSYSLOG("SRB STATUS %X", packet->scsiRequest.srbStatus);
  }

  //
  // Handle auto sense.
  //
  if (packet->scsiRequest.scsiStatus == kSCSITaskStatus_CHECK_CONDITION) {
    HVDBGLOG("Doing a sense");
    SetAutoSenseData(parallelRequest, (SCSI_Sense_Data*)packet->scsiRequest.senseData, kSenseDefaultSize);
  }

  //
  // Complete data transfer for data packets.
  //
  if (packet->scsiRequest.dataIn != kHyperVStorageSCSIRequestTypeUnknown) {
    completeDataTransfer(parallelRequest, packet);
  }

  //
  // Complete the task.
  //
  CompleteParallelTask(parallelRequest, (SCSITaskStatus)packet->scsiRequest.scsiStatus, kSCSIServiceResponse_TASK_COMPLETE);
}

IOReturn HyperVStorage::sendStorageCommand(HyperVStoragePacket *packet, bool checkCompletion) {
  //
  // Send packet and get response.
  //
  IOReturn status = _hvDevice->writeInbandPacket(packet, sizeof (HyperVStoragePacket) - packetSizeDelta, true, packet, sizeof (HyperVStoragePacket));
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

IOReturn HyperVStorage::prepareDataTransfer(SCSIParallelTaskIdentifier parallelRequest, VMBusPacketMultiPageBuffer **pagePacket, UInt32 *pagePacketLength) {
  IOReturn     status;
  UInt64       offsetSeg   = 0;
  UInt32       numSegs     = maxPageSegments;
  UInt64       dataLength  = GetRequestedDataTransferCount(parallelRequest);
  IODMACommand *dmaCommand = GetDMACommand(parallelRequest);

  if (dataLength > UINT32_MAX) {
    HVSYSLOG("Attempted to request more than 4GB of data");
    return kIOReturnUnsupported;
  }

  *pagePacket = (VMBusPacketMultiPageBuffer*) GetHBADataPointer(parallelRequest);
  if (*pagePacket == nullptr) {
    HVSYSLOG("Failed to get task HBA data");
    return kIOReturnIOError;
  }

  //
  // Get list of segments for DMA transfer.
  //
  status = dmaCommand->prepare(GetDataBufferOffset(parallelRequest), dataLength);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to prepare IODMACommand for buffer of %u bytes with status 0x%X", dataLength, status);
    return status;
  }

  status = dmaCommand->gen64IOVMSegments(&offsetSeg, segs64, &numSegs);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to generate segments for buffer of %u bytes", dataLength, status);
    dmaCommand->complete();
    return status;
  }

  //
  // Populate PFNs containing segments.
  //
  (*pagePacket)->range.length = (UInt32) dataLength;
  (*pagePacket)->range.offset = 0;

  for (UInt32 i = 0; i < numSegs; i++) {
    if (i != 0 && i != (numSegs - 1)) {
      if (segs64[i].fLength != PAGE_SIZE && segs64[i].fLength != 0) {
        panic("Invalid segment %u: 0x%llX %llu bytes", (unsigned int) i, segs64[i].fIOVMAddr, segs64[i].fLength);
      }
    }

    (*pagePacket)->range.pfns[i] = segs64[i].fIOVMAddr >> PAGE_SHIFT;
  }

  *pagePacketLength = sizeof (**pagePacket) + (sizeof (UInt64) * numSegs);
  return kIOReturnSuccess;
}

void HyperVStorage::completeDataTransfer(SCSIParallelTaskIdentifier parallelRequest, HyperVStoragePacket *packet) {
  IODMACommand *dmaCommand = GetDMACommand(parallelRequest);
  dmaCommand->complete();

  SetRealizedDataTransferCount(parallelRequest,
                               (packet->status == kHyperVStoragePacketSuccess) ? packet->scsiRequest.dataTransferLength : 0);
}

void HyperVStorage::setHBAInfo() {
  OSString *propString;
  char verString[10];

  //
  // Populate vendor/product.
  //
  propString = OSString::withCString(kHyperVStorageVendor);
  if (propString != nullptr) {
    SetHBAProperty(kIOPropertyVendorNameKey, propString);
    propString->release();
  }

  propString = OSString::withCString(kHyperVStorageProduct);
  if (propString != nullptr) {
    SetHBAProperty(kIOPropertyProductNameKey, propString);
    propString->release();
  }

  //
  // Populate protocol version.
  //
  snprintf(verString, sizeof (verString), "%u.%u",
           (unsigned int) HYPERV_STORAGE_PROTCOL_VERSION_MAJOR(protocolVersion),
           (unsigned int) HYPERV_STORAGE_PROTCOL_VERSION_MINOR(protocolVersion));
  propString = OSString::withCString(verString);
  if (propString != nullptr) {
    SetHBAProperty(kIOPropertyProductRevisionLevelKey, propString);
    propString->release();
  }
}

bool HyperVStorage::checkSCSIDiskPresent(UInt8 diskId) {
  IOReturn            status;
  HyperVStoragePacket packet = { };

  //
  // Prepare SCSI request packet and flags.
  //
  packet.operation = kHyperVStoragePacketOperationExecuteSRB;
  packet.flags     = kHyperVStoragePacketFlagRequestCompletion;

  packet.scsiRequest.targetID                = 0;
  packet.scsiRequest.lun                     = diskId;
  packet.scsiRequest.win8Extension.srbFlags |= 0x00000008;
  packet.scsiRequest.length                  = sizeof (packet.scsiRequest);
  packet.scsiRequest.senseInfoLength         = senseBufferSize;
  packet.scsiRequest.dataIn                  = kHyperVStorageSCSIRequestTypeUnknown;

  //
  // Set CDB to TEST UNIT READY command.
  //
  packet.scsiRequest.cdb[0]    = kSCSICmd_TEST_UNIT_READY;
  packet.scsiRequest.cdb[1]    = 0x00;
  packet.scsiRequest.cdb[2]    = 0x00;
  packet.scsiRequest.cdb[3]    = 0x00;
  packet.scsiRequest.cdb[4]    = 0x00;
  packet.scsiRequest.cdb[5]    = 0x00;
  packet.scsiRequest.cdbLength = 6;

  //
  // Send SCSI packet and check result to see if disk is present.
  //
  status = _hvDevice->writeInbandPacket(&packet, sizeof (packet) - packetSizeDelta, true, &packet, sizeof (packet));
  if (status != kIOReturnSuccess) {
    HVDBGLOG("Failed to send TEST UNIT READY SCSI packet with status 0x%X", status);
    return false;
  }

  HVDBGLOG("Disk %u status: 0x%X SRB status: 0x%X", diskId, packet.scsiRequest.scsiStatus, packet.scsiRequest.srbStatus);
  return packet.scsiRequest.srbStatus == kHyperVSRBStatusSuccess;
}

void HyperVStorage::startDiskEnumeration() {
  //
  // Begin disk enumeration on separate thread.
  //
  HVDBGLOG("Starting disk enumeration thread");
  thread_call_enter(scanSCSIDiskThread);
}

void HyperVStorage::scanSCSIDisks() {
  HVDBGLOG("Starting disk scan of %u disks", kHyperVStorageMaxTargets);

  for (UInt32 lun = 0; lun < kHyperVStorageMaxTargets; lun++) {
    if (checkSCSIDiskPresent(lun)) {
      if (GetTargetForID(lun) == nullptr) {
        HVDBGLOG("Disk %u is newly added", lun);
        CreateTargetForID(lun);
      } else {
        HVDBGLOG("Disk %u is still present", lun);
      }
    } else {
      if (GetTargetForID(lun) != nullptr) {
        HVDBGLOG("Disk %u was removed", lun);
        DestroyTargetForID(lun);
      }
    }
  }

  HVDBGLOG("Completed disk scan");
}
