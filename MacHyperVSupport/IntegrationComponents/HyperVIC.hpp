//
//  HyperVIC.hpp
//  Hyper-V IC base class
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVIC_h
#define HyperVIC_h

//
// Message types.
//
typedef enum : UInt16 {
  kVMBusICMessageTypeNegotiate    = 0,
  kVMBusICMessageTypeHeartbeat    = 1,
  kVMBusICMessageTypeKVPExchange  = 2,
  kVMBusICMessageTypeShutdown     = 3,
  kVMBusICMessageTypeTimeSync     = 4,
  kVMBusICMessageTypeVSS          = 5,
  kVMBusICMessageTypeFileCopy     = 7
} VMBusICMessageType;

#define kVMBusICFlagTransaction   1
#define kVMBusICFlagRequest       2
#define kVMBusICFlagResponse      4

//
// Header and common negotiation message.
//
typedef struct __attribute__((packed)) {
  UInt32              pipeFlags;
  UInt32              pipeMsgs;
  
  UInt32              frameworkVersion;
  VMBusICMessageType  type;
  UInt32              msgVersion;
  UInt16              dataSize;
  UInt32              status;
  UInt8               transactionId;
  UInt8               flags;
  UInt16              reserved;
} VMBusICMessageHeader;

typedef struct __attribute__((packed)) {
  VMBusICMessageHeader  header;
  
  UInt16                frameworkVersionCount;
  UInt16                messageVersionCount;
  UInt32                reserved;
  UInt32                versions[];
} VMBusICMessageNegotiate;

//
// Heartbeat messages.
//
typedef struct __attribute__((packed)) {
  VMBusICMessageHeader  header;
  
  UInt64                sequence;
  UInt32                reserved[8];
} VMBusICMessageHeartbeatSequence;

typedef struct __attribute__((packed)) {
  union {
    VMBusICMessageHeader            header;
    VMBusICMessageNegotiate         negotiate;
    VMBusICMessageHeartbeatSequence heartbeat;
  };
} VMBusICMessageHeartbeat;

//
// Shutdown messages.
//
typedef struct __attribute__((packed)) {
  VMBusICMessageHeader  header;
  
  UInt32                reason;
  UInt32                timeoutSeconds;
  UInt32                flags;
  char                  displayMessage[2048];
} VMBusICMessageShutdownData;

typedef struct __attribute__((packed)) {
  union {
    VMBusICMessageHeader            header;
    VMBusICMessageNegotiate         negotiate;
    VMBusICMessageShutdownData      shutdown;
  };
} VMBusICMessageShutdown;

#endif
