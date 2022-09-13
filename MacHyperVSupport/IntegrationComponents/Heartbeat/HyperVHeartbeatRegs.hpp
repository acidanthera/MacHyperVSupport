//
//  HyperVHeartbeatRegs.hpp
//  Hyper-V heartbeat driver
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVHeartbeatRegs_hpp
#define HyperVHeartbeatRegs_hpp

#include "HyperVIC.hpp"

//
// Heartbeat versions.
//
#define kHyperVHeartbeatVersionV1 { 1, 0 }
#define kHyperVHeartbeatVersionV3 { 3, 0 }

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

#endif
