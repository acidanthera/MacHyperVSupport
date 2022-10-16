//
//  HyperVIC.hpp
//  Hyper-V IC base class
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVIC_hpp
#define HyperVIC_hpp

#include "HyperV.hpp"

//
// Framework versions.
//
#define kHyperVICVersion2008  { 1, 0 }
#define kHyperVICVersionV3    { 3, 0 }

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
  UInt16 major;
  UInt16 minor;
} VMBusICVersion;

typedef struct __attribute__((packed)) {
  UInt32              pipeFlags;
  UInt32              pipeMsgs;

  VMBusICVersion      frameworkVersion;
  VMBusICMessageType  type;
  VMBusICVersion      msgVersion;
  UInt16              dataSize;
  HyperVStatus        status;
  UInt8               transactionId;
  UInt8               flags;
  UInt16              reserved;
} VMBusICMessageHeader;

typedef struct __attribute__((packed)) {
  VMBusICMessageHeader  header;

  UInt16                frameworkVersionCount;
  UInt16                messageVersionCount;
  UInt32                reserved;
  VMBusICVersion        versions[];
} VMBusICMessageNegotiate;

#endif
