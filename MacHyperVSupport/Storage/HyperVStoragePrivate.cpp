//
//  HyperVStoragePrivate.cpp
//  Hyper-V storage driver
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#include "HyperVStorage.hpp"

//
// Hyper-V storage protocol list.
//
static const HyperVStorageProtocol storageProtocols[] = {
  {
    kHyperVStorageVersionWin10,
    kHyperVStoragePostWin7SenseBufferSize,
    0
  },
  {
    kHyperVStorageVersionWin8_1,
    kHyperVStoragePostWin7SenseBufferSize,
    0
  },
  {
    kHyperVStorageVersionWin8,
    kHyperVStoragePostWin7SenseBufferSize,
    0
  },
  {
    kHyperVStorageVersionWin7,
    kHyperVStoragePreWin8SenseBufferSize,
    sizeof (HyperVStorageSCSIRequestWin8Extension)
  },
  {
    kHyperVStorageVersionWin2008,
    kHyperVStoragePreWin8SenseBufferSize,
    sizeof (HyperVStorageSCSIRequestWin8Extension)
  },
};

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
  packet->flags = kHyperVStoragePacketFlagRequestCompletion;
  IOReturn status = _hvDevice->writeInbandPacket(packet, sizeof (HyperVStoragePacket) - _packetSizeDelta, true, packet, sizeof (HyperVStoragePacket));
  if (status != kIOReturnSuccess) {
    return status;
  }

  //
  // Check packet status if requested.
  //
  if (checkCompletion) {
    if (packet->operation != kHyperVStoragePacketOperationCompleteIO || packet->status != 0) {
      return kIOReturnIOError;
    }
  }
  return kIOReturnSuccess;
}

IOReturn HyperVStorage::prepareDataTransfer(SCSIParallelTaskIdentifier parallelRequest, VMBusPacketMultiPageBuffer **pagePacket, UInt32 *pagePacketLength) {
  IOReturn     status;
  UInt64       offsetSeg   = 0;
  UInt32       numSegs     = _maxPageSegments;
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

  status = dmaCommand->gen64IOVMSegments(&offsetSeg, _segs64, &numSegs);
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
      if (_segs64[i].fLength != PAGE_SIZE && _segs64[i].fLength != 0) {
        panic("Invalid segment %u: 0x%llX %llu bytes", (unsigned int) i, _segs64[i].fIOVMAddr, _segs64[i].fLength);
      }
    }

    (*pagePacket)->range.pfns[i] = _segs64[i].fIOVMAddr >> PAGE_SHIFT;
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

  propString = OSString::withCString(_isIDE ? kHyperVStorageProductIDE : kHyperVStorageProductSCSI);
  if (propString != nullptr) {
    SetHBAProperty(kIOPropertyProductNameKey, propString);
    propString->release();
  }

  //
  // Populate protocol version.
  //
  snprintf(verString, sizeof (verString), "%u.%u",
           (unsigned int) HYPERV_STORAGE_PROTCOL_VERSION_MAJOR(_protocolVersion),
           (unsigned int) HYPERV_STORAGE_PROTCOL_VERSION_MINOR(_protocolVersion));
  propString = OSString::withCString(verString);
  if (propString != nullptr) {
    SetHBAProperty(kIOPropertyProductRevisionLevelKey, propString);
    propString->release();
  }
}

IOReturn HyperVStorage::connectStorage() {
  IOReturn            status;
  HyperVStoragePacket storPkt;
  
  //
  // Check if we are actually an IDE controller.
  // IDE supports one device only, and requires the target to be non-zero when communicating with Hyper-V.
  //
  _isIDE = (strcmp(_hvDevice->getTypeIdString(), kHyperVStorageGuidIDE)) == 0;
  if (_isIDE) {
    UInt8 *instanceId = (UInt8*)_hvDevice->getInstanceId();
    _maxLuns = kHyperVStorageMaxLunsIDE;
    _targetId = (instanceId[5] << 8) | instanceId[4];
    HVDBGLOG("Storage controller is IDE, using target ID %u", _targetId);
  }

  //
  // Begin controller initialization.
  //
  bzero(&storPkt, sizeof (storPkt));
  storPkt.operation = kHyperVStoragePacketOperationBeginInitialization;
  status = sendStorageCommand(&storPkt, true);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to send begin initialization command with status 0x%X", status);
    return status;
  }

  //
  // Negotiate protocol version.
  //
  for (UInt32 i = 0; i < arrsize(storageProtocols); i++) {
    bzero(&storPkt, sizeof (storPkt));
    storPkt.operation                  = kHyperVStoragePacketOperationQueryProtocolVersion;
    storPkt.protocolVersion.majorMinor = storageProtocols[i].protocolVersion;
    storPkt.protocolVersion.revision   = 0; // Revision is zero for non-Windows.

    status = sendStorageCommand(&storPkt, false);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to send query protocol command with status 0x%X", status);
      return status;
    }

    //
    // A success means this protocol version is acceptable.
    //
    if (storPkt.status == 0) {
      _protocolVersion = storageProtocols[i].protocolVersion;
      _senseBufferSize = storageProtocols[i].senseBufferSize;
      _packetSizeDelta = storageProtocols[i].packetSizeDelta;
      HVDBGLOG("SCSI protocol version: 0x%X, sense buffer size: %u", _protocolVersion, _senseBufferSize);
      break;
    }
  }
  if (storPkt.status != 0) {
    HVSYSLOG("Query protocol command return error status 0x%X", storPkt.status);
    return kIOReturnIOError;
  }

  //
  // Query controller properties.
  //
  bzero(&storPkt, sizeof (storPkt));
  storPkt.operation = kHyperVStoragePacketOperationQueryProperties;
  status = sendStorageCommand(&storPkt, true);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to send query properties command with status 0x%X", status);
    return status;
  }

  _subChannelsSupported = storPkt.storageChannelProperties.flags & kHyperVStorageFlagSupportsMultiChannel;
  _maxSubChannels       = storPkt.storageChannelProperties.maxChannelCount;
  _maxTransferBytes     = storPkt.storageChannelProperties.maxTransferBytes;
  _maxPageSegments      = _maxTransferBytes / PAGE_SIZE;
  HVDBGLOG("Multi channel supported: %s, max sub channels: %u, max transfer bytes: %u (%u segments)",
           _subChannelsSupported ? "yes" : "no", _maxSubChannels, _maxTransferBytes, _maxPageSegments);

  //
  // Complete initialization.
  //
  bzero(&storPkt, sizeof (storPkt));
  storPkt.operation = kHyperVStoragePacketOperationEndInitialization;
  status = sendStorageCommand(&storPkt, true);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to send end initialization command with status 0x%X", status);
    return status;
  }

  return kIOReturnSuccess;
}

bool HyperVStorage::checkSCSIDiskPresent(UInt8 diskId) {
  IOReturn            status;
  HyperVStoragePacket storPkt = { };

  //
  // Prepare SCSI request packet and flags.
  //
  storPkt.operation = kHyperVStoragePacketOperationExecuteSRB;

  storPkt.scsiRequest.targetID                = _targetId;
  storPkt.scsiRequest.lun                     = diskId;
  storPkt.scsiRequest.win8Extension.srbFlags |= 0x00000008;
  storPkt.scsiRequest.length                  = sizeof (storPkt.scsiRequest) - _packetSizeDelta;
  storPkt.scsiRequest.senseInfoLength         = _senseBufferSize;
  storPkt.scsiRequest.dataIn                  = kHyperVStorageSCSIRequestTypeUnknown;

  //
  // Set CDB to TEST UNIT READY command.
  //
  storPkt.scsiRequest.cdb[0]    = kSCSICmd_TEST_UNIT_READY;
  storPkt.scsiRequest.cdb[1]    = 0x00;
  storPkt.scsiRequest.cdb[2]    = 0x00;
  storPkt.scsiRequest.cdb[3]    = 0x00;
  storPkt.scsiRequest.cdb[4]    = 0x00;
  storPkt.scsiRequest.cdb[5]    = 0x00;
  storPkt.scsiRequest.cdbLength = 6;

  //
  // Send SCSI packet and check result to see if disk is present.
  //
  status = sendStorageCommand(&storPkt, false);
  if (status != kIOReturnSuccess) {
    HVDBGLOG("Failed to send TEST UNIT READY SCSI packet with status 0x%X", status);
    return false;
  }

  HVDBGLOG("Disk %u status: 0x%X SRB status: 0x%X", diskId, storPkt.scsiRequest.scsiStatus, storPkt.scsiRequest.srbStatus);
  return storPkt.scsiRequest.srbStatus != kHyperVSRBStatusInvalidLUN;
}

void HyperVStorage::startDiskEnumeration() {
  //
  // Begin disk enumeration on separate thread.
  //
  HVDBGLOG("Starting disk enumeration thread");
  thread_call_enter(_scanSCSIDiskThread);
}

void HyperVStorage::scanSCSIDisks() {
  HVDBGLOG("Starting disk scan of %u disks", _maxLuns);

  for (UInt32 lun = 0; lun < _maxLuns; lun++) {
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
