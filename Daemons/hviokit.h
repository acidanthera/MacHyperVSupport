//
//  hviokit.h
//  Hyper-V userspace I/O Kit support
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef hviokit_h
#define hviokit_h

#include <mach/mach_port.h>
#include <IOKit/IOKitLib.h>

void hvIOKitNotificationHandler(io_connect_t connection, CFMachPortRef port, void *msg, CFIndex size, void *info);
IOReturn hvIOKitSetupIOKitNotifications(const char *name);


#endif
