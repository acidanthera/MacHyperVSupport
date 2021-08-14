//
//  HyperVNetwork.cpp
//  Hyper-V network driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVNetwork_hpp
#define HyperVNetwork_hpp

#include "HyperVVMBusDevice.hpp"

#define super IOService

#define SYSLOG(str, ...) SYSLOG_PRINT("HyperVNetwork", str, ## __VA_ARGS__)
#define DBGLOG(str, ...) DBGLOG_PRINT("HyperVNetwork", str, ## __VA_ARGS__)

class HyperVNetwork : public IOService {
  OSDeclareDefaultStructors(HyperVNetwork);

private:
  //
  // Parent VMBus device.
  //
  HyperVVMBusDevice       *hvDevice;
  
  void handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count);
  
  bool connectNetwork();
  
public:
  //
  // IOService overrides.
  //
  virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#endif /* HyperVNetwork_hpp */
