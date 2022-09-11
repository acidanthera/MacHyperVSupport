//
//  HyperVStorage.cpp
//  Hyper-V storage driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
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
  hvDevice = OSDynamicCast(HyperVVMBusDevice, getProvider());
  if (hvDevice == NULL) {
    return false;
  }
  hvDevice->retain();
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
#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_5
  if (getKernelVersion() >= KernelVersion::Leopard) {
#endif
    hvDevice->installPacketActions(this, OSMemberFunctionCast(HyperVVMBusDevice::PacketReadyAction, this, &HyperVStorage::handlePacket), OSMemberFunctionCast(HyperVVMBusDevice::WakePacketAction, this, &HyperVStorage::wakePacketHandler), PAGE_SIZE);
#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_5
  } else {
    EnableInterrupt();
  }
#endif
  
  //
  // Configure the channel.
  //
  if (hvDevice->openVMBusChannel(kHyperVStorageRingBufferSize, kHyperVStorageRingBufferSize) != kIOReturnSuccess) {
    return false;
  }
  
  //
  // Begin controller initialization.
  //
  clearPacket(&packet);
  packet.operation = kHyperVStoragePacketOperationBeginInitialization;
  if (executeCommand(&packet, true) != kIOReturnSuccess) {
    return false;
  }
  
  //
  // Negotiate protocol version.
  //
  for (UInt32 i = 0; i < ARRAY_SIZE(storageProtocols); i++) {
    clearPacket(&packet);
    packet.operation                  = kHyperVStoragePacketOperationQueryProtocolVersion;
    packet.protocolVersion.majorMinor = storageProtocols[i].protocolVersion;
    packet.protocolVersion.revision   = 0; // Revision is zero for non-Windows.
    
    if (executeCommand(&packet, false) != kIOReturnSuccess) {
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
  clearPacket(&packet);
  packet.operation = kHyperVStoragePacketOperationQueryProperties;
  if (executeCommand(&packet, true) != kIOReturnSuccess) {
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
  clearPacket(&packet);
  packet.operation = kHyperVStoragePacketOperationEndInitialization;
  if (executeCommand(&packet, true) != kIOReturnSuccess) {
    return false;
  }
  
  
  hvDevice->allocateDmaBuffer(&dmaBufTest, 32000000);
  
  segs64 = (IODMACommand::Segment64*) IOMalloc(sizeof (IODMACommand::Segment64) * maxPageSegments);

  //
  // Populate HBA properties.
  //
  setHBAInfo();
  
  HVSYSLOG("Initialized Hyper-V Synthetic Storage controller");
  return true;
}

void HyperVStorage::TerminateController() {
  HVDBGLOG("Controller is terminated");
}

bool HyperVStorage::StartController() {
  HVDBGLOG("Controller is now started");
  return true;
}

void HyperVStorage::StopController() {
  HVDBGLOG("Controller is now stopped");
}

bool HyperVStorage::DoesHBAPerformDeviceManagement() {
  //
  // Let the OS handle device management.
  //
  return false;
}

bool HyperVStorage::DoesHBASupportSCSIParallelFeature(SCSIParallelFeature theFeature) {
  HVDBGLOG("start");
  return false;
}

bool HyperVStorage::InitializeTargetForID(SCSITargetIdentifier targetID) {
  return (targetID < kHyperVStorageMaxTargets);
}

void HyperVStorage::HandleInterruptRequest() {
#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_5
  //
  // macOS 10.4 configures the interrupt in the superclass, invoke
  // our interrupt handler here.
  //
 // handleInterrupt(this, NULL, 0);
#endif
}

SCSIInitiatorIdentifier HyperVStorage::ReportInitiatorIdentifier() {
  return kHyperVStorageMaxLuns;
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
  return NULL;
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
  //HVDBGLOG("start");
  
  SCSICommandDescriptorBlock cdbData;
  
  GetCommandDescriptorBlock(parallelRequest, &cdbData);
  
  //HVDBGLOG("######## Attempting CDB 0x%X for target %u, LUN %u ##########", cdbData[0], GetTargetIdentifier(parallelRequest), GetLogicalUnitNumber(parallelRequest));
 // HVDBGLOG("CDB %X %X %X %X %X %X", cdbData[0], cdbData[1], cdbData[2], cdbData[3], cdbData[4], cdbData[5]);
  
  HyperVStoragePacket packet;
  clearPacket(&packet);
  
  packet.scsiRequest.targetID = GetTargetIdentifier(parallelRequest);
  packet.scsiRequest.lun = GetLogicalUnitNumber(parallelRequest);
  
  HyperVStorageSCSIRequest *srb = &packet.scsiRequest;
  
  srb->win8Extension.srbFlags |= 0x00000008;
  
  UInt8 dataDirection = GetDataTransferDirection(parallelRequest);
  switch (dataDirection) {
    case kSCSIDataTransfer_NoDataTransfer:
      srb->dataIn = kHyperVStorageSCSIRequestTypeUnknown;
      
      break;
      
    case kSCSIDataTransfer_FromInitiatorToTarget:
      srb->dataIn = kHyperVStorageSCSIRequestTypeWrite;
      srb->win8Extension.srbFlags |= 0x00000080;
      break;
      
    case kSCSIDataTransfer_FromTargetToInitiator:
      srb->dataIn = kHyperVStorageSCSIRequestTypeRead;
      srb->win8Extension.srbFlags |= 0x00000040;
      break;
      
    default:
      HVDBGLOG("Bad data direction 0x%X", dataDirection);
      return kSCSIServiceResponse_FUNCTION_REJECTED;
  };
  
  srb->cdbLength = GetCommandDescriptorBlockSize(parallelRequest);
  memcpy(srb->cdb, cdbData, srb->cdbLength);
 // HVDBGLOG("CDB is %u bytes", srb->cdbLength);
  
  srb->length = sizeof (HyperVStorageSCSIRequest);
  srb->senseInfoLength = senseBufferSize;
  
  packet.operation = kHyperVStoragePacketOperationExecuteSRB;
  packet.flags = kHyperVStoragePacketFlagRequestCompletion;
  
  currentTask = parallelRequest;

  
  if (dataDirection != kSCSIDataTransfer_NoDataTransfer) {
    
    VMBusPacketMultiPageBuffer *pagePacket;
    UInt32 pagePacketLength;
    if (!prepareDataTransfer(parallelRequest, &pagePacket, &pagePacketLength)) {
      return kSCSIServiceResponse_FUNCTION_REJECTED;
    }
    
    UInt64 lengthPhys = GetRequestedDataTransferCount(parallelRequest);
    packet.scsiRequest.dataTransferLength = (UInt32) lengthPhys;
    
    hvDevice->writeGPADirectMultiPagePacket(&packet, sizeof (packet) - packetSizeDelta, true, pagePacket, pagePacketLength);
  } else {
    hvDevice->writeInbandPacket(&packet, sizeof (packet) - packetSizeDelta, true);
  }
  currentTask = parallelRequest;
  
  /*if (dataDirection != kSCSIDataTransfer_NoDataTransfer && request.mbpArray != NULL) {
    IOFree(request.mbpArray, request.mbpArrayLength);
  }*/

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

