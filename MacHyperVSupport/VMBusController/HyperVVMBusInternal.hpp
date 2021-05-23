//
//  HyperVVMBusInternal.h
//  MacHyperVServices
//
//  Created by John Davis on 5/7/21.
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
