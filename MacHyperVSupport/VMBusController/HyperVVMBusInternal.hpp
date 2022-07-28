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

#define HVSYSLOG(str, ...) HVSYSLOG_PRINT("HyperVVMBusController", false, 0, str, ## __VA_ARGS__)
#define HVDBGLOG(str, ...) \
  if (this->debugEnabled) HVDBGLOG_PRINT("HyperVVMBusController", false, 0, str, ## __VA_ARGS__)

extern "C" int cpu_number(void);

#endif
