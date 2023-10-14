//
//  HyperVPCIBridgeRegs.hpp
//  Hyper-V PCI passthrough device support
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVPCIBridgeRegs_h
#define HyperVPCIBridgeRegs_h

#define kHyperVPCIBridgeRingBufferSize  (4 * PAGE_SIZE)
#define kHyperVPCIBridgeResponsePacketSize 256

// First page selects the function, second page is PCI config space of selected function.
#define kHyperVPCIBridgeWindowSize      (2 * PAGE_SIZE)
#define kHyperVPCIConfigPageOffset      PAGE_SIZE

#define kHyperVPCIBarCount              6
#define kHyperVPCIBarSpaceIO            0x1
#define kHyperVPCIBarMemoryType64Bit    0x4
#define kHyperVPCIBarMemoryMask         ~(0x0FUL)

//
// Protocol versions.
//
typedef enum : UInt32 {
  kHyperVPCIBridgeProtocolVersion1  = 0x10001,
} HyperVPCIBridgeProtocolVersion;

//
// Message types.
//
typedef enum : UInt32 {
  kHyperVPCIBridgeMessageTypeBase                       = 0x42490000,
  kHyperVPCIBridgeMessageTypeBusRelations               = kHyperVPCIBridgeMessageTypeBase + 0x0,
  kHyperVPCIBridgeMessageTypeQueryBusRelations          = kHyperVPCIBridgeMessageTypeBase + 0x1,
  kHyperVPCIBridgeMessageTypePowerStateChange           = kHyperVPCIBridgeMessageTypeBase + 0x4,
  kHyperVPCIBridgeMessageTypeQueryResourceRequirements  = kHyperVPCIBridgeMessageTypeBase + 0x5,
  kHyperVPCIBridgeMessageTypeQueryResourceResources     = kHyperVPCIBridgeMessageTypeBase + 0x6,
  kHyperVPCIBridgeMessageTypeBusD0Entry                 = kHyperVPCIBridgeMessageTypeBase + 0x7,
  kHyperVPCIBridgeMessageTypeBusD0Exit                  = kHyperVPCIBridgeMessageTypeBase + 0x8,
  kHyperVPCIBridgeMessageTypeReadBlock                  = kHyperVPCIBridgeMessageTypeBase + 0x9,
  kHyperVPCIBridgeMessageTypeWriteBlock                 = kHyperVPCIBridgeMessageTypeBase + 0xA,
  kHyperVPCIBridgeMessageTypeEject                      = kHyperVPCIBridgeMessageTypeBase + 0xB,
  kHyperVPCIBridgeMessageTypeQueryStop                  = kHyperVPCIBridgeMessageTypeBase + 0xC,
  kHyperVPCIBridgeMessageTypeReEnable                   = kHyperVPCIBridgeMessageTypeBase + 0xD,
  kHyperVPCIBridgeMessageTypeQueryStopFailed            = kHyperVPCIBridgeMessageTypeBase + 0xE,
  kHyperVPCIBridgeMessageTypeEjectionComplete           = kHyperVPCIBridgeMessageTypeBase + 0xF,
  kHyperVPCIBridgeMessageTypeResourcesAssigned          = kHyperVPCIBridgeMessageTypeBase + 0x10,
  kHyperVPCIBridgeMessageTypeResourcesReleased          = kHyperVPCIBridgeMessageTypeBase + 0x11,
  kHyperVPCIBridgeMessageTypeInvalidateBlock            = kHyperVPCIBridgeMessageTypeBase + 0x12,
  kHyperVPCIBridgeMessageTypeQueryProtocolVersion       = kHyperVPCIBridgeMessageTypeBase + 0x13,
  kHyperVPCIBridgeMessageTypeCreateInterruptMessage     = kHyperVPCIBridgeMessageTypeBase + 0x14,
  kHyperVPCIBridgeMessageTypeDeleteInterruptMessage     = kHyperVPCIBridgeMessageTypeBase + 0x15,
  kHyperVPCIBridgeMessageTypeResourcesAssigned2         = kHyperVPCIBridgeMessageTypeBase + 0x16,
  kHyperVPCIBridgeMessageTypeCreateInterruptMessage2    = kHyperVPCIBridgeMessageTypeBase + 0x17,
  kHyperVPCIBridgeMessageTypeDeleteInterruptMessage2    = kHyperVPCIBridgeMessageTypeBase + 0x18,
  kHyperVPCIBridgeMessageTypeBusRelations2              = kHyperVPCIBridgeMessageTypeBase + 0x19,
} HyperVPCIBridgeMessageType;

//
// PCI structures.
//
typedef union __attribute__((packed)) {
  struct {
    UInt16  minorVersion;
    UInt16  majorVersion;
  } parts;
  UInt32    version;
} HyperVPCIVersion;

// Function number as used in Windows.
typedef union __attribute__((packed)) {
  struct {
    UInt32  device    : 5;
    UInt32  function  : 3;
    UInt32  reserved  : 24;
  } bits;
  UInt32    slot;
} HyperVPCISlotEncoding;

// PCI function description v1.
typedef struct __attribute__((packed)) {
  UInt16                vendorId;
  UInt16                deviceId;
  UInt8                 revision;
  UInt8                 progInterface;
  UInt8                 subClass;
  UInt8                 baseClass;
  UInt16                subVendorId;
  UInt16                subDeviceId;
  HyperVPCISlotEncoding slot;
  UInt32                serialNumber;
} HyperVPCIFunctionDescription;

//
// Outgoing messages.
//
// Message header.
typedef struct __attribute__((packed)) {
  HyperVPCIBridgeMessageType type;
} HyperVPCIBridgeMessageHeader;

// Message to specific slot (child device).
typedef struct __attribute__((packed)) {
  HyperVPCIBridgeMessageHeader  header;
  HyperVPCISlotEncoding         slot;
} HyperVPCIBridgeChildMessage;

// Protocol version request.
typedef struct __attribute__((packed)) {
  HyperVPCIBridgeMessageHeader    header;
  HyperVPCIBridgeProtocolVersion  version;
} HyperVPCIBridgeMessageProtocolVersionRequest;

// D0 entry (device on).
typedef struct __attribute__((packed)) {
  HyperVPCIBridgeMessageHeader  header;
  UInt32                        reserved;
  UInt64                        mmioBase;
} HyperVPCIBridgeMessagePCIBusD0Entry;

// Resources assigned v1.
typedef struct __attribute__((packed)) {
  HyperVPCIBridgeMessageHeader  header;
  HyperVPCISlotEncoding         slot;
  UInt8                         memoryRange[0x14][6];
  UInt32                        msiDescriptors;
  UInt32                        reserved[4];
} HyperVPCIBridgeMessageResourcesAssigned;

// Create interrupt
typedef struct __attribute__((packed))  {
  HyperVPCIBridgeMessageHeader  header;
  HyperVPCISlotEncoding         slot;
  UInt8 vector;
  UInt8 deliveryMode;
  UInt16 vectorCount;
  UInt32  reserved;
  UInt64  cpuMask;
} HyperVPCIBridgeMessageCreateInterrupt;

//
// Incoming messages.
//
// Message header.
typedef struct __attribute__((packed)) {
  HyperVPCIBridgeMessageType  type;
} HyperVPCIBridgeIncomingMessageHeader;

// Bus relations v1 (inband response).
typedef struct __attribute__((packed)) {
  HyperVPCIBridgeIncomingMessageHeader  header;
  UInt32                                functionCount;
  HyperVPCIFunctionDescription          functions[];
} HyperVPCIBridgeIncomingMessageBusRelations;

// Resource request response (completion response).
typedef struct __attribute__((packed)) {
  SInt32              status;
  UInt32              probedBARs[kHyperVPCIBarCount];
} HyperVPCIBridgeQueryResourceRequirementsResponse;

// Create interrupt
typedef struct __attribute__((packed))  {
  SInt32              status;
  UInt32  reserved2;
  UInt16  reserved;
  UInt16 vectorCount;
  
  UInt32  data;
  UInt64  address;
} HyperVPCIBridgeMessageCreateInterruptResponse;

#endif
