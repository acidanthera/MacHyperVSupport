//
//  HyperVShutdownUserClient.h
//  Hyper-V guest shutdown userspace client
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVShutdownUserClient_h
#define HyperVShutdownUserClient_h

#include <libkern/OSTypes.h>
#include <mach/message.h>

typedef enum : UInt32 {
  kHyperVShutdownNotificationTypePerformShutdown,
  kHyperVShutdownNotificationTypeClosed
} HyperVShutdownNotificationType;

typedef struct {
  mach_msg_header_t               header;
  HyperVShutdownNotificationType  type;
  
#ifndef KERNEL
  mach_msg_trailer_t              trailer;
#endif
} HyperVShutdownNotificationMessage;

#endif
