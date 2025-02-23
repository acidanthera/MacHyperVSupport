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
#define kHyperVShutdownVersionV1_0  VMBUS_VERSION(1, 0)
#define kHyperVShutdownVersionV3_0  VMBUS_VERSION(3, 0)
#define kHyperVShutdownVersionV3_1  VMBUS_VERSION(3, 1)
#define kHyperVShutdownVersionV3_2  VMBUS_VERSION(3, 2)

//
// Shutdown flags.
//
typedef enum : UInt32 {
  kVMBusICShutdownFlagsShutdown        = 0,
  kVMBusICShutdownFlagsShutdownForced  = 1,
  kVMBusICShutdownFlagsRestart         = 2,
  kVMBusICShutdownFlagsRestartForced   = 3,
  kVMBusICShutdownFlagsHibernate       = 4,
  kVMBusICShutdownFlagsHibernateForced = 5
} VMBusICShutdownFlags;

//
// Shutdown messages.
//
typedef struct __attribute__((packed)) {
  VMBusICMessageHeader  header;

  UInt32                reason;
  UInt32                timeoutSeconds;
  VMBusICShutdownFlags  flags;
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
