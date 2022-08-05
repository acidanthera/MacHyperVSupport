//
//  HyperVHeartbeat.hpp
//  Hyper-V heartbeat driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVHeartbeat_hpp
#define HyperVHeartbeat_hpp

#include "HyperVICService.hpp"

class HyperVHeartbeat : public HyperVICService {
  OSDeclareDefaultStructors(HyperVHeartbeat);
  HVDeclareLogFunctionsVMBusChild("heart");
  typedef HyperVICService super;

private:
  bool firstHeartbeatReceived;

protected:
  bool processMessage() APPLE_KEXT_OVERRIDE;

public:
  //
  // IOService overrides.
  //
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#endif
