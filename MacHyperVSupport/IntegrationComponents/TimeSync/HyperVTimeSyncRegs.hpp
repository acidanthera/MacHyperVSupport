//
//  HyperVTimeSyncRegs.hpp
//  Hyper-V time synchronization driver
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVTimeSyncRegs_hpp
#define HyperVTimeSyncRegs_hpp

#include "HyperVIC.hpp"

//
// Time sync versions.
//
#define kHyperVTimeSyncVersionV1_0  { 1, 0 }
#define kHyperVTimeSyncVersionV3_0  { 3, 0 }
#define kHyperVTimeSyncVersionV4_0  { 4, 0 }

//
// Time sync flags.
//
typedef enum : UInt8 {
  kVMBusICTimeSyncFlagsProbe  = 0,
  kVMBusICTimeSyncFlagsSync   = 1,
  kVMBusICTimeSyncFlagsSample = 2
} VMBusICTimeSyncFlags;

//
// Time sync messages.
//
// Used in v1 and v3.
//
typedef struct __attribute__((packed)) {
  VMBusICMessageHeader  header;

  UInt64                parentTime;
  UInt64                childTime;
  UInt64                roundTripTime;
  VMBusICTimeSyncFlags  flags;
} VMBusICMessageTimeSyncData;

//
// Used in v4.
//
typedef struct __attribute__((packed)) {
  VMBusICMessageHeader  header;

  UInt64                parentTime;
  UInt64                referenceTime;
  VMBusICTimeSyncFlags  flags;
  UInt8                 leapFlags;
  UInt8                 stratum;
  UInt8                 reserved[3];
} VMBusICMessageTimeSyncRefData;

typedef struct __attribute__((packed)) {
  union {
    VMBusICMessageHeader            header;
    VMBusICMessageNegotiate         negotiate;
    VMBusICMessageTimeSyncData      timeSync;
    VMBusICMessageTimeSyncRefData   timeSyncRef;
  };
} VMBusICMessageTimeSync;

#endif
