//
//  HyperVShutdownUserClient.h
//  Hyper-V guest shutdown user client
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVShutdownUserClient_h
#define HyperVShutdownUserClient_h

#include <libkern/OSTypes.h>
#include <mach/message.h>

#define kHyperVShutdownMagic  0x66697368

typedef enum : UInt32 {
  kHyperVShutdownUserClientMethodReportShutdownAbility,

  kHyperVShutdownUserClientMethodNumberOfMethods
} HyperVShutdownUserClientMethod;

typedef enum : UInt32 {
  kHyperVShutdownUserClientNotificationTypeCheck,
  kHyperVShutdownUserClientNotificationTypePerformShutdown,
  kHyperVShutdownUserClientNotificationTypePerformRestart,
} HyperVShutdownUserClientNotificationType;

typedef struct {
  mach_msg_header_t                        header;
  HyperVShutdownUserClientNotificationType type;
#ifndef KERNEL
  mach_msg_trailer_t                       trailer;
#endif
} HyperVShutdownUserClientNotificationMessage;

#endif
