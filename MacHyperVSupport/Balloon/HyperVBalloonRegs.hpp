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

//
// Current dynamic memory protocol is 3.0 (Windows 10).
// TODO: Support older protocol 2.0 (Windows 8) as well.
//
typedef enum : UInt32 {
  kHyperVDynamicMemoryProtocolVersion1 = 0x10000,
  kHyperVDynamicMemoryProtocolVersion2 = 0x20000,
  kHyperVDynamicMemoryProtocolVersion3 = 0x30000
} HyperVDynamicMemoryProtocolVersion;

typedef enum : UInt16 {
  kDynamicMemoryMessageTypeError                    = 0,
  kDynamicMemoryMessageTypeVersionRequest           = 1,
  kDynamicMemoryMessageTypeVersionResponse          = 2,
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

typedef struct __attribute__((packed)) {
  UInt64 supportBalloon  : 1;
  UInt64 supportHotAdd   : 1;
  UInt64 hotAddAlignment : 4;
  UInt64 reserved        : 58;
} HyperVDynamicMemoryCapabilities; 

typedef struct __attribute__((packed)) {
  UInt64 startPageFrameNumber : 40;
  UInt64 pageCount            : 24;
} HyperVDynamicMemoryPageRange;

typedef struct __attribute__((packed)) {
  UInt32 version;
  UInt32 isLastAttempt : 1;
  UInt32 reserved      : 31;
} HyperVDynamicMemoryMessageVersionRequest;

typedef struct __attribute__((packed)) {
  UInt64 isAccepted : 1;
  UInt64 reserved   : 63;
} HyperVDynamicMemoryMessageVersionResponse;

typedef struct __attribute__((packed)) {
  HyperVDynamicMemoryCapabilities capabilities;
  UInt64                          minimumPageCount;
  UInt64                          maximumPageNumber;
} HyperVDynamicMemoryMessageCapabilitiesReport;

typedef struct __attribute__((packed)) {
  UInt64 isAccepted : 1;
  UInt64 reserved   : 63;
} HyperVDynamicMemoryMessageCapabilitiesResponse;

typedef struct __attribute__((packed)) {
  UInt64 availablePages;
  UInt64 committedPages;
  UInt64 pageFileSizeInPages;
  UInt64 zeroAndFreePages;
  UInt32 pageFileWritesInPages;
  UInt32 ioDifference;
} HyperVDynamicMemoryMessageStatusReport;

typedef struct __attribute__((packed)) {
  UInt32 pageCount;
  UInt32 reserved;
} HyperVDynamicMemoryMessageBalloonInflationRequest;

typedef struct __attribute__((packed)) {
  UInt32                       reserved;
  UInt32                       morePages  : 1;
  UInt32                       rangeCount : 31;
  HyperVDynamicMemoryPageRange ranges[];
} HyperVDynamicMemoryMessageBalloonInflationResponse;

typedef struct __attribute__((packed)) {
  UInt32                       reserved;
  UInt32                       morePages  : 1;
  UInt32                       rangeCount : 31;
  HyperVDynamicMemoryPageRange ranges[];
} HyperVDynamicMemoryMessageBalloonDeflationRequest;

typedef struct __attribute__((packed)) {
  // empty
} HyperVDynamicMemoryMessageBalloonDeflationResponse;

typedef struct __attribute__((packed)) {
  HyperVMemoryPageRange ranges[];
} HyperVDynamicMemoryMessageHotAddRequest;

typedef struct __attribute__((packed)) {
  UInt32 pageCount;
  UInt32 result;
} HyperVDynamicMemoryMessageHotAddResponse;

typedef struct __attribute__((packed)) {
  UInt16 type;
  UInt16 size;
  UInt32 transactionId;
  
  union {
    HyperVDynamicMemoryMessageVersionRequest           versionRequest;
    HyperVDynamicMemoryMessageVersionResponse          versionResponse;
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
