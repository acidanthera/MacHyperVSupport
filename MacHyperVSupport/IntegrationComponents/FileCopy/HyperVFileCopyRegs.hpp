//
//  HyperVFileCopyRegs.hpp
//  Hyper-V file copy driver
//
//  Copyright © 2022 flagers. All rights reserved.
//

#ifndef HyperVFileCopyRegs_hpp
#define HyperVFileCopyRegs_hpp

#include "HyperVIC.hpp"

#define kHyperVFileCopyMaxPath       260
#define kHyperVFileCopyFragmentSize  (6 * 1024)
#define kHyperVFileCopyBufferSize    (8 * PAGE_SIZE)

//
// File Copy versions.
//
#define kHyperVFileCopyVersionV1 { 1, 1 }

//
// File Copy operations.
//
typedef enum : UInt32 {
  kVMBusICFileCopyOperationStartFileCopy    = 0,
  kVMBusICFileCopyOperationWriteToFile      = 1,
  kVMBusICFileCopyOperationCompleteFileCopy = 2,
  kVMBusICFileCopyOperationCancelFileCopy   = 3
} VMBusICFileCopyOperation;

//
// File Copy messages.
//
typedef struct __attribute__((packed)) {
  VMBusICMessageHeader      header;
  
  VMBusICFileCopyOperation  operation;
  uuid_t                    unused1;
  uuid_t                    unused2;
} VMBusICMessageFileCopyHeader;

typedef enum : UInt32 {
  kVMBusICFileCopyFlagsOverwrite    = 1,
  kVMBusICFileCopyFlagsCreatePath   = 2
} VMBusICFileCopyFlags;

typedef struct __attribute__((packed)) {
  VMBusICMessageFileCopyHeader  fcopyHeader;
  // fileName & filePath are UTF-16 strings
  UInt16                        fileName[kHyperVFileCopyMaxPath];
  UInt16                        filePath[kHyperVFileCopyMaxPath];
  VMBusICFileCopyFlags          copyFlags;
  UInt64                        fileSize;
} VMBusICMessageFileCopyStartCopy;

typedef struct __attribute__((packed)) {
  VMBusICMessageFileCopyHeader  fcopyHeader;
  UInt32                        reserved;
  UInt64                        offset;
  UInt32                        size;
  UInt8                         data[kHyperVFileCopyFragmentSize];
} VMBusICMessageFileCopyDoCopy;

typedef struct __attribute__((packed)) {
  union {
    VMBusICMessageHeader             header;
    VMBusICMessageNegotiate          negotiate;

    VMBusICMessageFileCopyHeader     fcopyHeader;
    VMBusICMessageFileCopyStartCopy  startCopy;
    VMBusICMessageFileCopyDoCopy     doCopy;
  };
} VMBusICMessageFileCopy;

#endif
