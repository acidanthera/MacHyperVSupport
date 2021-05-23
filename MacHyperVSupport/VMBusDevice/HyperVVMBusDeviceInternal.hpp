//
//  HyperVVMBusDeviceInternal.hpp
//  Hyper-V VMBus device nub
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVVMBusDeviceInternal_h
#define HyperVVMBusDeviceInternal_h

#define super IOService

#define SYSLOG(str, ...) LOG_PRINT("HyperVVMBusDevice", str, ## __VA_ARGS__)

#ifdef DEBUG
#define DBGLOG(str, ...) LOG_PRINT("HyperVVMBusDevice", str, ## __VA_ARGS__)
#else
#define DBGLOG(str, ...) {}
#endif

#endif /* HyperVVMBusDeviceInternal_h */
