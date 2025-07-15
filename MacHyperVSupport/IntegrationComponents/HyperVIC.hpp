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
#define kHyperVICVersion2008  VMBUS_VERSION(1, 0)
#define kHyperVICVersionV3    VMBUS_VERSION(3, 0)

//
// IC message types.
//
typedef enum : UInt16 {
  // IC general negotiation.
  kVMBusICMessageTypeNegotiate    = 0,
  // Heartbeat component.
  kVMBusICMessageTypeHeartbeat    = 1,
  // KVP component.
  kVMBusICMessageTypeKVPExchange  = 2,
  // Guest shutdown component.
  kVMBusICMessageTypeShutdown     = 3,
  // Time sync component.
  kVMBusICMessageTypeTimeSync     = 4,
  // Snapshot (VSS) component.
  kVMBusICMessageTypeSnapshot     = 5,
  // File copy component.
  kVMBusICMessageTypeFileCopy     = 7
} VMBusICMessageType;

// IC message is part of a transaction.
#define kVMBusICFlagTransaction   1
// IC message is from host to guest.
#define kVMBusICFlagRequest       2
// IC message is from guest to host.
#define kVMBusICFlagResponse      4

//
// IC message header common to all integration components.
//
typedef struct __attribute__((packed)) {
  // Pipe flags? Unknown field that LIS does not use.
  UInt32              pipeFlags;
  // Pipe message size? Unknown field that LIS does not use.
  UInt32              pipeMsgSize;

  // Framework version.
  VMBusVersion        frameworkVersion;
  // Message type.
  VMBusICMessageType  type;
  // Message version.
  VMBusVersion        msgVersion;
  // Size of data excluding header.
  UInt16              dataSize;
  // Return status to Hyper-V.
  HyperVStatus        status;
  // Transaction ID. Integration components seem to disregard
  //   the VMBus transaction ID in favor of this one.
  UInt8               transactionId;
  // Message flags.
  UInt8               flags;
  // Reserved.
  UInt16              reserved;
} VMBusICMessageHeader;

//
// IC version negotiation message.
//
typedef struct __attribute__((packed)) {
  // IC message header.
  VMBusICMessageHeader  header;

  // Number of framework versions contained in message.
  UInt16                frameworkVersionCount;
  // Number of message versions contained in message.
  UInt16                messageVersionCount;
  // Reserved.
  UInt32                reserved;
  // Array containing supported framework and message versions.
  VMBusVersion          versions[];
} VMBusICMessageNegotiate;

#endif
