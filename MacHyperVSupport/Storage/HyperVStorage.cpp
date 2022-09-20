//
//  HyperVStorage.cpp
//  Hyper-V storage driver
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#include "HyperVStorage.hpp"

OSDefineMetaClassAndStructors(HyperVStorage, super);

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

bool HyperVStorage::InitializeController() {
  HyperVStoragePacket packet;
  
  if (HVCheckOffArg()) {
    return false;
  }
  
  //
  // Get parent VMBus device object.
  //
  _hvDevice = OSDynamicCast(HyperVVMBusDevice, getProvider());
  if (_hvDevice == NULL) {
    return false;
  }
  _hvDevice->retain();
  HVCheckDebugArgs();
  
  HVDBGLOG("Initializing Hyper-V Synthetic Storage controller");
  
  //
  // Assume we are on an older host and take off the Windows 8 extensions by default.
  //
  packetSizeDelta = sizeof (HyperVStorageSCSIRequestWin8Extension);
  
  //
  // Configure interrupt.
  // macOS 10.4 always configures the interrupt in the superclass, do
  // not configure the interrupt ourselves in that case.
  //
  _hvDevice->installPacketActions(this, OSMemberFunctionCast(HyperVVMBusDevice::PacketReadyAction, this, &HyperVStorage::handlePacket), OSMemberFunctionCast(HyperVVMBusDevice::WakePacketAction, this, &HyperVStorage::wakePacketHandler), PAGE_SIZE, getKernelVersion() >= KernelVersion::Leopard);
#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_5
  if (getKernelVersion() < KernelVersion::Leopard) {
    EnableInterrupt();
  }
#endif
  
  //
  // Configure the channel.
  //
  if (_hvDevice->openVMBusChannel(kHyperVStorageRingBufferSize, kHyperVStorageRingBufferSize) != kIOReturnSuccess) {
    return false;
  }
  
  //
  // Begin controller initialization.
  //
  bzero(&packet, sizeof (packet));
  packet.operation = kHyperVStoragePacketOperationBeginInitialization;
  if (sendStorageCommand(&packet, true) != kIOReturnSuccess) {
    return false;
  }
  
  //
  // Negotiate protocol version.
  //
  for (UInt32 i = 0; i < ARRAY_SIZE(storageProtocols); i++) {
    bzero(&packet, sizeof (packet));
    packet.operation                  = kHyperVStoragePacketOperationQueryProtocolVersion;
    packet.protocolVersion.majorMinor = storageProtocols[i].protocolVersion;
    packet.protocolVersion.revision   = 0; // Revision is zero for non-Windows.
    
    if (sendStorageCommand(&packet, false) != kIOReturnSuccess) {
      return false;
    }
    
    //
    // A success means this protocol version is acceptable.
    //
    if (packet.status == 0) {
      protocolVersion = storageProtocols[i].protocolVersion;
      senseBufferSize = storageProtocols[i].senseBufferSize;
      packetSizeDelta = storageProtocols[i].packetSizeDelta;
      HVDBGLOG("SCSI protocol version: 0x%X, sense buffer size: %u", protocolVersion, senseBufferSize);
      break;
    }
  }
  
  if (packet.status != 0) {
    return false;
  }
  
  //
  // Query controller properties.
  //
  bzero(&packet, sizeof (packet));
  packet.operation = kHyperVStoragePacketOperationQueryProperties;
  if (sendStorageCommand(&packet, true) != kIOReturnSuccess) {
    return false;
  }
  
  subChannelsSupported = packet.storageChannelProperties.flags & kHyperVStorageFlagSupportsMultiChannel;
  maxSubChannels       = packet.storageChannelProperties.maxChannelCount;
  maxTransferBytes     = packet.storageChannelProperties.maxTransferBytes;
  maxPageSegments      = maxTransferBytes / PAGE_SIZE;
  HVDBGLOG("Multi channel supported: %s, max sub channels: %u, max transfer bytes: %u (%u segments)",
         subChannelsSupported ? "yes" : "no", maxSubChannels, maxTransferBytes, maxPageSegments);
  
  //
  // Complete initialization.
  //
  bzero(&packet, sizeof (packet));
  packet.operation = kHyperVStoragePacketOperationEndInitialization;
  if (sendStorageCommand(&packet, true) != kIOReturnSuccess) {
    return false;
  }
  
  segs64 = (IODMACommand::Segment64*) IOMalloc(sizeof (IODMACommand::Segment64) * maxPageSegments);

  //
  // Populate HBA properties.
  //
  setHBAInfo();
  
  scanSCSIDiskThread = thread_call_allocate(OSMemberFunctionCast(thread_call_func_t, this, &HyperVStorage::scanSCSIDisks), this);
  
  HVSYSLOG("Initialized Hyper-V Synthetic Storage controller");
  return true;
}

void HyperVStorage::TerminateController() {
  HVDBGLOG("Controller is terminated");
}

bool HyperVStorage::StartController() {
  HVDBGLOG("Controller is now started");
  startDiskEnumeration();
  return true;
}

void HyperVStorage::StopController() {
  HVDBGLOG("Controller is now stopped");
}

bool HyperVStorage::DoesHBAPerformDeviceManagement() {
  //
  // This driver handles target creation/removal.
  //
  return true;
}

bool HyperVStorage::DoesHBASupportSCSIParallelFeature(SCSIParallelFeature theFeature) {
  //
  // No special features are supported.
  //
  return false;
}

bool HyperVStorage::InitializeTargetForID(SCSITargetIdentifier targetID) {
  //
  // Ensure target ID is under maximum.
  //
  return (targetID < kHyperVStorageMaxTargets);
}

void HyperVStorage::HandleInterruptRequest() {
#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_5
  //
  // macOS 10.4 configures the interrupt in the superclass, invoke
  // our interrupt handler here.
  //
  _hvDevice->triggerPacketAction();
#endif
}

SCSIInitiatorIdentifier HyperVStorage::ReportInitiatorIdentifier() {
  return kHyperVStorageMaxTargets;
}

SCSIDeviceIdentifier HyperVStorage::ReportHighestSupportedDeviceID() {
  return kHyperVStorageMaxTargets - 1;
}

UInt32 HyperVStorage::ReportMaximumTaskCount() {
  HVDBGLOG("start");
  return 1;
}

UInt32 HyperVStorage::ReportHBASpecificTaskDataSize() {
  HVDBGLOG("start");
  return sizeof (VMBusPacketMultiPageBuffer) + (sizeof (UInt64) * maxPageSegments); //32 * 4096;
}

UInt32 HyperVStorage::ReportHBASpecificDeviceDataSize() {
  HVDBGLOG("start");
  return 0;
}

IOInterruptEventSource* HyperVStorage::CreateDeviceInterrupt(IOInterruptEventSource::Action action,
                                                             IOFilterInterruptEventSource::Filter filter, IOService *provider) {
  //
  // Interrupts are handled by the provider, so we do
  // not want the parent class trying to make one.
  //
  return nullptr;
}

bool HyperVStorage::InitializeDMASpecification(IODMACommand *command) {
  //
  // IODMACommand is configured with 64-bit addressing and page-sized, page-aligned segments.
  // Hyper-V requires page-sized segments due to its use of page numbers.
  //
  return command->initWithSpecification(kIODMACommandOutputHost64, kHyperVStorageSegmentBits, kHyperVStorageSegmentSize,
                                        IODMACommand::kMapped, maxTransferBytes, kHyperVStorageSegmentSize);
}

SCSILogicalUnitNumber HyperVStorage::ReportHBAHighestLogicalUnitNumber() {
  return kHyperVStorageMaxLuns - 1;
}

SCSIServiceResponse HyperVStorage::AbortTaskRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL, SCSITaggedTaskIdentifier theQ) {
  HVDBGLOG("start");
  return kSCSIServiceResponse_TASK_COMPLETE;
}

SCSIServiceResponse HyperVStorage::AbortTaskSetRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL) {
  HVDBGLOG("start");
  return kSCSIServiceResponse_TASK_COMPLETE;
}

SCSIServiceResponse HyperVStorage::ClearACARequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL) {
  HVDBGLOG("start");
  return kSCSIServiceResponse_TASK_COMPLETE;
}

SCSIServiceResponse HyperVStorage::ClearTaskSetRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL) {
  HVDBGLOG("start");
  return kSCSIServiceResponse_TASK_COMPLETE;
}

SCSIServiceResponse HyperVStorage::LogicalUnitResetRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL) {
  HVDBGLOG("start");
  return kSCSIServiceResponse_TASK_COMPLETE;
}

SCSIServiceResponse HyperVStorage::TargetResetRequest(SCSITargetIdentifier theT) {
  HVDBGLOG("start");
  return kSCSIServiceResponse_TASK_COMPLETE;
}

SCSIServiceResponse HyperVStorage::ProcessParallelTask(SCSIParallelTaskIdentifier parallelRequest) {
  IOReturn            status;
  HyperVStoragePacket packet = { };

  UInt8                      dataDirection;
  VMBusPacketMultiPageBuffer *pagePacket;
  UInt32                     pagePacketLength;

  if (parallelRequest == nullptr) {
    HVSYSLOG("Invalid SCSI request passed");
    return kSCSIServiceResponse_FUNCTION_REJECTED;
  }

  if (GetLogicalUnitNumber(parallelRequest) != 0) {
    HVDBGLOG("LUN is non-zero");
    return kSCSIServiceResponse_FUNCTION_REJECTED;
  }

  //
  // Create SCSI command execution packet.
  //
  packet.operation = kHyperVStoragePacketOperationExecuteSRB;
  packet.flags     = kHyperVStoragePacketFlagRequestCompletion;

  packet.scsiRequest.targetID        = 0;
  packet.scsiRequest.lun             = GetTargetIdentifier(parallelRequest);
  packet.scsiRequest.senseInfoLength = senseBufferSize;
  packet.scsiRequest.win8Extension.srbFlags |= 0x00000008;
  packet.scsiRequest.length = sizeof (packet.scsiRequest); // TODO

  //
  // Determine data direction flags.
  //
  dataDirection = GetDataTransferDirection(parallelRequest);
  switch (dataDirection) {
    case kSCSIDataTransfer_NoDataTransfer:
      packet.scsiRequest.dataIn = kHyperVStorageSCSIRequestTypeUnknown;
      break;

    case kSCSIDataTransfer_FromInitiatorToTarget:
      packet.scsiRequest.dataIn = kHyperVStorageSCSIRequestTypeWrite;
      packet.scsiRequest.win8Extension.srbFlags |= 0x00000080;
      break;

    case kSCSIDataTransfer_FromTargetToInitiator:
      packet.scsiRequest.dataIn = kHyperVStorageSCSIRequestTypeRead;
      packet.scsiRequest.win8Extension.srbFlags |= 0x00000040;
      break;

    default:
      HVDBGLOG("Bad data direction 0x%X", dataDirection);
      return kSCSIServiceResponse_FUNCTION_REJECTED;
  }
  HVDATADBGLOG("Sending command to LUN %u (direction %X) with request %p", packet.scsiRequest.lun,
               dataDirection, parallelRequest);

  //
  // Get CDB data.
  //
  packet.scsiRequest.cdbLength = GetCommandDescriptorBlockSize(parallelRequest);
  GetCommandDescriptorBlock(parallelRequest, &packet.scsiRequest.cdb);
  HVDATADBGLOG("CDB Data %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X %X",
               packet.scsiRequest.cdb[0], packet.scsiRequest.cdb[1], packet.scsiRequest.cdb[2], packet.scsiRequest.cdb[3],
               packet.scsiRequest.cdb[4], packet.scsiRequest.cdb[5], packet.scsiRequest.cdb[6], packet.scsiRequest.cdb[7],
               packet.scsiRequest.cdb[8], packet.scsiRequest.cdb[9], packet.scsiRequest.cdb[10], packet.scsiRequest.cdb[11],
               packet.scsiRequest.cdb[12], packet.scsiRequest.cdb[13], packet.scsiRequest.cdb[14], packet.scsiRequest.cdb[15]);

  //
  // Prepare for data transfer if one is requested.
  // Otherwise send basic inband packet.
  //
  if (dataDirection != kSCSIDataTransfer_NoDataTransfer) {
    status = prepareDataTransfer(parallelRequest, &pagePacket, &pagePacketLength);
    if (status != kIOReturnSuccess) {
      HVDBGLOG("Failed to prepare data transfer with status 0x%X", status);
      return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
    }

    packet.scsiRequest.dataTransferLength = (UInt32) GetRequestedDataTransferCount(parallelRequest);
    status = _hvDevice->writeGPADirectMultiPagePacket(&packet, sizeof (packet) - packetSizeDelta, true,
                                                      pagePacket, pagePacketLength, nullptr, 0,
                                                      (UInt64)parallelRequest);
    if (status != kIOReturnSuccess) {
      HVDBGLOG("Failed to send data SCSI packet with status 0x%X", status);
      return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
    }
  } else {
    status = _hvDevice->writeInbandPacketWithTransactionId(&packet, sizeof (packet) - packetSizeDelta, (UInt64)parallelRequest, true);
    if (status != kIOReturnSuccess) {
      HVDBGLOG("Failed to send non-data SCSI packet with status 0x%X", status);
      return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
    }
  }

  HVDATADBGLOG("Request %p submitted", parallelRequest);
  return kSCSIServiceResponse_Request_In_Process;
}

void HyperVStorage::ReportHBAConstraints(OSDictionary *constraints) {
  OSNumber *osNumber;
  
  //
  // Populate HBA requirements and limitations.
  //
  osNumber = OSNumber::withNumber(maxPageSegments, 32);
  if (osNumber != NULL) {
    constraints->setObject(kIOMaximumSegmentCountReadKey, osNumber);
    constraints->setObject(kIOMaximumSegmentCountWriteKey, osNumber);
    osNumber->release();
  }

  osNumber = OSNumber::withNumber(kHyperVStorageSegmentSize, 32);
  if (osNumber != NULL) {
    constraints->setObject(kIOMaximumSegmentByteCountReadKey, osNumber);
    constraints->setObject(kIOMaximumSegmentByteCountWriteKey, osNumber);
    osNumber->release();
  }
  
  osNumber = OSNumber::withNumber(kHyperVStorageSegmentAlignment, 64);
  if (osNumber != NULL) {
    constraints->setObject(kIOMinimumHBADataAlignmentMaskKey, osNumber);
    osNumber->release();
  }
  
  osNumber = OSNumber::withNumber(kHyperVStorageSegmentBits, 32);
  if (osNumber != NULL) {
    constraints->setObject(kIOMaximumSegmentAddressableBitCountKey, osNumber);
    osNumber->release();
  }
  
  osNumber = OSNumber::withNumber(4, 32);
  if (osNumber != NULL) {
    constraints->setObject(kIOMinimumSegmentAlignmentByteCountKey, osNumber);
    osNumber->release();
  }
}

