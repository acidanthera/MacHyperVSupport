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
  kHyperVShutdownNotificationTypePerformShutdown
} HyperVShutdownNotificationType;

typedef struct {
  mach_msg_header_t header;
  HyperVShutdownNotificationType type;
  io_user_reference_t ref;
} HyperVShutdownNotificationMessage;

#endif
