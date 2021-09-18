//
//  HyperVKeyboardRegs.h
//  Hyper-V keyboard driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVKeyboardRegs_hpp
#define HyperVKeyboardRegs_hpp

#define kHyperVKeyboardRingBufferSize (0x8000)

//
// Current keyboard protocol is 1.0.
//
#define kHyperVKeyboardVersionMajor   1
#define kHyperVKeyboardVersionMinor   0
#define kHyperVKeyboardVersion        (kHyperVKeyboardVersionMinor | (kHyperVKeyboardVersionMajor << 16))

//
// Keyboard message types.
//
typedef enum : UInt32 {
  kHyperVKeyboardMessageTypeProtocolRequest   = 1,
  kHyperVKeyboardMessageTypeProtocolResponse  = 2,
  kHyperVKeyboardMessageTypeEvent             = 3,
  kHyperVKeyboardMessageTypeLEDIndicators     = 4
} HyperVKeyboardMessageType;

//
// Message structures.
//
// Header
typedef struct __attribute__((packed)) {
  HyperVKeyboardMessageType   type;
} HyperVKeyboardMessageHeader;

// Protocol request
typedef struct __attribute__((packed)) {
  HyperVKeyboardMessageHeader header;

  UInt32                      versionRequested;
} HyperVKeyboardMessageProtocolRequest;

// Protocol response
#define kHyperVKeyboardProtocolResponseAccepted 1
typedef struct __attribute__((packed)) {
  HyperVKeyboardMessageHeader header;
  
  UInt32                      status;
} HyperVKeyboardMessageProtocolResponse;

// Keystroke
typedef struct __attribute__((packed)) {
  HyperVKeyboardMessageHeader header;
  
  UInt16                      makeCode;
  UInt16                      reserved;
  UInt32                      isUnicode : 1;
  UInt32                      isBreak   : 1;
  UInt32                      isE0      : 1;
  UInt32                      isE1      : 1;
  UInt32                      reserved2 : 28;
} HyperVKeyboardMessageKeystroke;

//
// All messages.
//
typedef union __attribute__((packed)) {
  HyperVKeyboardMessageHeader           header;
  HyperVKeyboardMessageProtocolRequest  protocolRequest;
  HyperVKeyboardMessageProtocolResponse protocolResponse;
  HyperVKeyboardMessageKeystroke        keystroke;
} HyperVKeyboardMessage;

#endif
