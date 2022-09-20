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
  
  UInt32                  protocolVersion;
  UInt32                  senseBufferSize;
  UInt32                  packetSizeDelta;
  
  bool                    subChannelsSupported;
  UInt16                  maxSubChannels;
  UInt32                  maxTransferBytes;
  UInt32                  maxPageSegments;

  IODMACommand::Segment64     *segs64;
  
  bool wakePacketHandler(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength);
  void handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength);
  void handleIOCompletion(UInt64 transactionId, HyperVStoragePacket *packet);
  
  IOReturn sendStorageCommand(HyperVStoragePacket *packet, bool checkCompletion);
  IOReturn prepareDataTransfer(SCSIParallelTaskIdentifier parallelRequest, VMBusPacketMultiPageBuffer **pagePacket, UInt32 *pagePacketLength);
  void completeDataTransfer(SCSIParallelTaskIdentifier parallelRequest, HyperVStoragePacket *packet);
  
  void setHBAInfo();
  
  //
  // Disk enumeration.
  //
  thread_call_t scanSCSIDiskThread;
  bool checkSCSIDiskPresent(UInt8 diskId);
  void startDiskEnumeration();
  void scanSCSIDisks();

protected:
  //
  // IOSCSIParallelInterfaceController overrides.
  //
  virtual bool InitializeController() APPLE_KEXT_OVERRIDE;
  virtual void TerminateController() APPLE_KEXT_OVERRIDE;
  virtual bool StartController() APPLE_KEXT_OVERRIDE;
  virtual void StopController() APPLE_KEXT_OVERRIDE;
  virtual bool DoesHBAPerformDeviceManagement() APPLE_KEXT_OVERRIDE;
  virtual void HandleInterruptRequest() APPLE_KEXT_OVERRIDE;
  virtual SCSIInitiatorIdentifier ReportInitiatorIdentifier() APPLE_KEXT_OVERRIDE;
  virtual SCSIDeviceIdentifier ReportHighestSupportedDeviceID() APPLE_KEXT_OVERRIDE;
  virtual UInt32 ReportMaximumTaskCount() APPLE_KEXT_OVERRIDE;
  virtual UInt32 ReportHBASpecificTaskDataSize() APPLE_KEXT_OVERRIDE;
  virtual UInt32 ReportHBASpecificDeviceDataSize() APPLE_KEXT_OVERRIDE;
  
  virtual IOInterruptEventSource *CreateDeviceInterrupt(IOInterruptEventSource::Action action,
                                                        IOFilterInterruptEventSource::Filter filter,
                                                        IOService *provider) APPLE_KEXT_OVERRIDE;
  
  virtual bool InitializeDMASpecification(IODMACommand *command) APPLE_KEXT_OVERRIDE;
  
public:
  //
  // IOSCSIParallelInterfaceController overrides.
  //
  virtual bool DoesHBASupportSCSIParallelFeature(SCSIParallelFeature theFeature) APPLE_KEXT_OVERRIDE;
  virtual bool InitializeTargetForID(SCSITargetIdentifier targetID) APPLE_KEXT_OVERRIDE;
  virtual SCSILogicalUnitNumber ReportHBAHighestLogicalUnitNumber() APPLE_KEXT_OVERRIDE;
  virtual SCSIServiceResponse AbortTaskRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL, SCSITaggedTaskIdentifier theQ) APPLE_KEXT_OVERRIDE;
  virtual SCSIServiceResponse AbortTaskSetRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL) APPLE_KEXT_OVERRIDE;
  virtual SCSIServiceResponse ClearACARequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL) APPLE_KEXT_OVERRIDE;
  virtual SCSIServiceResponse ClearTaskSetRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL) APPLE_KEXT_OVERRIDE;
  virtual SCSIServiceResponse LogicalUnitResetRequest(SCSITargetIdentifier theT, SCSILogicalUnitNumber theL) APPLE_KEXT_OVERRIDE;
  virtual SCSIServiceResponse TargetResetRequest(SCSITargetIdentifier theT) APPLE_KEXT_OVERRIDE;
  virtual SCSIServiceResponse ProcessParallelTask(SCSIParallelTaskIdentifier parallelRequest) APPLE_KEXT_OVERRIDE;
  
  virtual void ReportHBAConstraints(OSDictionary *constraints) APPLE_KEXT_OVERRIDE;
};

#endif
