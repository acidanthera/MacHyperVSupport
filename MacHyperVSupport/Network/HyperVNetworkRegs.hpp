//
//  HyperVNetworkRegs.cpp
//  Hyper-V network driver
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#ifndef HyperVNetworkRegs_hpp
#define HyperVNetworkRegs_hpp

#define kHyperVNetworkRingBufferSize (128 * PAGE_SIZE)

#define kHyperVNetworkNDISVersion60001    0x00060001
#define kHyperVNetworkNDISVersion6001E    0x0006001E

#define kHyperVNetworkReceiveBufferSize         (1024 * 1024 * 16)
#define kHyperVNetworkReceiveBufferSizeLegacy   (1024 * 1024 * 15)
#define kHyperVNetworkSendBufferSize            (1024 * 1024 * 15)

#define kHyperVNetworkReceivePacketSize         (16 * PAGE_SIZE)

#define MBit 1000000

#define kHyperVNetworkMaximumTransId  0xFFFFFFFF
#define kHyperVNetworkSendTransIdBits 0xFA00000000000000

#define kHyperVNetworkVendor    "Microsoft"
#define kHyperVNetworkModel     "Hyper-V Network Adapter"

//
// Protocol versions.
//
typedef enum : UInt32 {
  kHyperVNetworkProtocolVersion1  = 0x00002,
  kHyperVNetworkProtocolVersion2  = 0x30002,
  kHyperVNetworkProtocolVersion4  = 0x40000,
  kHyperVNetworkProtocolVersion5  = 0x50000,
  kHyperVNetworkProtocolVersion6  = 0x60000,
  kHyperVNetworkProtocolVersion61 = 0x60001
} HyperVNetworkProtocolVersion;

//
// Network message types.
//
typedef enum : UInt32 {
  kHyperVNetworkMessageTypeNone           = 0,
  
  // Initialization.
  kHyperVNetworkMessageTypeInit           = 1,
  kHyperVNetworkMessageTypeInitComplete   = 2,
  
  // Protocol version 1.
  kHyperVNetworkMessageTypeV1SendNDISVersion              = 100,
  kHyperVNetworkMessageTypeV1SendReceiveBuffer,
  kHyperVNetworkMessageTypeV1SendReceiveBufferComplete,
  kHyperVNetworkMessageTypeV1RevokeReceiveBuffer,
  kHyperVNetworkMessageTypeV1SendSendBuffer,
  kHyperVNetworkMessageTypeV1SendSendBufferComplete,
  kHyperVNetworkMessageTypeV1RevokeSendBuffer,
  kHyperVNetworkMessageTypeV1SendRNDISPacket,
  kHyperVNetworkMessageTypeV1SendRNDISPacketComplete
} HyperVNetworkMessageType;

//
// Network status codes.
//
typedef enum : UInt32 {
  kHyperVNetworkMessageStatusNone = 0,
  kHyperVNetworkMessageStatusSuccess,
  kHyperVNetworkMessageStatusFailure,
  kHyperVNetworkMessageStatusProtocolTooNew,
  kHyperVNetworkMessageStatusProtocolTooOld,
  kHyperVNetworkMessageStatusInvalidRNDISPacket,
  kHyperVNetworkMessageStatusBusy,
  kHyperVNetworkMessageStatusProtocolUnsupported,
  kHyperVNetworkMessageStatusMaximum
} HyperVNetworkMessageStatus;

//
// Initialization (all versions)
//

//
// Initialization version message.
// This message is used during intial setup right after opening the VMBus channel.
//
typedef struct __attribute__((packed)) {
  HyperVNetworkProtocolVersion  minProtocolVersion;
  HyperVNetworkProtocolVersion  maxProtocolVersion;
} HyperVNetworkMessageInitVersion;

//
// Initialization complete message.
// This is the response message from Hyper-V used during intial setup right
// after opening the VMBus channel.
//
typedef struct __attribute__((packed)) {
  HyperVNetworkProtocolVersion  negotiatedProtocolVersion;
  UInt32                        maxMdlChainLength;
  HyperVNetworkMessageStatus    status;
} HyperVNetworkMessageInitComplete;

//
// Initialization message.
//
typedef union __attribute__((packed)) {
  HyperVNetworkMessageInitVersion   initVersion;
  HyperVNetworkMessageInitComplete  initComplete;
} HyperVNetworkMessageInit;

//
// Protocol version 1
//

//
// Send NDIS version to Hyper-V.
//
typedef struct __attribute__((packed)) {
  UInt32 major;
  UInt32 minor;
} HyperVNetworkV1MessageSendNDISVersion;

//
// Send receive buffer to Hyper-V using an allocated GPADL.
//
typedef struct __attribute__((packed)) {
  UInt32 gpadlHandle;
  UInt16 id;
} HyperVNetworkV1MessageSendReceiveBuffer;

typedef struct __attribute__((packed)) {
  UInt32 offset;
  UInt32 subAllocSize;
  UInt32 numSubAllocs;
  UInt32 endOffset;
} HyperVNetworkV1MessageReceiveBufferSection;

//
// Completion response message from Hyper-V after sending a receive buffer.
// This message will be sent before Hyper-V begins using the receive buffer.
//
typedef struct __attribute__((packed)) {
  HyperVNetworkMessageStatus  status;
  UInt32                      numSections;

  //
  // A receive buffer is split into two portions, a large section and a small section.
  // These are then suballocated by a certain size.
  //
  // For example, the following break up of the receive buffer has 6
  // large suballocations and 10 small suballocations.
  //
  //
  //
  // |            Large Section          |  |   Small Section   |
  // ------------------------------------------------------------
  // |     |     |     |     |     |     |  | | | | | | | | | | |
  // |                                      |
  // LargeOffset                            SmallOffset
  HyperVNetworkV1MessageReceiveBufferSection sections[1];
} HyperVNetworkV1MessageSendReceiveBufferComplete;

//
// Message sent to Hyper-V revoking the receive buffer.
//
typedef struct {
  UInt16 id;
} HyperVNetworkV1MessageRevokeReceiveBuffer;

//
// Send send buffer to Hyper-V using an allocated GPADL.
//
typedef struct __attribute__((packed)) {
  UInt32 gpadlHandle;
  UInt16 id;
} HyperVNetworkV1MessageSendSendBuffer;

//
// Completion response message from Hyper-V after sending a send buffer.
// This message will be sent before Hyper-V begins using the send buffer.
//
typedef struct __attribute__((packed)) {
  HyperVNetworkMessageStatus  status;
  
  // VM can select the size of the send buffer.
  UInt32                      sectionSize;
} HyperVNetworkV1MessageSendSendBufferComplete;

//
// Message sent to Hyper-V revoking the send buffer.
//
typedef struct {
  UInt16 id;
} HyperVNetworkV1MessageRevokeSendBuffer;

typedef enum : UInt32 {
  kHyperVNetworkRNDISChannelTypeData      = 0,
  kHyperVNetworkRNDISChannelTypeControl   = 1
} HyperVNetworkRNDISChannelType;

#define kHyperVNetworkRNDISSendSectionIndexInvalid      (-1)

//
// Message used to send an RNDIS packet to the other end of the channel.
// This message is used by both Hyper-V and the VM.
//
typedef struct __attribute__((packed)) {
  //
  // Specified by RNDIS. RNDIS uses 0 for DATA, and 1 for CONTROL.
  // Ethernet packets are typically data, all other non-Ethernet packets are control.
  //
  HyperVNetworkRNDISChannelType channelType;

  //
  // Used to specify what data is to be sent from the send buffer.
  // 0xFFFFFFFF means the send buffer is not being used and data was sent via another method.
  //
  UInt32 sendBufferSectionIndex;
  UInt32 sendBufferSectionSize;
} HyperVNetworkV1MessageSendRNDISPacket;

//
// Response message used after an RNDIS packet is received at the other end of the channel.
// This message is used by both Hyper-V and the VM.
//
typedef struct __attribute__((packed)) {
  HyperVNetworkMessageStatus  status;
} HyperVNetworkV1MessageSendRNDISPacketComplete;

//
// Protocol version 1 messages.
//
typedef union __attribute__((packed)) {
  HyperVNetworkV1MessageSendNDISVersion             sendNDISVersion;
  HyperVNetworkV1MessageSendReceiveBuffer           sendReceiveBuffer;
  HyperVNetworkV1MessageSendReceiveBufferComplete   sendReceiveBufferComplete;
  HyperVNetworkV1MessageRevokeReceiveBuffer         revokeReceiveBuffer;
  HyperVNetworkV1MessageSendSendBuffer              sendSendBuffer;
  HyperVNetworkV1MessageSendSendBufferComplete      sendSendBufferComplete;
  HyperVNetworkV1MessageSendRNDISPacket             sendRNDISPacket;
  HyperVNetworkV1MessageSendRNDISPacketComplete     sendRNDISPacketComplete;
} HyperVNetworkV1Message;

//
// Main message structure.
//
typedef struct __attribute__((packed)) {
  HyperVNetworkMessageType messageType;
  union {
    HyperVNetworkMessageInit    init;
    HyperVNetworkV1Message      v1;
  } __attribute__((packed));
  UInt8 padd[sizeof (HyperVNetworkMessageInit)]; // TODO: required for now for some reason, otherwise Hyper-V rejects message
} HyperVNetworkMessage;

//
// RNDIS messages.
//

#define kHyperVNetworkReceiveBufferID           0xCAFE
#define kHyperVNetworkSendBufferID              0x0000

#define kHyperVNetworkRNDISVersionMajor         0x0001
#define kHyperVNetworkRNDISVersionMinor         0x0000
#define kHyperVNetworkRNDISMaxTransferSize      0x4000

#define kHyperVNetworkRNDISMessageTypeCompletion  0x80000000

typedef enum : UInt32 {
  kHyperVNetworkRNDISStatusSuccess              = 0x0,
  kHyperVNetworkRNDISStatusMediaConnect         = 0x4001000B,
  kHyperVNetworkRNDISStatusMediaDisconnect      = 0x4001000C,
  kHyperVNetworkRNDISStatusLinkSpeedChange      = 0x40010013,
  kHyperVNetworkRNDISStatusNetworkChange        = 0x40010018
} HyperVNetworkRNDISStatus;

typedef enum : UInt32 {
  kHyperVNetworkRNDISMessageTypePacket            = 0x1,
  kHyperVNetworkRNDISMessageTypeInit              = 0x2,
  kHyperVNetworkRNDISMessageTypeInitComplete      = (kHyperVNetworkRNDISMessageTypeInit | kHyperVNetworkRNDISMessageTypeCompletion),
  kHyperVNetworkRNDISMessageTypeHalt              = 0x3,
  kHyperVNetworkRNDISMessageTypeGetOID            = 0x4,
  kHyperVNetworkRNDISMessageTypeGetOIDComplete    = (kHyperVNetworkRNDISMessageTypeGetOID | kHyperVNetworkRNDISMessageTypeCompletion),
  kHyperVNetworkRNDISMessageTypeSetOID            = 0x5,
  kHyperVNetworkRNDISMessageTypeSetOIDComplete    = (kHyperVNetworkRNDISMessageTypeSetOID | kHyperVNetworkRNDISMessageTypeCompletion),
  kHyperVNetworkRNDISMessageTypeReset             = 0x6,
  kHyperVNetworkRNDISMessageTypeResetComplete     = (kHyperVNetworkRNDISMessageTypeReset | kHyperVNetworkRNDISMessageTypeCompletion),
  kHyperVNetworkRNDISMessageTypeIndicate          = 0x7,
  kHyperVNetworkRNDISMessageTypeKeepalive         = 0x8,
  kHyperVNetworkRNDISMessageTypeKeepaliveComplete = (kHyperVNetworkRNDISMessageTypeKeepalive | kHyperVNetworkRNDISMessageTypeCompletion)
} HyperVNetworkRNDISMessageType;

//
// RNDIS message header.
//
typedef struct {
  HyperVNetworkRNDISMessageType type;
  UInt32                        length;
} HyperVNetworkRNDISMessageHeader;

//
// Data packet message.
// This message is used for Ethernet frames. Offsets are from
// the beginning of the packet header.
//
typedef struct {
  UInt32 dataOffset;
  UInt32 dataLength;
  UInt32 oobDataOffset;
  UInt32 oobDataLength;
  UInt32 oobNumDataElements;
  UInt32 perPacketInfoOffset;
  UInt32 perPacketInfoLength;
  UInt32 vcHandle;
  UInt32 reserved;
} HyperVNetworkRNDISMessageDataPacket;

//
// Initialization message.
//
typedef struct {
  UInt32 requestId;
  UInt32 majorVersion;
  UInt32 minorVersion;
  UInt32 maxTransferSize;
} HyperVNetworkRNDISMessageInitializeRequest;

//
// Initialization complete message.
//
typedef struct {
  UInt32                    requestId;
  HyperVNetworkRNDISStatus  status;
  UInt32                    majorVersion;
  UInt32                    minorVersion;
  UInt32                    devFlags;
  UInt32                    medium;
  UInt32                    maxPacketsPerMessage;
  UInt32                    maxTransferSize;
  UInt32                    packetAlignmentFactor;
  UInt32                    afListOffset;
  UInt32                    afListSize;
} HyperVNetworkRNDISMessageInitializeComplete;

//
// OID definitions.
//
typedef enum : UInt32 {
  // Required general OIDs.
  kHyperVNetworkRNDISOIDGeneralSupportedList                = 0x10101,
  kHyperVNetworkRNDISOIDGeneralHardwareStatus               = 0x10102,
  kHyperVNetworkRNDISOIDGeneralMediaSupported               = 0x10103,
  kHyperVNetworkRNDISOIDGeneralMediaInUse                   = 0x10104,
  kHyperVNetworkRNDISOIDGeneralMaximumLookahead             = 0x10105,
  kHyperVNetworkRNDISOIDGeneralMaximumFrameSize             = 0x10106,
  kHyperVNetworkRNDISOIDGeneralLinkSpeed                    = 0x10107,
  kHyperVNetworkRNDISOIDGeneralTransmitBufferSpace          = 0x10108,
  kHyperVNetworkRNDISOIDGeneralReceiveBufferSpace           = 0x10109,
  kHyperVNetworkRNDISOIDGeneralTransmitBlockSize            = 0x1010A,
  kHyperVNetworkRNDISOIDGeneralReceiveBlockSize             = 0x1010B,
  kHyperVNetworkRNDISOIDGeneralVendorId                     = 0x1010C,
  kHyperVNetworkRNDISOIDGeneralVendorDescription            = 0x1010D,
  kHyperVNetworkRNDISOIDGeneralCurrentPacketFilter          = 0x1010E,
  kHyperVNetworkRNDISOIDGeneralCurrentLookahead             = 0x1010F,
  kHyperVNetworkRNDISOIDGeneralDriverVersion                = 0x10110,
  kHyperVNetworkRNDISOIDGeneralMaximumTotalSize             = 0x10111,
  kHyperVNetworkRNDISOIDGeneralProtocolOptions              = 0x10112,
  kHyperVNetworkRNDISOIDGeneralMACOptions                   = 0x10113,
  kHyperVNetworkRNDISOIDGeneralMediaConnectStatus           = 0x10114,
  kHyperVNetworkRNDISOIDGeneralMaximumSendPackets           = 0x10115,
  kHyperVNetworkRNDISOIDGeneralVendorDriverVersion          = 0x10116,
  kHyperVNetworkRNDISOIDGeneralSupportedGUIDs               = 0x10117,
  kHyperVNetworkRNDISOIDGeneralNetworkLayerAddresses        = 0x10118,
  kHyperVNetworkRNDISOIDGeneralTransportHeaderOffset        = 0x10119,
  kHyperVNetworkRNDISOIDGeneralPhysicalMedium               = 0x10202,
  kHyperVNetworkRNDISOIDGeneralMachineName                  = 0x1021A,
  kHyperVNetworkRNDISOIDGeneralRNDISConfigParameter         = 0x1021B,
  kHyperVNetworkRNDISOIDGeneralVLANId                       = 0x1021C,
  
  // Optional general OIDs.
  kHyperVNetworkRNDISOIDGeneralMediaCapabilities            = 0x10201,
  
  // Required statistics OIDs.
  kHyperVNetworkRNDISOIDGeneralTransmitOk                   = 0x20101,
  kHyperVNetworkRNDISOIDGeneralReceiveOk                    = 0x20102,
  kHyperVNetworkRNDISOIDGeneralTransmitError                = 0x20103,
  kHyperVNetworkRNDISOIDGeneralReceiveError                 = 0x20104,
  kHyperVNetworkRNDISOIDGeneralReceiveNoBuffer              = 0x20105,
  
  // 802.3 Ethernet OIDs.
  kHyperVNetworkRNDISOIDEthernetPermanentAddress            = 0x1010101,
  kHyperVNetworkRNDISOIDEthernetCurrentAddress              = 0x1010102,
  kHyperVNetworkRNDISOIDEthernetMulticastList               = 0x1010103,
  kHyperVNetworkRNDISOIDEthernetMaximumListSize             = 0x1010104,
  kHyperVNetworkRNDISOIDEthernetMACOptions                  = 0x1010105,
  kHyperVNetworkRNDISOIDEthernetReceiveErrorAlignment       = 0x1020101,
  kHyperVNetworkRNDISOIDEthernetTransmitOneCollision        = 0x1020102,
  kHyperVNetworkRNDISOIDEthernetTransmitMoreCollisions      = 0x1020103,
  kHyperVNetworkRNDISOIDEthernetTransmitDeferred            = 0x1020201,
  kHyperVNetworkRNDISOIDEthernetTransmitMaxCollisions       = 0x1020202,
  kHyperVNetworkRNDISOIDEthernetReceiveOverrun              = 0x1020203,
  kHyperVNetworkRNDISOIDEthernetTransmitUnderrun            = 0x1020204,
  kHyperVNetworkRNDISOIDEthernetTransmitHeartbeatFailure    = 0x1020205,
  kHyperVNetworkRNDISOIDEthernetTransmitTimesCRSLost        = 0x1020206,
  kHyperVNetworkRNDISOIDEthernetTransmitLateCollision       = 0x1020207
} HyperVNetworkRNDISOID;

typedef enum : UInt32 {
  kHyperVNetworkRNDISLinkStateConnected,
  kHyperVNetworkRNDISLinkStateDisconnted
} HyperVNetworkRNDISLinkState;

//
// Packet filter bits.
//
#define kHyperVNetworkPacketFilterDirected        BIT(0)
#define kHyperVNetworkPacketFilterMulticast       BIT(1)
#define kHyperVNetworkPacketFilterAllMulticast    BIT(2)
#define kHyperVNetworkPacketFilterBroadcast       BIT(3)
#define kHyperVNetworkPacketFilterSourceRouting   BIT(4)
#define kHyperVNetworkPacketFilterPromiscuous     BIT(5)
#define kHyperVNetworkPacketFilterSMT             BIT(6)
#define kHyperVNetworkPacketFilterAllLocal        BIT(7)
#define kHyperVNetworkPacketFilterGroup           BIT(8)
#define kHyperVNetworkPacketFilterAllFunctional   BIT(9)
#define kHyperVNetworkPacketFilterFunctional      BIT(10)
#define kHyperVNetworkPacketFilterMACFrame        BIT(11)

//
// Get OID request message.
//
typedef struct {
  UInt32                requestId;
  HyperVNetworkRNDISOID oid;
  UInt32                infoBufferLength;
  UInt32                infoBufferOffset;
  UInt32                deviceVcHandle;
} HyperVNetworkRNDISMessageGetOIDRequest;

//
// Get OID complete message.
//
typedef struct {
  UInt32                   requestId;
  HyperVNetworkRNDISStatus status;
  UInt32                   infoBufferLength;
  UInt32                   infoBufferOffset;
} HyperVNetworkRNDISMessageGetOIDComplete;

//
// Set OID request message.
//
typedef struct {
  UInt32                requestId;
  HyperVNetworkRNDISOID oid;
  UInt32                infoBufferLength;
  UInt32                infoBufferOffset;
  UInt32                deviceVcHandle;
} HyperVNetworkRNDISMessageSetOIDRequest;

//
// Set OID complete message.
//
typedef struct {
  UInt32                   requestId;
  HyperVNetworkRNDISStatus status;
} HyperVNetworkRNDISMessageSetOIDComplete;

//
// Reset request message.
//
typedef struct {
  UInt32 reserved;
} HyperVNetworkRNDISMessageResetRequest;

//
// Reset request message.
//
typedef struct {
  HyperVNetworkRNDISStatus  status;
  UInt32                    addressingReset;
} HyperVNetworkRNDISMessageResetComplete;

//
// Indicate status message.
//
typedef struct {
  HyperVNetworkRNDISStatus  status;
  UInt32                    statusBufferLength;
  UInt32                    statusBufferOffset;
} HyperVNetworkRNDISMessageIndicateStatus;

//
// Keep alive request message.
//
typedef struct {
  UInt32 requestId;
} HyperVNetworkRNDISMessageKeepaliveRequest;

//
// Keep alive complete message.
//
typedef struct {
  UInt32                    requestId;
  HyperVNetworkRNDISStatus  status;
} HyperVNetworkRNDISMessageKeepaliveComplete;

//
// Main message structure.
//
typedef struct {
  HyperVNetworkRNDISMessageHeader header;
  union {
    HyperVNetworkRNDISMessageDataPacket           dataPacket;
    HyperVNetworkRNDISMessageInitializeRequest    initRequest;
    HyperVNetworkRNDISMessageInitializeComplete   initComplete;
    HyperVNetworkRNDISMessageGetOIDRequest        getOIDRequest;
    HyperVNetworkRNDISMessageGetOIDComplete       getOIDComplete;
    HyperVNetworkRNDISMessageSetOIDRequest        setOIDRequest;
    HyperVNetworkRNDISMessageSetOIDComplete       setOIDComplete;
    HyperVNetworkRNDISMessageResetRequest         resetRequest;
    HyperVNetworkRNDISMessageResetComplete        resetComplete;
    HyperVNetworkRNDISMessageIndicateStatus       indicateStatus;
    HyperVNetworkRNDISMessageKeepaliveRequest     keepaliveRequest;
    HyperVNetworkRNDISMessageKeepaliveComplete    keepaliveComplete;
  };
} HyperVNetworkRNDISMessage;

#endif
