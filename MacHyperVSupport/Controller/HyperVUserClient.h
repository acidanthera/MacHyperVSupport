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

typedef enum : UInt32 {
  kHyperVUserClientNotificationTypePerformShutdown = 0x66697368,
  kHyperVUserClientNotificationTypePerformRestart,
  kHyperVUserClientNotificationTypeTimeSync
} HyperVUserClientNotificationType;

typedef struct {
  UInt64 seconds;
  UInt32 microseconds;
} HyperVUserClientTimeData;


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
