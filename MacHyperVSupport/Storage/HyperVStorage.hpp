//
//  HyperVStorage.hpp
//  Hyper-V storage driver
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#ifndef HyperVStorage_hpp
#define HyperVStorage_hpp

#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/scsi/SCSICommandOperationCodes.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/IOKitKeys.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#include <IOKit/scsi/spi/IOSCSIParallelInterfaceController.h>
#pragma clang diagnostic pop

#include "HyperVVMBusDevice.hpp"
#include "HyperVStorageRegs.hpp"

class HyperVStorage : public IOSCSIParallelInterfaceController {
  OSDeclareDefaultStructors(HyperVStorage);
  HVDeclareLogFunctionsVMBusChild("stor");
  typedef IOSCSIParallelInterfaceController super;

private:
  HyperVVMBusDevice *_hvDevice = nullptr;

  //
  // Storage protocol.
  //
  UInt32 _protocolVersion = 0;
  UInt32 _senseBufferSize = 0;
  UInt32 _packetSizeDelta = 0;
  bool   _isIDE           = false;
  UInt32 _maxLuns         = kHyperVStorageMaxLunsSCSI;
  UInt8  _targetId        = 0;
  
  bool   _subChannelsSupported = false;
  UInt16 _maxSubChannels       = 0;
  UInt32 _maxTransferBytes     = 0;
  UInt32 _maxPageSegments      = 0;

  //
  // Segments for DMA transfers.
  //
  IODMACommand::Segment64 *_segs64 = nullptr;

  //
  // Thread for disk enumeration.
  //
  thread_call_t _scanSCSIDiskThread = nullptr;

  //
  // Packets and I/O.
  //
  bool wakePacketHandler(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength);
  void handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength);
  void handleIOCompletion(UInt64 transactionId, HyperVStoragePacket *packet);
  IOReturn sendStorageCommand(HyperVStoragePacket *packet, bool checkCompletion);
  IOReturn prepareDataTransfer(SCSIParallelTaskIdentifier parallelRequest, VMBusPacketMultiPageBuffer **pagePacket, UInt32 *pagePacketLength);
  void completeDataTransfer(SCSIParallelTaskIdentifier parallelRequest, HyperVStoragePacket *packet);

  //
  // Disk enumeration and misc.
  //
  void setHBAInfo();
  IOReturn connectStorage();
  bool checkSCSIDiskPresent(UInt8 diskId);
  void startDiskEnumeration();
  void scanSCSIDisks();

protected:
  //
  // IOSCSIParallelInterfaceController overrides.
  //
  bool InitializeController() APPLE_KEXT_OVERRIDE;
  void TerminateController() APPLE_KEXT_OVERRIDE;
  bool StartController() APPLE_KEXT_OVERRIDE;
  void StopController() APPLE_KEXT_OVERRIDE;
  bool DoesHBAPerformDeviceManagement() APPLE_KEXT_OVERRIDE;
  void HandleInterruptRequest() APPLE_KEXT_OVERRIDE;
  SCSIInitiatorIdentifier ReportInitiatorIdentifier() APPLE_KEXT_OVERRIDE;
  SCSIDeviceIdentifier ReportHighestSupportedDeviceID() APPLE_KEXT_OVERRIDE;
  UInt32 ReportMaximumTaskCount() APPLE_KEXT_OVERRIDE;
  UInt32 ReportHBASpecificTaskDataSize() APPLE_KEXT_OVERRIDE;
  UInt32 ReportHBASpecificDeviceDataSize() APPLE_KEXT_OVERRIDE;
  IOInterruptEventSource *CreateDeviceInterrupt(IOInterruptEventSource::Action action,
                                                IOFilterInterruptEventSource::Filter filter,
                                                IOService *provider) APPLE_KEXT_OVERRIDE;
  bool InitializeDMASpecification(IODMACommand *command) APPLE_KEXT_OVERRIDE;

public:
  //
  // IOSCSIParallelInterfaceController overrides.
  //
  bool DoesHBASupportSCSIParallelFeature(SCSIParallelFeature theFeature) APPLE_KEXT_OVERRIDE;
  bool InitializeTargetForID(SCSITargetIdentifier targetID) APPLE_KEXT_OVERRIDE;
  SCSILogicalUnitNumber ReportHBAHighestLogicalUnitNumber() APPLE_KEXT_OVERRIDE;
  SCSIServiceResponse AbortTaskRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL, SCSITaggedTaskIdentifier theQ) APPLE_KEXT_OVERRIDE;
  SCSIServiceResponse AbortTaskSetRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL) APPLE_KEXT_OVERRIDE;
  SCSIServiceResponse ClearACARequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL) APPLE_KEXT_OVERRIDE;
  SCSIServiceResponse ClearTaskSetRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL) APPLE_KEXT_OVERRIDE;
  SCSIServiceResponse LogicalUnitResetRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL) APPLE_KEXT_OVERRIDE;
  SCSIServiceResponse TargetResetRequest(SCSITargetIdentifier theT) APPLE_KEXT_OVERRIDE;
  SCSIServiceResponse ProcessParallelTask(SCSIParallelTaskIdentifier parallelRequest) APPLE_KEXT_OVERRIDE;
  void ReportHBAConstraints(OSDictionary *constraints) APPLE_KEXT_OVERRIDE;
};

#endif
