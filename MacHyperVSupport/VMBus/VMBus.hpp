//
//  VMBus.hpp
//  Hyper-V VMBus core logic
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef VMBus_hpp
#define VMBus_hpp

#include "HyperV.hpp"

//
// Default VMBus interrupt slots on SynIC for messages and timers.
//
#define kVMBusInterruptMessage  2
#define kVMBusInterruptTimer    4

//
// Default VMBus connection IDs.
//
#define kVMBusConnIdMessage1    1
#define kVMBusConnIdEvent       2
#define kVMBusConnIdMessage4    4

//
// Max number of channels supported by driver.
//
#define kVMBusMaxChannels       256

#define VMBUS_CHANNEL_EVENT_INDEX(chan) (chan / 8)
#define VMBUS_CHANNEL_EVENT_MASK(chan)  (1 << (chan % 8))

//
// Linux and FreeBSD use this as a starting handle, but any non-zero value appears to work.
//
#define kHyperVGpadlNullHandle    0
#define kHyperVGpadlStartHandle   0xE1E10

//
// Version structure.
//
typedef struct __attribute__((packed)) {
  UInt16 major;
  UInt16 minor;
} VMBusVersion;

//
// VMBus protocol versions.
//
// 0.13 - Windows Server 2008
// 1.1  - Windows Server 2008 R2
// 2.4  - Windows 8 and Server 2012
// 3.0  - Windows 8.1 and Server 2012 R2
// 4.0  - Windows 10 v1607 and Server 2016
// 4.1  - Windows 10 v1709
// 5.0  - Windows 10 (newer)
// 5.1  - Windows 10 v1803
// 5.2  - Windows 10 v1809 and Server 2019
// 5.3  - Windows Server 2022
//
#define kVMBusVersionWS2008       ((0 << 16) | (13))
#define kVMBusVersionWIN7         ((1 << 16) | (1))
#define kVMBusVersionWIN8         ((2 << 16) | (4))
#define kVMBusVersionWIN8_1       ((3 << 16) | (0))
#define kVMBusVersionWIN10        ((4 << 16) | (0))
#define kVMBusVersionWIN10_V4_1   ((4 << 16) | (1))
#define kVMBusVersionWIN10_V5     ((5 << 16) | (0))
#define kVMBusVersionWIN10_V5_1   ((5 << 16) | (1))
#define kVMBusVersionWIN10_V5_2   ((5 << 16) | (2))
#define kVMBusVersionWIN10_V5_3   ((5 << 16) | (3))

#define VMBUS_VERSION_MAJOR(ver)  (((UInt32)(ver)) >> 16)
#define VMBUS_VERSION_MINOR(ver)  (((UInt32)(ver)) & 0xFFFF)

//
// VMBus message types.
//
typedef enum : UInt32 {
  kVMBusChannelMessageTypeInvalid               = 0,
  kVMBusChannelMessageTypeChannelOffer          = 1,
  kVMBusChannelMessageTypeRescindChannelOffer   = 2,
  kVMBusChannelMessageTypeRequestChannels       = 3,
  kVMBusChannelMessageTypeRequestChannelsDone   = 4,
  kVMBusChannelMessageTypeChannelOpen           = 5,
  kVMBusChannelMessageTypeChannelOpenResponse   = 6,
  kVMBusChannelMessageTypeChannelClose          = 7,
  kVMBusChannelMessageTypeGPADLHeader           = 8,
  kVMBusChannelMessageTypeGPADLBody             = 9,
  kVMBusChannelMessageTypeGPADLCreated          = 10,
  kVMBusChannelMessageTypeGPADLTeardown         = 11,
  kVMBusChannelMessageTypeGPADLTeardownResponse = 12,
  kVMBusChannelMessageTypeChannelFree           = 13,
  kVMBusChannelMessageTypeConnect               = 14,
  kVMBusChannelMessageTypeConnectResponse       = 15,
  kVMBusChannelMessageTypeDisconnect            = 16,
  kVMBusChannelMessageTypeDisconnectResponse    = 17,
  kVMBusChannelMessageType18                    = 18,
  kVMBusChannelMessageType19                    = 19,
  kVMBusChannelMessageType20                    = 20,
  kVMBusChannelMessageTypeTLConnect             = 21,
  kVMBusChannelMessageTypeChannelModify         = 22,
  kVMBusChannelMessageTypeTLConnectResponse     = 23,
  kVMBusChannelMessageTypeChannelModifyResponse = 24,
  kVMBusChannelMessageTypeMax                   = 24
} VMBusChannelMessageType;

//
// Message type table.
//
typedef struct {
  VMBusChannelMessageType type;
  UInt32                  size;
} VMBusMessageTypeTableEntry;

//
// VMBus message structures.
//
// Header
typedef struct __attribute__((packed)) {
  VMBusChannelMessageType   type;
  UInt32                    reserved;
} VMBusChannelMessageHeader;

//
// Parameterless messages.
//
// kVMBusChannelMessageTypeRequestChannels
// kVMBusChannelMessageTypeRequestChannelsDone
// kVMBusChannelMessageTypeDisconnect
typedef struct __attribute__((packed)) {
  VMBusChannelMessageHeader header;
} VMBusChannelMessage;

// The size of the user defined data buffer for non-pipe offers.
#define kVMBusChannelOfferMaxUserDefinedBytes       120

// The size of the user defined data buffer for pipe offers.
#define kVMBusChannelOfferMaxPipeUserDefinedBytes   116

// kVMBusChannelMessageTypeChannelOffer
typedef struct __attribute__((packed)) {
  VMBusChannelMessageHeader header;
  
  //
  // Channel type identification.
  // This is used for nub matching.
  //
  uuid_t                    type;
  uuid_t                    instance;
  
  UInt64                    reserved1;
  UInt64                    reserved2;
  
  UInt16                    flags;
  UInt16                    mmioSizeMegabytes;
  
  union {
    //
    // Non-pipe mode.
    //
    struct {
      UInt8                 data[kVMBusChannelOfferMaxUserDefinedBytes];
    } standard;
    
    //
    // Pipe mode.
    //
    struct {
      UInt32                mode;
      UInt8                 data[kVMBusChannelOfferMaxPipeUserDefinedBytes];
    } pipe;
  };
  
  UInt16                    channelSubIndex;
  UInt16                    reserved3;
  UInt32                    channelId;
  UInt8                     monitorId;

  //
  // Windows 7 and later.
  //
  UInt8                     monitorAllocated : 1;
  UInt8                     reserved4 : 7;
  UInt16                    dedicatedInterrupt : 1;
  UInt16                    reserved5 : 15;
  UInt32                    connectionId;
} VMBusChannelMessageChannelOffer;

// kVMBusChannelMessageTypeRescindChannelOffer
typedef struct __attribute__((packed)) {
  VMBusChannelMessageHeader header;
  
  UInt32                    channelId;
} VMBusChannelMessageChannelRescindOffer;

// kVMBusChannelMessageTypeChannelOpen
typedef struct __attribute__((packed)) {
  VMBusChannelMessageHeader header;
  
  UInt32                    channelId;
  UInt32                    openId;
  UInt32                    ringBufferGpadlHandle;
  UInt32                    targetCpu;
  
  UInt32                    downstreamRingBufferPageOffset;
  UInt8                     userData[kVMBusChannelOfferMaxUserDefinedBytes];
} VMBusChannelMessageChannelOpen;

// kVMBusChannelMessageTypeChannelOpenResponse
typedef struct __attribute__((packed)) {
  VMBusChannelMessageHeader header;
  
  UInt32                    channelId;
  UInt32                    openId;
  UInt32                    status;
} VMBusChannelMessageChannelOpenResponse;

// kVMBusChannelMessageTypeChannelClose
typedef struct __attribute__((packed)) {
  VMBusChannelMessageHeader header;
  UInt32                    channelId;
} VMBusChannelMessageChannelClose;

#define kHyperVMaxGpadlPages    8192
#define kHyperVGpadlRangeCount  1

// kVMBusChannelMessageTypeGPADLHeader
typedef struct __attribute__((packed)) {
  VMBusChannelMessageHeader header;

  UInt32                    channelId;
  UInt32                    gpadl;
  UInt16                    rangeBufferLength;
  UInt16                    rangeCount;
  HyperVGPARange            range[];
} VMBusChannelMessageGPADLHeader;

// kVMBusChannelMessageTypeGPADLBody
typedef struct __attribute__((packed)) {
  VMBusChannelMessageHeader header;

  UInt32                    messageNumber;
  UInt32                    gpadl;
  UInt64                    pfn[];
} VMBusChannelMessageGPADLBody;

#define kHyperVMaxGpadlBodyPfns   ((kHyperVMessageDataSize - \
  sizeof (VMBusChannelMessageGPADLBody)) / sizeof (UInt64))

// kVMBusChannelMessageTypeGPADLCreated
typedef struct __attribute__((packed)) {
  VMBusChannelMessageHeader header;

  UInt32                    channelId;
  UInt32                    gpadl;
  UInt32                    status;
} VMBusChannelMessageGPADLCreated;

// kVMBusChannelMessageTypeGPADLTeardown
typedef struct __attribute__((packed)) {
  VMBusChannelMessageHeader header;

  UInt32                    channelId;
  UInt32                    gpadl;
} VMBusChannelMessageGPADLTeardown;

// kVMBusChannelMessageTypeGPADLTeardownResponse
typedef struct __attribute__((packed)) {
  VMBusChannelMessageHeader header;
  UInt32                    gpadl;
} VMBusChannelMessageGPADLTeardownResponse;

// kVMBusChannelMessageTypeChannelFree
typedef struct __attribute__((packed)) {
  VMBusChannelMessageHeader header;
  UInt32                    channelId;
} VMBusChannelMessageChannelFree;

// kVMBusChannelMessageTypeConnect
typedef struct __attribute__((packed)) {
  VMBusChannelMessageHeader header;
  UInt32                    protocolVersion;
  UInt32                    targetProcessor;
  
  union {
    UInt64                  interruptPage;
    struct {
      UInt8                 messageInt;
      UInt8                 reserved1[3];
      UInt32                reserved2;
    };
  };
  
  UInt64                    monitorPage1;
  UInt64                    monitorPage2;
} VMBusChannelMessageConnect;

// kVMBusChannelMessageTypeConnectResponse
typedef struct __attribute__((packed)) {
  VMBusChannelMessageHeader header;

  UInt8                     supported;
  UInt8                     connectionState;
  UInt16                    reserved;
  
  UInt32                    messageConnectionId;
} VMBusChannelMessageConnectResponse;

//
// Data structures
//

//
// Ring buffer
//
typedef struct __attribute__((packed)) {
  //
  // Offsets in bytes from start of ring data.
  //
  volatile UInt32   writeIndex;
  volatile UInt32   readIndex;
  //
  // Interrupt mask, values are either 0 or 1.
  //
  // For a transmit ring, host can set this to 1 which indicates we
  // do not need to notify the host.
  //
  // For a receive ring, we can set this to 1 to prevent host
  // from sending us interrupts.
  //
  volatile UInt32   interruptMask;
  volatile UInt32   pendingSendSize;
  UInt32            reserved[12];
  
  union {
    struct {
      UInt32        pendingSendSizeSupported : 1;
    };
    UInt32          value;
  } features;
  //
  // Padding to ensure buffer starts on a page boundary.
  //
  UInt8             reserved2[PAGE_SIZE - 76];
  //
  // Guest to host interrupt count
  //
  // For transmit rings, this counts the guest signaling the host
  // when this ring changes from empty to not empty.
  // For receive rings, this counts the guest signaling the host
  // when this ring changes from full to not full.
  //
  volatile UInt64            guestToHostInterruptCount;
  //
  // Actual ring buffer data.
  //
  UInt8             buffer[];
} VMBusRingBuffer;

//
// Packets.
//
#define kVMBusPacketSizeShift         3
#define kVMBusPacketResponseRequired  1

#define HV_GET_VMBUS_PACKETSIZE(p)    ((p) << kVMBusPacketSizeShift)
#define HV_SET_VMBUS_PACKETSIZE(p)    ((p) >> kVMBusPacketSizeShift)

#define HV_PACKETALIGN(a)         (((a) + (sizeof(UInt64) - 1)) &~ (sizeof(UInt64) - 1))

typedef enum : UInt16 {
  kVMBusPacketTypeInvalid                   = 0,
  kVMBusPacketTypeSynch                     = 1,
  kVMBusPacketTypeAddTransferPageset        = 2,
  kVMBusPacketTypeRemoveTransferPageset     = 3,
  kVMBusPacketTypeEstablishGPADL            = 4,
  kVMBusPacketTypeTeardownGPADL             = 5,
  kVMBusPacketTypeDataInband                = 6,
  kVMBusPacketTypeDataUsingTransferPages    = 7,
  kVMBusPacketTypeDataUsingGPADL            = 8,
  kVMBusPacketTypeDataUsingGPADirect        = 9,
  kVMBusPacketTypeCancelRequest             = 10,
  kVMBusPacketTypeCompletion                = 11,
  kVMBusPacketTypeDataUsingAdditionalPacket = 12,
  kVMBusPacketTypeAdditionalData            = 13
} VMBusPacketType;

typedef struct __attribute__((packed)) {
  //
  // Packet type.
  //
  VMBusPacketType   type;
  //
  // Header length as a multiple of 8 bytes.
  //
  UInt16            headerLength;
  //
  // Total packet length as a multiple of 8 bytes.
  //
  UInt16            totalLength;
  //
  // Packet flags.
  //
  UInt16            flags;
  //
  // Packet transaction ID.
  //
  UInt64            transactionId;
} VMBusPacketHeader;

typedef struct __attribute__((packed)) {
  UInt32 count;
  UInt32 offset;
} VMBusTransferPageRange;

typedef struct __attribute__((packed)) {
  VMBusPacketHeader       header;
  
  UInt16                  transferPagesetId;
  UInt8                   senderOwnsSets;
  UInt8                   reserved;
  UInt32                  rangeCount;
  VMBusTransferPageRange  ranges[1];
} VMBusPacketTransferPages;

#define kVMBusMaxPageBufferCount    32

//
// Single page buffer.
//
typedef struct __attribute__((packed)) {
  UInt32  length;
  UInt32  offset;
  UInt64  pfn;
} VMBusSinglePageBuffer;

typedef struct __attribute__((packed)) {
  VMBusPacketHeader         header;
  
  UInt32                    reserved;
  //
  // Number of single pages.
  //
  UInt32                    rangeCount;
  VMBusSinglePageBuffer     ranges[kVMBusMaxPageBufferCount];
} VMBusPacketSinglePageBuffer;

//
// Multiple page buffer.
//
typedef struct __attribute__((packed)) {
  UInt32  length;
  UInt32  offset;
  UInt64  pfns[];
} VMBusMultiPageBuffer;

typedef struct __attribute__((packed)) {
  VMBusPacketHeader         header;
  
  UInt32                    reserved;
  UInt32                    rangeCount; // Always 1 for this driver.
  VMBusMultiPageBuffer      range;
} VMBusPacketMultiPageBuffer;

#endif
