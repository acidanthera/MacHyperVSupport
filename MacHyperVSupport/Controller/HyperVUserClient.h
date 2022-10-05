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

#define kHyperVUserClientNotificationMessageDataStandardLength 64
#define kHyperVUserClientNotificationMessageDataLargeLength (kHyperVUserClientNotificationMessageDataStandardLength + (6 * 1024))

typedef enum {
    kMethodReturnFileCopy,
    
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
  kHyperVUserClientFileCopyOperationCancelFileCopy    = 3
} HyperVUserClientFileCopyOperation;

typedef enum : UInt32 {
  kHyperVUserClientFileCopyFlagsOverwrite   = 1,
  kHyperVUserClientFileCopyFlagsCreatePath  = 2
} HyperVUserClientFileCopyFlags;

typedef struct __attribute__((packed)) {
  HyperVUserClientFileCopyFlags  copyFlags;
  UInt64                         fileSize;
  UInt8                          fileName[1024];
  UInt8                          filePath[1024];
} HyperVUserClientFileCopyStartCopy;

typedef struct __attribute__((packed)) {
  UInt32  reserved;
  UInt64  offset;
  UInt32  size;
  UInt8   data[(6 * 1024)];
} HyperVUserClientFileCopyDoCopy;

typedef struct __attribute__((packed)) {
  HyperVUserClientFileCopyOperation    operation;
  union {
    HyperVUserClientFileCopyStartCopy  startCopy;
    HyperVUserClientFileCopyDoCopy     doCopy;
  } operationData;
} HyperVUserClientFileCopy;

typedef union {
  struct {
    mach_msg_header_t                header;
    HyperVUserClientNotificationType type;
    UInt8                            data[kHyperVUserClientNotificationMessageDataStandardLength];
    UInt32                           dataLength;
    
#ifndef KERNEL
    mach_msg_trailer_t               trailer;
#endif
  } standard;
  struct {
    mach_msg_header_t                header;
    HyperVUserClientNotificationType type;
    UInt8                            data[kHyperVUserClientNotificationMessageDataLargeLength];
    UInt32                           dataLength;
    
#ifndef KERNEL
    mach_msg_trailer_t               trailer;
#endif
  } large;
} HyperVUserClientNotificationMessage;

#endif
