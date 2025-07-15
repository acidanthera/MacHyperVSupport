//
//  HyperVSnapshotUserClient.h
//  Hyper-V snapshot (VSS) user client
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#ifndef HyperVSnapshotUserClient_h
#define HyperVSnapshotUserClient_h

#include <libkern/OSTypes.h>
#include <mach/message.h>

#define kHyperVSnapshotMagic  0x4E455754
#define kHyperVSnapshotTimeoutSeconds   15

//
// Snapshot user client methods from daemon.
// These must match notifications.
//
typedef enum : UInt32 {
  // Report daemon is alive and snapshot can be completed on all mounted volumes.
  // Params: magic value, snapshot status
  kHyperVSnapshotUserClientMethodReportSnapshotAbility,
  // Indicate to user client that freeze has completed.
  // Params: magic value, freeze status
  kHyperVSnapshotUserClientMethodCompleteFreeze,
  // Indicate to user client that thaw has completed.
  // Params: magic value, thaw status
  kHyperVSnapshotUserClientMethodCompleteThaw,

  kHyperVSnapshotUserClientMethodNumberOfMethods
} HyperVSnapshotUserClientMethod;

//
// Snapshot user client notifications to daemon.
// These must match method types.
//
typedef enum : UInt32 {
  // Request from user client to ensure daemon is alive.
  kHyperVSnapshotUserClientNotificationTypeCheck,
  // Request from user client to begin freeze.
  kHyperVSnapshotUserClientNotificationTypeFreeze,
  // Request from user client to begin thaw.
  kHyperVSnapshotUserClientNotificationTypeThaw,
} HyperVSnapshotUserClientNotificationType;

//
// Snapshot user client notification message.
//
typedef struct {
  mach_msg_header_t                        header;
  HyperVSnapshotUserClientNotificationType type;
#ifndef KERNEL
  mach_msg_trailer_t                       trailer;
#endif
} HyperVSnapshotUserClientNotificationMessage;

#endif
