//
//  HyperVFileCopyRegs.hpp
//  Hyper-V file copy driver
//
//  Copyright © 2022 flagers. All rights reserved.
//

#ifndef HyperVFileCopyRegs_hpp
#define HyperVFileCopyRegs_hpp

#include "HyperVIC.hpp"
#include "HyperVUserClient.h"

#define kHyperVFileCopyMaxPath       260
#define kHyperVFileCopyBufferSize    (8 * PAGE_SIZE)

//
// File Copy versions.
//
#define kHyperVFileCopyVersionV1 { 1, 1 }

//
// File Copy messages.
//
typedef struct __attribute__((packed)) {
  VMBusICMessageHeader               header;
  
  HyperVUserClientFileCopyOperation  operation;
  uuid_t                             unused1;
  uuid_t                             unused2;
} VMBusICMessageFileCopyHeader;

typedef struct __attribute__((packed)) {
  VMBusICMessageFileCopyHeader   fcopyHeader;
  // fileName & filePath are UTF-16 strings
  UInt16                         fileName[kHyperVFileCopyMaxPath];
  UInt16                         filePath[kHyperVFileCopyMaxPath];
  HyperVUserClientFileCopyFlags  copyFlags;
  UInt64                         fileSize;
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
