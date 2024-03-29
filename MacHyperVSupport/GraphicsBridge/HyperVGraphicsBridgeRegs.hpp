//
//  HyperVGraphicsBridgeRegs.hpp
//  Hyper-V synthetic graphics bridge
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVGraphicsBridgeRegs_hpp
#define HyperVGraphicsBridgeRegs_hpp

#define kHyperVPCIVendorMicrosoft   0x1414
#define kHyperVPCIDeviceHyperVVideo 0x5353

#define kHyperVGraphicsDefaultWidth   1152
#define kHyperVGraphicsDefaultHeight  864
#define kHyperVGraphicsMinWidth       640
#define kHyperVGraphicsMinHeight      480

#define kHyperVGraphicsRingBufferSize (64 * PAGE_SIZE)
#define kHyperVGraphicsMaxPacketSize  (4 * PAGE_SIZE)

//
// Graphics versions.
//
#define kHyperVGraphicsVersionV3_0  { 3, 0 } // Windows Server 2008 R2 and older
#define kHyperVGraphicsVersionV3_2  { 3, 2 } // Windows 8 / Windows Server 2012 and newer

//
// Fixed transaction IDs for request/response due to transaction IDs
// not being respected by graphics system.
//
#define kHyperVGraphicsVersionRequestTransactionID 0xCAFECAFE

//
// Graphics messages.
//
typedef enum : UInt32 {
  kHyperVGraphicsPipeMessageTypeInvalid = 0x0,
  kHyperVGraphicsPipeMessageTypeData    = 0x1
} HyperVGraphicsPipeMessageType;

typedef struct __attribute__((packed)) {
  HyperVGraphicsPipeMessageType type;
  UInt32                        size;
} HyperVGraphicsPipeMessageHeader;

typedef enum : UInt32 {
  kHyperVGraphicsMessageTypeError               = 0x0,
  kHyperVGraphicsMessageTypeVersionRequest      = 0x1,
  kHyperVGraphicsMessageTypeVersionResponse     = 0x2,
  kHyperVGraphicsMessageTypeVRAMLocation        = 0x3,
  kHyperVGraphicsMessageTypeVRAMAck             = 0x4,
  kHyperVGraphicsMessageTypeSituationUpdate     = 0x5,
  kHyperVGraphicsMessageTypeSituationUpdateAck  = 0x6,
  kHyperVGraphicsMessageTypePointerPosition     = 0x7,
  kHyperVGraphicsMessageTypePointerShape        = 0x8,
  kHyperVGraphicsMessageTypeFeatureChange       = 0x9,
  kHyperVGraphicsMessageTypeDIRT                = 0xA
} HyperVGraphicsMessageType;

//
// Message header.
// Size includes this header and payload.
//
typedef struct __attribute__((packed)) {
  HyperVGraphicsMessageType type;
  UInt32                    size;
} HyperVGraphicsMessageHeader;

typedef struct __attribute__((packed)) {
  VMBusVersion version;
} HyperVGraphicsMessageVersionRequest;

typedef struct __attribute__((packed)) {
  VMBusVersion version;
  UInt8        accepted;
  UInt8        maxVideoOutputs;
} HyperVGraphicsMessageVersionResponse;

typedef struct __attribute__((packed)) {
  UInt64 context;
  UInt8  isVRAMGPASpecified;
  UInt64 vramGPA;
} HyperVGraphicsMessageVRAMLocation;

typedef struct __attribute__((packed)) {
  UInt64 context;
} HyperVGraphicsMessageVRAMAck;

typedef struct __attribute__((packed)) {
  UInt8  active;
  UInt32 vramOffset;
  UInt8  depth;
  UInt32 width;
  UInt32 height;
  UInt32 pitch;
} HyperVGraphicsVideoOutputSituation;

typedef struct __attribute__((packed)) {
  UInt64                             context;
  UInt8                              videoOutputCount;
  HyperVGraphicsVideoOutputSituation videoOutputs[];
} HyperVGraphicsMessageSituationUpdate;

typedef struct __attribute__((packed)) {
  UInt64 context;
} HyperVGraphicsMessageSituationAck;

typedef struct __attribute__((packed)) {
  UInt8  isVisible;
  UInt8  videoOutput;
  SInt32 x;
  SInt32 y;
} HyperVGraphicsMessagePointerPosition;

#define kHyperVGraphicsPointerMaxWidth      96
#define kHyperVGraphicsPointerMaxHeight     96
#define kHyperVGraphicsPointerARGBPixelSize 4
#define kHyperVGraphicsPointerMaxSize       (kHyperVGraphicsPointerMaxWidth * kHyperVGraphicsPointerMaxHeight * kHyperVGraphicsPointerARGBPixelSize)
#define kHyperVGraphicsPointerComplete      (-1)

typedef struct __attribute__((packed)) {
  UInt8  partIndex;
  UInt8  isARGB;
  UInt32 width;
  UInt32 height;
  UInt32 hotX;
  UInt32 hotY;
  UInt8  data[4];
} HyperVGraphicsMessagePointerShape;

typedef struct __attribute__((packed)) {
  UInt8 isDIRTNeeded;
  UInt8 isPointerPositionNeeded;
  UInt8 isPointerShapeNeeded;
  UInt8 isSituationNeeded;
} HyperVGraphicsMessageFeatureUpdate;

typedef struct __attribute__((packed)) {
  SInt32 x1, y1; // Top left corner.
  SInt32 x2, y2; // Bottom right corner.
} HyperVGraphicsDIRTRectangle;

typedef struct __attribute__((packed)) {
  UInt8                       videoOutput;
  UInt8                       dirtCount;
  HyperVGraphicsDIRTRectangle dirtRects[];
} HyperVGraphicsMessageDIRT;

typedef struct __attribute__((packed)) {
  HyperVGraphicsPipeMessageHeader pipeHeader;
  HyperVGraphicsMessageHeader     gfxHeader;

  union {
    HyperVGraphicsMessageVersionRequest  versionRequest;
    HyperVGraphicsMessageVersionResponse versionResponse;
    HyperVGraphicsMessageVRAMLocation    vramLocation;
    HyperVGraphicsMessageVRAMAck         vramAck;
    HyperVGraphicsMessageSituationUpdate situationUpdate;
    HyperVGraphicsMessageSituationAck    situationAck;
    HyperVGraphicsMessagePointerPosition pointerPosition;
    HyperVGraphicsMessagePointerShape    pointerShape;
    HyperVGraphicsMessageFeatureUpdate   featureUpdate;
    HyperVGraphicsMessageDIRT            dirt;
  };
} HyperVGraphicsMessage;

#endif
