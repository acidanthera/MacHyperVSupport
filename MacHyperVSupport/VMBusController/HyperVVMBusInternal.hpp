//
//  HyperVVMBusInternal.h
//  Hyper-V VMBus controller
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVVMBusInternal_hpp
#define HyperVVMBusInternal_hpp

#include "HyperV.hpp"

#define super IOInterruptController

#define SYSLOG(str, ...) LOG_PRINT("HyperVVMBusController", str, ## __VA_ARGS__)

#ifdef DEBUG
#define DBGLOG(str, ...) LOG_PRINT("HyperVVMBusController", str, ## __VA_ARGS__)
#else
#define DBGLOG(str, ...) {}
#endif

#endif
