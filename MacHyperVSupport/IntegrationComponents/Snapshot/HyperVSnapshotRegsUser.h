//
//  HyperVSnapshotRegsUser.h
//  Hyper-V snapshot (VSS) driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#ifndef HyperVSnapshotRegsUser_h
#define HyperVSnapshotRegsUser_h

//
// Maximum time to stay frozen.
//
#define kHyperVSnapshotTimeoutSeconds   15

//
// Snapshot message types.
//
typedef enum : UInt8 {
  kHyperVSnapshotMessageTypeCreate      = 0,
  kHyperVSnapshotMessageTypeDelete      = 1,
  kHyperVSnapshotMessageTypeHotBackup   = 2,
  kHyperVSnapshotMessageTypeGetDMInfo   = 3,
  kHyperVSnapshotMessageTypeBUComplete  = 4,
  kHyperVSnapshotMessageTypeFreeze      = 5,
  kHyperVSnapshotMessageTypeThaw        = 6,
  kHyperVSnapshotMessageTypeAutoRecover = 7
} HyperVSnapshotMessageType;

#endif
