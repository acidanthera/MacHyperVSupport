//
//  HyperVShutdownRegs.hpp
//  Hyper-V guest shutdown driver
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVShutdownRegs_hpp
#define HyperVShutdownRegs_hpp

#include "HyperVIC.hpp"

//
// Shutdown versions.
//
#define kHyperVShutdownVersionV1_0  { 1, 0 }
#define kHyperVShutdownVersionV3_0  { 3, 0 }
#define kHyperVShutdownVersionV3_1  { 3, 1 }
#define kHyperVShutdownVersionV3_2  { 3, 2 }

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
