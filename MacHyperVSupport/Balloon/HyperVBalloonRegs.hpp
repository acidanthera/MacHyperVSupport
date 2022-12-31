//
//  HyperVBalloonRegs.hpp
//  Hyper-V balloon driver constants and structures
//
//  Copyright © 2022-2023 xdqi. All rights reserved.
//

#ifndef HyperVBalloonRegs_hpp
#define HyperVBalloonRegs_hpp

#define kHyperVDynamicMemoryBufferSize                      (16 * 1024)
#define kHyperVDynamicMemoryResponsePacketSize              (2 * PAGE_SIZE)
#define kHyperVDynamicMemoryStatusReportIntervalMilliseconds 1000
#define kHyperVDynamicMemoryReservedPageCount               (512 * 1024 * 1024 / PAGE_SIZE)
#define kHyperVDynamicMemoryBigChunkSize                    (1 * 1024 * 1024)

//
// Current dynamic memory protocol is 3.0 (Windows 10).
// We support older protocol 2.0 (Windows 8) as well, as Gen2 VM may be running on Windows 8 / Server 2012 host
//
typedef enum : UInt32 {
  kHyperVDynamicMemoryProtocolVersion1 = 0x10000,
  kHyperVDynamicMemoryProtocolVersion2 = 0x20000,
  kHyperVDynamicMemoryProtocolVersion3 = 0x30000
} HyperVDynamicMemoryProtocolVersion;

//
// Hyper-V Dynamic Message Protocol Message Types
//
typedef enum : UInt16 {
  kDynamicMemoryMessageTypeError                    = 0,
  kDynamicMemoryMessageTypeProtocolRequest          = 1,
  kDynamicMemoryMessageTypeProtocolResponse         = 2,
  kDynamicMemoryMessageTypeCapabilitiesReport       = 3,
  kDynamicMemoryMessageTypeCapabilitiesResponse     = 4,
  kDynamicMemoryMessageTypeStatusReport             = 5,
  kDynamicMemoryMessageTypeBalloonInflationRequest  = 6,
  kDynamicMemoryMessageTypeBalloonInflationResponse = 7,
  kDynamicMemoryMessageTypeBalloonDeflationRequest  = 8,
  kDynamicMemoryMessageTypeBalloonDeflationResponse = 9,
  kDynamicMemoryMessageTypeHotAddRequest            = 10,
  kDynamicMemoryMessageTypeHotAddResponse           = 11,
  kDynamicMemoryMessageTypeInfoMessage              = 12
} HyperVDynamicMemoryMessageType;

//
// a memory range in guest RAM physical address space
//
typedef struct __attribute__((packed)) {
  UInt64 startPageFrameNumber : 40;
  UInt64 pageCount            : 24;
} HyperVDynamicMemoryPageRange;

//
// Hyper-V Dynamic Message Message Header, included in every message
//
typedef struct __attribute__((packed)) {
  UInt16 type;
  UInt16 size;
  UInt32 transactionId;
} HyperVDynamicMemoryMessageHeader;

//
// Protocol version negotiation
// TX
//
typedef struct __attribute__((packed)) {
  UInt32 version;
  UInt32 isLastAttempt : 1;
  UInt32 reserved      : 31;
} HyperVDynamicMemoryMessageProtocolRequest;

//
// ACK of Protocol version negotiation
// RX
//
typedef struct __attribute__((packed)) {
  UInt64 isAccepted : 1;
  UInt64 reserved   : 63;
} HyperVDynamicMemoryMessageProtocolResponse;

//
// Guest dynamic memory capabilities report
// TX
//
typedef struct __attribute__((packed)) {
  UInt64 supportBalloon  : 1;
  UInt64 supportHotAdd   : 1;
  UInt64 hotAddAlignment : 4;
  UInt64 reserved        : 58;
  UInt64 minimumPageCount;
  UInt64 maximumPageNumber;
} HyperVDynamicMemoryMessageCapabilitiesReport;

//
// ACK of guest dynamic memory capabilities report
// RX
//
typedef struct __attribute__((packed)) {
  UInt64 isAccepted : 1;
  UInt64 reserved   : 63;
} HyperVDynamicMemoryMessageCapabilitiesResponse;

//
// Guest memory status report
// TX, not excepting response
//
typedef struct __attribute__((packed)) {
  UInt64 availablePages;
  UInt64 committedPages;
  UInt64 pageFileSizeInPages;
  UInt64 zeroAndFreePages;
  UInt32 pageFileWritesInPages;
  UInt32 ioDifference;
} HyperVDynamicMemoryMessageStatusReport;

//
// Balloon inflation request
// RX
//
typedef struct __attribute__((packed)) {
  UInt32 pageCount;
  UInt32 reserved;
} HyperVDynamicMemoryMessageBalloonInflationRequest;

//
// Balloon inflation response
// TX
//
typedef struct __attribute__((packed)) {
  UInt32                       reserved;
  UInt32                       morePages  : 1;
  UInt32                       rangeCount : 31;
  HyperVDynamicMemoryPageRange ranges[];
} HyperVDynamicMemoryMessageBalloonInflationResponse;

//
// Balloon deflation request
// RX
//
typedef struct __attribute__((packed)) {
  UInt32                       reserved;
  UInt32                       morePages  : 1;
  UInt32                       rangeCount : 31;
  HyperVDynamicMemoryPageRange ranges[];
} HyperVDynamicMemoryMessageBalloonDeflationRequest;

//
// Balloon deflation response
// TX
//
typedef struct __attribute__((packed)) {
  // empty
} HyperVDynamicMemoryMessageBalloonDeflationResponse;

//
// Memory hot-add request
// RX
//
typedef struct __attribute__((packed)) {
  HyperVDynamicMemoryPageRange ranges[];
} HyperVDynamicMemoryMessageHotAddRequest;

//
// Memory hot-add response
// TX
//
typedef struct __attribute__((packed)) {
  UInt32 pageCount;
  UInt32 result;
} HyperVDynamicMemoryMessageHotAddResponse;

//
// All messages
//
typedef struct __attribute__((packed)) {
  HyperVDynamicMemoryMessageHeader header;
  
  union {
    HyperVDynamicMemoryMessageProtocolRequest          protocolRequest;
    HyperVDynamicMemoryMessageProtocolResponse         protocolResponse;
    HyperVDynamicMemoryMessageCapabilitiesReport       capabilitiesReport;
    HyperVDynamicMemoryMessageCapabilitiesResponse     capabilitiesResponse;
    HyperVDynamicMemoryMessageStatusReport             statusReport;
    HyperVDynamicMemoryMessageBalloonInflationRequest  inflationRequest;
    HyperVDynamicMemoryMessageBalloonInflationResponse inflationResponse;
    HyperVDynamicMemoryMessageBalloonDeflationRequest  deflationRequest;
    HyperVDynamicMemoryMessageBalloonDeflationResponse deflationResponse;
    HyperVDynamicMemoryMessageHotAddRequest            hotAddRequest;
    HyperVDynamicMemoryMessageHotAddResponse           hotAddResponse;
  };
} HyperVDynamicMemoryMessage;

#endif /* HyperVBalloonRegs_hpp */
