//
//  HyperVFileCopyRegs.hpp
//  Hyper-V file copy driver
//
//  Copyright © 2022 flagers. All rights reserved.
//

#ifndef HyperVFileCopyRegs_hpp
#define HyperVFileCopyRegs_hpp

#include "HyperVIC.hpp"
#include "HyperVFileCopyRegsUser.h"

#define kHyperVFileCopyBufferSize   (8 * PAGE_SIZE)

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
#define kHyperVFileCopyMaxDataSize  kHyperVFileCopyFragmentSize
#else
#define kHyperVFileCopyMaxDataSize  sizeof(io_struct_inband_t)
#endif

//
// File copy versions.
//
#define kHyperVFileCopyVersionV1_1 VMBUS_VERSION(1, 1)

//
// File copy messages.
//
typedef struct __attribute__((packed)) {
  VMBusICMessageHeader      icHeader;

  HyperVFileCopyMessageType type;
  uuid_t                    unused1;
  uuid_t                    unused2;
} HyperVFileCopyMessageHeader;

typedef struct __attribute__((packed)) {
  HyperVFileCopyMessageHeader header;

  //
  // fileName & filePath are UTF-16 strings.
  //
  UInt16                      fileName[kHyperVFileCopyMaxPath];
  UInt16                      filePath[kHyperVFileCopyMaxPath];

  HyperVFileCopyMessageFlags  flags;
  UInt64                      fileSize;
} HyperVFileCopyMessageStartCopy;

typedef struct __attribute__((packed)) {
  HyperVFileCopyMessageHeader header;

  UInt32                      reserved;
  UInt64                      offset;
  UInt32                      size;
  UInt8                       data[kHyperVFileCopyFragmentSize];
} HyperVFileCopyMessageDataFragment;

typedef struct __attribute__((packed)) {
  union {
    VMBusICMessageHeader              icHeader;
    HyperVFileCopyMessageHeader       fileCopyHeader;

    VMBusICMessageNegotiate           negotiate;
    HyperVFileCopyMessageStartCopy    startCopy;
    HyperVFileCopyMessageDataFragment dataFragment;
  };
} HyperVFileCopyMessage;

#endif
