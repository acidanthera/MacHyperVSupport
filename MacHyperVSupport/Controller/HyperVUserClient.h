//
//  HyperVUserClient.h
//  Hyper-V userspace client
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVUserClient_h
#define HyperVUserClient_h

#include <libkern/OSTypes.h>
#include <mach/message.h>

#define kHyperVUserClientNotificationMessageDataLength 64

typedef enum {
  kMethodFileCopyReturnGeneric,
  kMethodFileCopyGetStartCopyData,
  kMethodFileCopyGetDoCopyData,
  kNumberOfMethods // Must be last
} HyperVUserClientMethod;

typedef enum : UInt32 {
  kHyperVUserClientStatusSuccess        = 0x00000000,
  kHyperVUserClientStatusFailure        = 0x80004005,
  kHyperVUserClientStatusTimedOut       = 0x800705B4,
  kHyperVUserClientStatusInvalidArg     = 0x80070057,
  kHyperVUserClientStatusAlreadyExists  = 0x80070050,
  kHyperVUserClientStatusDiskFull       = 0x80070070
} HyperVUserClientStatusCode;

typedef enum : UInt32 {
  kHyperVUserClientNotificationTypePerformShutdown = 0x66697368,
  kHyperVUserClientNotificationTypePerformRestart,
  kHyperVUserClientNotificationTypeTimeSync,
  kHyperVUserClientNotificationTypeFileCopy
} HyperVUserClientNotificationType;

typedef struct {
  UInt64 seconds;
  UInt32 microseconds;
} HyperVUserClientTimeData;

typedef enum : UInt32 {
  kHyperVUserClientFileCopyOperationStartFileCopy     = 0,
  kHyperVUserClientFileCopyOperationWriteToFile       = 1,
  kHyperVUserClientFileCopyOperationCompleteFileCopy  = 2,
  kHyperVUserClientFileCopyOperationCancelFileCopy    = 3,
  kHyperVUserClientFileCopySetup                      = 4
} HyperVUserClientFileCopyOperation;

typedef enum : UInt32 {
  kHyperVUserClientFileCopyFlagsOverwrite   = 1,
  kHyperVUserClientFileCopyFlagsCreatePath  = 2
} HyperVUserClientFileCopyFlags;

typedef struct __attribute__((packed)) {
  UInt8  fileName[PATH_MAX];
  UInt8  filePath[PATH_MAX];
} HyperVUserClientFileCopyStartCopyData;

#define kHyperVFileCopyFragmentSize (6 * 1024)
typedef struct __attribute__((packed)) {
  UInt8   data[kHyperVFileCopyFragmentSize];
} HyperVUserClientFileCopyDoCopyData;

typedef struct {
  UInt64                         fileSize;
  HyperVUserClientFileCopyFlags  copyFlags;
} HyperVUserClientFileCopyStartCopyParam;

typedef struct {
  UInt64  offset;
  UInt32  size;
} HyperVUserClientFileCopyDoCopyParam;

typedef struct {
  HyperVUserClientFileCopyOperation  operation;
  union {
    HyperVUserClientFileCopyStartCopyParam startCopy;
    HyperVUserClientFileCopyDoCopyParam doCopy;
  };
} HyperVUserClientFileCopyMessage;

typedef struct {
  mach_msg_header_t                header;
  HyperVUserClientNotificationType type;
  UInt8                            data[kHyperVUserClientNotificationMessageDataLength];
  UInt32                           dataLength;
  
#ifndef KERNEL
  mach_msg_trailer_t               trailer;
#endif
} HyperVUserClientNotificationMessage;

#endif
