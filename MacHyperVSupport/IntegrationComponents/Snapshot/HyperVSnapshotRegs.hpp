//
//  HyperVSnapshotRegs.hpp
//  Hyper-V snapshot (VSS) driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#ifndef HyperVSnapshotRegs_hpp
#define HyperVSnapshotRegs_hpp

#include "HyperVIC.hpp"
#include "HyperVSnapshotRegsUser.h"

//
// Snapshot versions.
//
#define kHyperVSnapshotVersionV5_0  VMBUS_VERSION(5, 0)

//
// Snapshot message header.
//
typedef struct __attribute__((packed)) {
  // Integration services header.
  VMBusICMessageHeader      icHeader;

  // Message type.
  HyperVSnapshotMessageType type;
  // Reserved.
  UInt8                     reserved[7];
} HyperVSnapshotMessageHeader;

typedef struct __attribute__((packed)) {
  // Snapshot message header.
  HyperVSnapshotMessageHeader header;

  // Flags.
  UInt32                      flags;
} HyperVSnapshotMessageCheckFeature;

typedef struct __attribute__((packed)) {
  // Snapshot message header.
  HyperVSnapshotMessageHeader header;

  // Flags.
  UInt32                      flags;
} HyperVSnapshotMessageCheckDMInfo;

//
// Snapshot message.
//
typedef struct __attribute__((packed)) {
  union {
    VMBusICMessageHeader              icHeader;
    HyperVSnapshotMessageHeader       snapshotHeader;

    VMBusICMessageNegotiate           negotiate;
    HyperVSnapshotMessageCheckFeature checkFeature;
    HyperVSnapshotMessageCheckDMInfo  checkDMInfo;
  };
} HyperVSnapshotMessage;

#endif
