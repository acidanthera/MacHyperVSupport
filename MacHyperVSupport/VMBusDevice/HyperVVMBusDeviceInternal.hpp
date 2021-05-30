//
//  HyperVVMBusDeviceInternal.hpp
//  Hyper-V VMBus device nub
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVVMBusDeviceInternal_h
#define HyperVVMBusDeviceInternal_h

#define super IOService

#define SYSLOG(str, ...) SYSLOG_PRINT("HyperVVMBusDevice", str, ## __VA_ARGS__)
#define DBGLOG(str, ...) DBGLOG_PRINT("HyperVVMBusDevice", str, ## __VA_ARGS__)

#endif
