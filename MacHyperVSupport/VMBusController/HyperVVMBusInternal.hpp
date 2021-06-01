//
//  HyperVVMBusInternal.h
//  Hyper-V VMBus controller
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVVMBusInternal_hpp
#define HyperVVMBusInternal_hpp

#include "HyperV.hpp"

#include <Headers/kern_api.hpp>

#undef SYSLOG
#undef DBGLOG

#define super IOInterruptController

#define SYSLOG(str, ...) SYSLOG_PRINT("HyperVVMBusController", str, ## __VA_ARGS__)
#define DBGLOG(str, ...) DBGLOG_PRINT("HyperVVMBusController", str, ## __VA_ARGS__)

extern "C" int cpu_number(void);

#endif
