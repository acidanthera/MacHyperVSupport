//
//  HyperVMouseRegs.hpp
//  Hyper-V mouse driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVMouseRegs_hpp
#define HyperVMouseRegs_hpp

#define kHyperVMouseRingBufferSize  (0x8000)

#define kHyperVMouseInitTimeout     10000

#define kHyperVMouseProtocolRequestTransactionID 0xCAFECAFE

//
// Current mouse protocol is 2.0.
//
#define kHyperVMouseVersionMajor    2
#define kHyperVMouseVersionMinor    0
#define kHyperVMouseVersion         (kHyperVMouseVersionMinor | (kHyperVMouseVersionMajor << 16))

//
// HID descriptor
//
typedef struct __attribute__((packed)) {
  UInt8       descLen;
  UInt8       descType;
  UInt16      descVersNum;
  UInt8       hidCountryCode;
  UInt8       hidNumDescriptors;
  UInt8       hidDescriptorType;
  UInt16      hidDescriptorLength;
} HyperVHIDDescriptor;

typedef enum : UInt32 {
  kHyperVPipeMessageTypeInvalid = 0,
  kHyperVPipeMessageTypeData    = 1
} HyperVPipeMessageType;

//
// Mouse message types.
//
typedef enum : UInt32 {
  kHyperVMouseMessageTypeProtocolRequest      = 0,
  kHyperVMouseMessageTypeProtocolResponse     = 1,
  kHyperVMouseMessageTypeInitialDeviceInfo    = 2,
  kHyperVMouseMessageTypeInitialDeviceInfoAck = 3,
  kHyperVMouseMessageTypeInputReport          = 4,
} HyperVMouseMessageType;

//
// Mouse info
//
typedef struct __attribute__((packed)) {
  UInt32  size;
  UInt16  vendor;
  UInt16  product;
  UInt16  version;
  UInt16  reserved[11];
} HyperVMouseDeviceInfo;

//
// Message structures.
//
// Header
typedef struct __attribute__((packed)) {
  HyperVMouseMessageType    type;
  UInt32                    size;
} HyperVMouseMessageHeader;

// Protocol request
typedef struct __attribute__((packed)) {
  HyperVMouseMessageHeader  header;

  UInt32                    versionRequested;
} HyperVMouseMessageProtocolRequest;

// Protocol response
typedef struct __attribute__((packed)) {
  HyperVMouseMessageHeader  header;

  UInt32                    versionRequested;
  UInt8                     status;
  UInt8                     reserved[3];
} HyperVMouseMessageProtocolResponse;

// Device info
typedef struct __attribute__((packed)) {
  HyperVMouseMessageHeader  header;

  HyperVMouseDeviceInfo     info;
  HyperVHIDDescriptor       hidDescriptor;
  UInt8                     hidDescriptorData[];
} HyperVMouseMessageInitialDeviceInfo;

// Device info ack
typedef struct __attribute__((packed)) {
  HyperVMouseMessageHeader  header;

  UInt8                     reserved;
} HyperVMouseMessageInitialDeviceInfoAck;

// Input report
typedef struct __attribute__((packed)) {
  HyperVMouseMessageHeader  header;

  UInt8                     data[];
} HyperVMouseMessageInputReport;

//
// Used for outgoing messages.
//
typedef struct __attribute__((packed)) {
  HyperVPipeMessageType type;
  UInt32                size;
  union {
    HyperVMouseMessageProtocolRequest       request;
    HyperVMouseMessageProtocolResponse      response;
    HyperVMouseMessageInitialDeviceInfoAck  deviceInfoAck;
  };
} HyperVMousePipeMessage;

//
// Used for incoming messages.
//
typedef struct __attribute__((packed)) {
  HyperVPipeMessageType type;
  UInt32                size;
  union {
    HyperVMouseMessageHeader                header;
    HyperVMouseMessageProtocolRequest       request;
    HyperVMouseMessageProtocolResponse      response;
    HyperVMouseMessageInitialDeviceInfo     deviceInfo;
    HyperVMouseMessageInitialDeviceInfoAck  deviceInfoAck;
    HyperVMouseMessageInputReport           inputReport;
  };
} HyperVMousePipeIncomingMessage;

#endif /* HyperVMouseRegs_hpp */
