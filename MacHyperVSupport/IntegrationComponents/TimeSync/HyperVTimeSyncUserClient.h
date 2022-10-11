//
//  HyperVTimeSyncUserClient.h
//  Hyper-V time synchronization user client
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVTimeSyncUserClient_h
#define HyperVTimeSyncUserClient_h

#include <libkern/OSTypes.h>
#include <mach/message.h>

typedef struct {
  mach_msg_header_t   header;
  UInt64              seconds;
  UInt32              microseconds;

#ifndef KERNEL
  mach_msg_trailer_t  trailer;
#endif
} HyperVTimeSyncUserClientNotificationMessage;

#endif
