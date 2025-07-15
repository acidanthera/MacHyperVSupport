//
//  HyperVSnapshotRegs.hpp
//  Hyper-V snapshot (VSS) driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#ifndef HyperVSnapshotRegs_hpp
#define HyperVSnapshotRegs_hpp

#include "HyperVIC.hpp"

//
// Snapshot versions.
//
#define kHyperVSnapshotVersionV5_0  VMBUS_VERSION(5, 0)

//
// Snapshot message types.
//
typedef enum : UInt8 {
  kHyperVSnapshotMessageTypeCreate      = 0,
  kHyperVSnapshotMessageTypeDelete      = 1,
  // Hot backup is starting.
  kHyperVSnapshotMessageTypeHotBackup   = 2,
  kHyperVSnapshotMessageTypeGetDMInfo   = 3,
  kHyperVSnapshotMessageTypeBUComplete  = 4,
  // All filesystems are to be frozen.
  kHyperVSnapshotMessageTypeFreeze      = 5,
  // All filesystems are to be thawed.
  kHyperVSnapshotMessageTypeThaw        = 6,
  kHyperVSnapshotMessageTypeAutoRecover = 7
} HyperVSnapshotMessageType;

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

//
// Supported feature check, sent in response to kHyperVSnapshotMessageTypeHotBackup.
//
#define kHyperVSnapshotMessageCheckFeatureFlagNoAutoRecovery    0x5
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
