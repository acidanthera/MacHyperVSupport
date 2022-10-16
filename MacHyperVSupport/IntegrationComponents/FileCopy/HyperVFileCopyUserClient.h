//
//  HyperVFileCopyUserClient.h
//  Hyper-V file copy user client
//
//  Copyright © 2022 flagers, Goldfish64. All rights reserved.
//

#ifndef HyperVFileCopyUserClient_h
#define HyperVFileCopyUserClient_h

#include <libkern/OSTypes.h>
#include <mach/message.h>
#include <sys/syslimits.h>

#include "HyperVFileCopyRegsUser.h"

typedef enum : UInt32 {
  kHyperVFileCopyUserClientMethodGetFilePath,
  kHyperVFileCopyUserClientMethodGetNextDataFragment,
  kHyperVFileCopyUserClientMethodCompleteIO,

  kHyperVFileCopyUserClientMethodNumberOfMethods
} HyperVFileCopyUserClientMethod;

typedef struct {
  UInt8 fileName[NAME_MAX];
  UInt8 filePath[PATH_MAX];
} HyperVFileCopyUserClientStartCopyData;

typedef struct {
  HyperVFileCopyMessageFlags flags;
  UInt64                     fileSize;
} HyperVFileCopyUserClientNotificationMessageStartCopy;

typedef struct {
  UInt64 offset;
  UInt32 size;
} HyperVFileCopyUserClientNotificationMessageDataFragment;

typedef struct {
  mach_msg_header_t         header;
  union {
    HyperVFileCopyUserClientNotificationMessageStartCopy    startCopy;
    HyperVFileCopyUserClientNotificationMessageDataFragment dataFragment;
  };
  HyperVFileCopyMessageType type;

#ifndef KERNEL
  mach_msg_trailer_t        trailer;
#endif
} HyperVFileCopyUserClientNotificationMessage;

#endif
