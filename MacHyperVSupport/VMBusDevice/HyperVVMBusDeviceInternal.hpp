//
//  HyperVVMBusDeviceInternal.hpp
//  Hyper-V VMBus device nub
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVVMBusDeviceInternal_h
#define HyperVVMBusDeviceInternal_h

#define HVSYSLOG(str, ...) HVSYSLOG_PRINT("HyperVVMBusDevice", true, this->channelId, str, ## __VA_ARGS__)
#define HVDBGLOG(str, ...) HVDBGLOG_PRINT("HyperVVMBusDevice", true, this->channelId, str, ## __VA_ARGS__)

#define MSGDBG(str, ...) \
  if (this->debugPackets) HVDBGLOG_PRINT("HyperVVMBusDevice", true, this->channelId, str, ## __VA_ARGS__)

#endif
