//
//  HyperVStorage.cpp
//  Hyper-V storage driver
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#include "HyperVStorage.hpp"

OSDefineMetaClassAndStructors(HyperVStorage, super);

bool HyperVStorage::InitializeController() {
  bool                result = false;
  IOReturn            status;

  //
  // Get parent VMBus device object.
  //
  _hvDevice = OSDynamicCast(HyperVVMBusDevice, getProvider());
  if (_hvDevice == nullptr) {
    HVSYSLOG("Provider is not HyperVVMBusDevice");
    return false;
  }
  _hvDevice->retain();

  HVCheckDebugArgs();
  HVDBGLOG("Initializing Hyper-V Synthetic Storage");

  if (HVCheckOffArg()) {
    HVSYSLOG("Disabling Hyper-V Synthetic Storage due to boot arg");
    OSSafeReleaseNULL(_hvDevice);
    return false;
  }

  //
  // Assume we are on an older host and take off the Windows 8 extensions by default.
  //
  _packetSizeDelta = sizeof (HyperVStorageSCSIRequestWin8Extension);

  do {
    //
    // Install packet handler.
    // macOS 10.4 always configures the interrupt in the superclass, do
    // not configure the interrupt ourselves in that case.
    //
    status = _hvDevice->installPacketActions(this, OSMemberFunctionCast(HyperVVMBusDevice::PacketReadyAction, this, &HyperVStorage::handlePacket),
                                             OSMemberFunctionCast(HyperVVMBusDevice::WakePacketAction, this, &HyperVStorage::wakePacketHandler),
                                             PAGE_SIZE, getKernelVersion() >= KernelVersion::Leopard);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to install packet handler with status 0x%X", status);
      break;
    }

#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_5
    if (getKernelVersion() < KernelVersion::Leopard) {
      EnableInterrupt();
    }
#endif

    //
    // Open VMBus channel and connect to storage.
    //
    status = _hvDevice->openVMBusChannel(kHyperVStorageRingBufferSize, kHyperVStorageRingBufferSize);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to open VMBus channel with status 0x%X", status);
      break;
    }

    status = connectStorage();
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to connect to storage device with status 0x%X", status);
      break;
    }

    //
    // Initialize segments used for DMA.
    //
    _segs64 = (IODMACommand::Segment64*) IOMalloc(sizeof (IODMACommand::Segment64) * _maxPageSegments);
    if (_segs64 == nullptr) {
      HVSYSLOG("Failed to initialize segments");
      break;
    }
    
    //
    // Populate HBA properties and create disk enumeration thread.
    //
    setHBAInfo();
    _scanSCSIDiskThread = thread_call_allocate(OSMemberFunctionCast(thread_call_func_t, this, &HyperVStorage::scanSCSIDisks), this);
    if (_scanSCSIDiskThread == nullptr) {
      HVSYSLOG("Failed to create disk enumeration thread");
      break;
    }

    result = true;
    HVDBGLOG("Initialized Hyper-V Synthetic Storage");
  } while (false);

  if (!result) {
    TerminateController();
  }
  return result;
}

void HyperVStorage::TerminateController() {
  HVDBGLOG("Stopping Hyper-V Synthetic Storage");

  if (_scanSCSIDiskThread != nullptr) {
    thread_call_free(_scanSCSIDiskThread);
  }

  if (_hvDevice != nullptr) {
    _hvDevice->closeVMBusChannel();
    _hvDevice->uninstallPacketActions();
    OSSafeReleaseNULL(_hvDevice);
  }

  if (_segs64 != nullptr) {
    IOFree(_segs64, sizeof (IODMACommand::Segment64) * _maxPageSegments);
  }
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
  // Ensure target ID is under maximum LUN count.
  //
  return (targetID < _maxLuns);
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
  return _maxLuns;
}

SCSIDeviceIdentifier HyperVStorage::ReportHighestSupportedDeviceID() {
  // Each Hyper-V LUN is treated as a separate target under macOS.
  return _maxLuns - 1;
}

UInt32 HyperVStorage::ReportMaximumTaskCount() {
  HVDBGLOG("start");
  return 1;
}

UInt32 HyperVStorage::ReportHBASpecificTaskDataSize() {
  HVDBGLOG("start");
  return sizeof (VMBusPacketMultiPageBuffer) + (sizeof (UInt64) * _maxPageSegments); //32 * 4096;
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
                                        IODMACommand::kMapped, _maxTransferBytes, kHyperVStorageSegmentSize);
}

SCSILogicalUnitNumber HyperVStorage::ReportHBAHighestLogicalUnitNumber() {
  // Report max LUN of 0 to macOS.
  return 0;
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
    HVSYSLOG("LUN is non-zero");
    return kSCSIServiceResponse_FUNCTION_REJECTED;
  }

  //
  // Create SCSI command execution packet.
  // Each LUN is represented in macOS as a separate target, use that target ID for the LUN here.
  //
  packet.operation = kHyperVStoragePacketOperationExecuteSRB;
  packet.flags     = kHyperVStoragePacketFlagRequestCompletion;

  packet.scsiRequest.targetID                = _targetId;
  packet.scsiRequest.lun                     = GetTargetIdentifier(parallelRequest);
  packet.scsiRequest.win8Extension.srbFlags |= 0x00000008;
  packet.scsiRequest.length                  = sizeof (packet.scsiRequest) - _packetSizeDelta;
  packet.scsiRequest.senseInfoLength         = _senseBufferSize;

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
      HVSYSLOG("Bad data direction 0x%X", dataDirection);
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
      HVSYSLOG("Failed to prepare data transfer with status 0x%X", status);
      return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
    }

    packet.scsiRequest.dataTransferLength = (UInt32) GetRequestedDataTransferCount(parallelRequest);
    status = _hvDevice->writeGPADirectMultiPagePacket(&packet, sizeof (packet) - _packetSizeDelta, true,
                                                      pagePacket, pagePacketLength, nullptr, 0,
                                                      (UInt64)parallelRequest);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to send data SCSI packet with status 0x%X", status);
      return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
    }
  } else {
    status = _hvDevice->writeInbandPacketWithTransactionId(&packet, sizeof (packet) - _packetSizeDelta, (UInt64)parallelRequest, true);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to send non-data SCSI packet with status 0x%X", status);
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
  osNumber = OSNumber::withNumber(_maxPageSegments, 32);
  if (osNumber != nullptr) {
    constraints->setObject(kIOMaximumSegmentCountReadKey, osNumber);
    constraints->setObject(kIOMaximumSegmentCountWriteKey, osNumber);
    osNumber->release();
  }

  osNumber = OSNumber::withNumber(kHyperVStorageSegmentSize, 32);
  if (osNumber != nullptr) {
    constraints->setObject(kIOMaximumSegmentByteCountReadKey, osNumber);
    constraints->setObject(kIOMaximumSegmentByteCountWriteKey, osNumber);
    osNumber->release();
  }

  osNumber = OSNumber::withNumber(kHyperVStorageSegmentAlignment, 64);
  if (osNumber != nullptr) {
    constraints->setObject(kIOMinimumHBADataAlignmentMaskKey, osNumber);
    osNumber->release();
  }

  osNumber = OSNumber::withNumber(kHyperVStorageSegmentBits, 32);
  if (osNumber != nullptr) {
    constraints->setObject(kIOMaximumSegmentAddressableBitCountKey, osNumber);
    osNumber->release();
  }

  osNumber = OSNumber::withNumber(4, 32);
  if (osNumber != nullptr) {
    constraints->setObject(kIOMinimumSegmentAlignmentByteCountKey, osNumber);
    osNumber->release();
  }
}

