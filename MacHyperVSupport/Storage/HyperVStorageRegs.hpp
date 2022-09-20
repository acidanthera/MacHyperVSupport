//
//  HyperVStorageRegs.hpp
//  Hyper-V storage driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVStorageRegs_h
#define HyperVStorageRegs_h

#define kHyperVStorageRingBufferSize 0x200000// 0x1000000//0xF0000//0xF000 //(0x1000000)// (0x9000)

#define kHyperVStorageMaxCommandLength        0x10
#define kHyperVStoragePostWin7SenseBufferSize 0x14
#define kHyperVStoragePreWin8SenseBufferSize  0x12

#define kHyperVStorageSenseBufferSize         0x14
#define kHyperVStorageMaxBufferLengthPadding  0x14

#define kHyperVStorageVendor                  "Microsoft"
#define kHyperVStorageProduct                 "Hyper-V SCSI Controller"

#define kHyperVStorageMaxTargets              64
#define kHyperVStorageMaxLuns                 1

#define kHyperVStorageSegmentSize             PAGE_SIZE
#define kHyperVStorageSegmentAlignment        0xFFFFFFFFFFFFF000ULL
#define kHyperVStorageSegmentBits             64

#define kHyperVSRBStatusSuccess         0x01
#define kHyperVSRBStatusAborted         0x02
#define kHyperVSRBStatusError           0x04
#define kHyperVSRBStatusInvalidLUN      0x20
#define kHyperVSRBStatusAutosenseValid  0x80

//
// Packet operations.
//
typedef enum : UInt32 {
  kHyperVStoragePacketOperationCompleteIO             = 1,
  kHyperVStoragePacketOperationRemoveDevice           = 2,
  kHyperVStoragePacketOperationExecuteSRB             = 3,
  kHyperVStoragePacketOperationResetLUN               = 4,
  kHyperVStoragePacketOperationResetAdapter           = 5,
  kHyperVStoragePacketOperationResetBus               = 6,
  kHyperVStoragePacketOperationBeginInitialization    = 7,
  kHyperVStoragePacketOperationEndInitialization      = 8,
  kHyperVStoragePacketOperationQueryProtocolVersion   = 9,
  kHyperVStoragePacketOperationQueryProperties        = 10,
  kHyperVStoragePacketOperationEnumerateBus           = 11,
  kHyperVStoragePacketOperationFCHBAData              = 12,
  kHyperVStoragePacketOperationCreateSubChannels      = 13
} HyperVStoragePacketOperation;

//
// SCSI request extension for Windows 8.
//
typedef struct __attribute__((packed)) {
  UInt16  reserved;
  UInt8   queueTag;
  UInt8   queueAction;
  UInt32  srbFlags;
  UInt32  timeoutValue;
  UInt32  queueSortEy;
} HyperVStorageSCSIRequestWin8Extension;

typedef enum : UInt8 {
  kHyperVStorageSCSIRequestTypeWrite    = 0,
  kHyperVStorageSCSIRequestTypeRead     = 1,
  kHyperVStorageSCSIRequestTypeUnknown  = 2
} HyperVStorageSCSIRequestType;

//
// SCSI request packet.
//
typedef struct __attribute__((packed)) {
  UInt16  length;
  UInt8   srbStatus;
  UInt8   scsiStatus;
  
  UInt8   portNumber;
  UInt8   pathID;
  UInt8   targetID;
  UInt8   lun;
  
  UInt8   cdbLength;
  UInt8   senseInfoLength;
  HyperVStorageSCSIRequestType   dataIn;
  UInt8   reserved;
  
  UInt32  dataTransferLength;
  
  union {
    SCSICommandDescriptorBlock cdb;
    UInt8 senseData[kHyperVStorageSenseBufferSize];
    UInt8 reservedData[kHyperVStorageMaxBufferLengthPadding];
  };
  
  HyperVStorageSCSIRequestWin8Extension win8Extension;
} HyperVStorageSCSIRequest;

#define kHyperVStorageFlagSupportsMultiChannel    1

//
// Channel properties packet.
//
typedef struct __attribute__((packed)) {
  UInt32  reserved1;
  UInt16  maxChannelCount;
  UInt16  reserved2;
  
  UInt32  flags;
  UInt32  maxTransferBytes;
  
  UInt64  reserved3;
} HyperVStorageChannelProperties;

//
// Storage protocol negotiation packet.
//
typedef struct __attribute__((packed)) {
  //
  // Major and minor version.
  //
  UInt16  majorMinor;
  //
  // Revision number used only for Windows, set to zero.
  //
  UInt16  revision;
} HyperVStorageProtocolVersion;

//
// WWN packet for Fibre Channel.
//
typedef struct __attribute__((packed)) {
  UInt8   primaryActive;
  UInt8   reserved[3];
  UInt8   primaryPortWWN[8];
  UInt8   primaryNodeWWN[8];
  UInt8   secondaryPortWWN[8];
  UInt8   secondaryNodeWWN[8];
} HyperVStorageFibreChannelWWN;

#define kHyperVStoragePacketFlagRequestCompletion   1
#define kHyperVStoragePacketSuccess                 0

//
// Data packet structure.
//
typedef struct __attribute__((packed)) {
  //
  // Operation type.
  //
  HyperVStoragePacketOperation    operation;
  //
  // Flags.
  //
  UInt32                          flags;
  //
  // Status returned from Hyper-V.
  //
  UInt32                          status;
  //
  // Data payload.
  //
  union {
    HyperVStorageSCSIRequest        scsiRequest;
    HyperVStorageChannelProperties  storageChannelProperties;
    HyperVStorageProtocolVersion    protocolVersion;
    HyperVStorageFibreChannelWWN    fibreChannelWWN;
    
    UInt16  subChannelCount;
    UInt8   buffer[0x34];
  };
} HyperVStoragePacket;

//
// Driver data
//
//
// Storage protocol version
//
#define HYPERV_STORAGE_PROTCOL_VERSION(major, minor)    ((((major) & 0xFF) << 8) | ((minor) & 0xFF))
#define HYPERV_STORAGE_PROTCOL_VERSION_MAJOR(ver)       (((ver) >> 8) & 0xFF)
#define HYPERV_STORAGE_PROTCOL_VERSION_MINOR(ver)       ((ver) & 0xFF)

#define kHyperVStorageVersionWin2008  HYPERV_STORAGE_PROTCOL_VERSION(2, 0)
#define kHyperVStorageVersionWin7     HYPERV_STORAGE_PROTCOL_VERSION(4, 2)
#define kHyperVStorageVersionWin8     HYPERV_STORAGE_PROTCOL_VERSION(5, 1)
#define kHyperVStorageVersionWin8_1   HYPERV_STORAGE_PROTCOL_VERSION(6, 0)
#define kHyperVStorageVersionWin10    HYPERV_STORAGE_PROTCOL_VERSION(6, 2)

typedef struct {
  UInt32  protocolVersion;
  UInt32  senseBufferSize;
  UInt32  packetSizeDelta;
} HyperVStorageProtocol;

#endif
