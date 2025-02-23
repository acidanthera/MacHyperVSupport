//
//  HyperVGraphicsRegs.hpp
//  Hyper-V synthetic graphics driver
//
//  Copyright © 2022-2025 Goldfish64. All rights reserved.
//

#ifndef HyperVGraphicsRegs_hpp
#define HyperVGraphicsRegs_hpp

#include "VMBus.hpp"

#define kHyperVPCIVendorMicrosoft   0x1414
#define kHyperVPCIDeviceHyperVVideo 0x5353

#define kHyperVGraphicsDefaultWidth   1152
#define kHyperVGraphicsDefaultHeight  864

//
// Size limits.
//
#define kHyperVGraphicsMinWidth           640
#define kHyperVGraphicsMinHeight          480
#define kHyperVGraphicsMaxWidth2008       1600
#define kHyperVGraphicsMaxHeight2008      1200
#define kHyperVGraphicsBitDepth2008       16
#define kHyperVGraphicsBitDepth           32
#define kHyperVGraphicsBitsPerByte        8

#define kHyperVGraphicsCursorMaxWidth     32
#define kHyperVGraphicsCursorMaxHeight    32

#define kHyperVGraphicsRingBufferSize (64 * PAGE_SIZE)
#define kHyperVGraphicsMaxPacketSize  (4 * PAGE_SIZE)

//
// Graphics versions.
//
#define kHyperVGraphicsVersionV3_0  VMBUS_VERSION(3, 0) // Windows Server 2008 and Windows Server 2008 R2
#define kHyperVGraphicsVersionV3_2  VMBUS_VERSION(3, 2) // Windows 8 / Windows Server 2012, Windows 8.1 / Windows Server 2012 R2, and Windows 10 / Windows Server 2016
#define kHyperVGraphicsVersionV3_5  VMBUS_VERSION(3, 5) // Windows 10 v1809 / Windows Server 2019 and newer

//
// Fixed transaction IDs for request/response due to transaction IDs
// not being respected by graphics system.
//
#define kHyperVGraphicsRequestTransactionBaseID 0x46495348

#define kHyperVGraphicsImageUpdateRefreshRateMS      10

//
// Graphics messages.
//
typedef enum : UInt32 {
  kHyperVGraphicsPipeMessageTypeInvalid = 0x0,
  kHyperVGraphicsPipeMessageTypeData    = 0x1
} HyperVGraphicsPipeMessageType;

//
// Message pipe header.
//
typedef struct __attribute__((packed)) {
  // Message type.
  HyperVGraphicsPipeMessageType type;
  // Size of message body.
  UInt32                        size;
} HyperVGraphicsPipeMessageHeader;

//
// Graphics message types.
//
typedef enum : UInt32 {
  kHyperVGraphicsMessageTypeError               = 0x0,
  kHyperVGraphicsMessageTypeVersionRequest      = 0x1,
  kHyperVGraphicsMessageTypeVersionResponse     = 0x2,
  kHyperVGraphicsMessageTypeVRAMLocation        = 0x3,
  kHyperVGraphicsMessageTypeVRAMAck             = 0x4,
  kHyperVGraphicsMessageTypeResolutionUpdate    = 0x5,
  kHyperVGraphicsMessageTypeResolutionUpdateAck = 0x6,
  kHyperVGraphicsMessageTypeCursorPosition      = 0x7,
  kHyperVGraphicsMessageTypeCursorShape         = 0x8,
  kHyperVGraphicsMessageTypeFeatureChange       = 0x9,
  kHyperVGraphicsMessageTypeImageUpdate         = 0xA
} HyperVGraphicsMessageType;

//
// Message header.
//
typedef struct __attribute__((packed)) {
  // Message type.
  HyperVGraphicsMessageType type;
  // Size of this header and entire message body.
  UInt32                    size;
} HyperVGraphicsMessageHeader;

//
// Version request message.
// Indicates the client version to the Hyper-V host.
//
typedef struct __attribute__((packed)) {
  // Version of client.
  VMBusVersion version;
} HyperVGraphicsMessageVersionRequest;

//
// Version request message response.
// Contains the server version from the Hyper-V host.
//
typedef struct __attribute__((packed)) {
  // Version of host.
  VMBusVersion version;
  // Client version was accepted by host.
  UInt8        accepted;
  // Maximum number of video outputs supported (seems to be always one).
  UInt8        maxVideoOutputs;
} HyperVGraphicsMessageVersionResponse;

//
// VRAM location message.
// Specifies to Hyper-V where the graphics memory is located.
//
typedef struct __attribute__((packed)) {
  // Driver-specified context.
  UInt64 context;
  // Always 1, indicates a memory location is specified.
  UInt8  isVRAMGPASpecified;
  // Physical memory location to use as graphics memory.
  UInt64 vramGPA;
} HyperVGraphicsMessageVRAMLocation;

//
// VRAM location message acknowledgement.
// Response from Hyper-V after graphics memory message is processed.
//
typedef struct __attribute__((packed)) {
  // Driver-specified context.
  UInt64 context;
} HyperVGraphicsMessageVRAMAck;

//
// Resolution change information for an video output.
// Specifies new resolution to Hyper-V.
//
typedef struct __attribute__((packed)) {
  // Video output is active?
  UInt8  active;
  // Offset into VRAM?
  UInt32 vramOffset;
  // Screen depth in bits.
  UInt8  depth;
  // Screen width in pixels.
  UInt32 width;
  // Screen height in pixels.
  UInt32 height;
  // Screen pitch in bytes.
  UInt32 pitch;
} HyperVGraphicsVideoOutputResolution;

//
// Resolution change message.
// Specifies new resolution to Hyper-V.
//
typedef struct __attribute__((packed)) {
  // Driver-specified context.
  UInt64                              context;
  // Number of outputs (always one for this driver).
  UInt8                               videoOutputCount;
  // Array of outputs (always contains one output for this driver).
  HyperVGraphicsVideoOutputResolution videoOutputs[1];
} HyperVGraphicsMessageResolutionUpdate;

//
// Resolution change message acknowledgement.
// Response from Hyper-V after new resolution change is processed.
//
typedef struct __attribute__((packed)) {
  // Driver-specified context.
  UInt64 context;
} HyperVGraphicsMessageResolutionUpdateAck;

//
// Cursor position change message.
// Specifies new cursor position or visibility to Hyper-V.
//
typedef struct __attribute__((packed)) {
  // 1 = cursor is visible.
  UInt8  isVisible;
  // Video output index (always zero for this driver).
  UInt8  videoOutput;
  // Cursor X coordinate.
  SInt32 x;
  // Cursor Y coordinate.
  SInt32 y;
} HyperVGraphicsMessageCursorPosition;

#define kHyperVGraphicsCursorARGBPixelSize      4
#define kHyperVGraphicsCursorMaxSize            (kHyperVGraphicsCursorMaxWidth * kHyperVGraphicsCursorMaxHeight * kHyperVGraphicsCursorARGBPixelSize)
#define kHyperVGraphicsCursorPartIndexComplete  (-1)

//
// Cursor shape change message.
// Specifies new cursor shape image to Hyper-V.
//
typedef struct __attribute__((packed)) {
  // Unknown
  UInt8  partIndex;
  // 1 = ARGB format.
  UInt8  isARGB;
  // Width of cursor in pixels.
  UInt32 width;
  // Height of cursor in pixels.
  UInt32 height;
  // Hotpoint in pixels for X coordinate.
  UInt32 hotX;
  // Hotpoint in pixels for Y coordinate.
  UInt32 hotY;
  // Cursor bitmap data.
  UInt8  data[];
} HyperVGraphicsMessageCursorShape;

//
// Feature request message from Hyper-V.
//
typedef struct __attribute__((packed)) {
  // Screen data needs to be updated.
  UInt8 isImageUpdateNeeded;
  // Cursor position needs to be reported.
  UInt8 isCursorPositionNeeded;
  // Cursor shape needs to be reported.
  UInt8 isCursorShapeNeeded;
  // Screen resolution needs to be reported.
  UInt8 isResolutionUpdateNeeded;
} HyperVGraphicsMessageFeatureUpdate;

typedef struct __attribute__((packed)) {
  SInt32 x1, y1; // Top left corner.
  SInt32 x2, y2; // Bottom right corner.
} HyperVGraphicsImageUpdateRectangle;

//
// Image update of screen to Hyper-V.
//
typedef struct __attribute__((packed)) {
  UInt8                               videoOutput;
  UInt8                               count;
  HyperVGraphicsImageUpdateRectangle  rects[1];
} HyperVGraphicsMessageImageUpdate;

//
// Graphics message union.
// Encapsulates all graphics messages to/from Hyper-V.
//
typedef struct __attribute__((packed)) {
  HyperVGraphicsPipeMessageHeader pipeHeader;
  HyperVGraphicsMessageHeader     gfxHeader;

  union {
    HyperVGraphicsMessageVersionRequest       versionRequest;
    HyperVGraphicsMessageVersionResponse      versionResponse;
    HyperVGraphicsMessageVRAMLocation         vramLocation;
    HyperVGraphicsMessageVRAMAck              vramAck;
    HyperVGraphicsMessageResolutionUpdate     resolutionUpdate;
    HyperVGraphicsMessageResolutionUpdateAck  resolutionAck;
    HyperVGraphicsMessageCursorPosition       cursorPosition;
    HyperVGraphicsMessageCursorShape          cursorShape;
    HyperVGraphicsMessageFeatureUpdate        featureUpdate;
    HyperVGraphicsMessageImageUpdate          imageUpdate;
  };
} HyperVGraphicsMessage;

#endif
