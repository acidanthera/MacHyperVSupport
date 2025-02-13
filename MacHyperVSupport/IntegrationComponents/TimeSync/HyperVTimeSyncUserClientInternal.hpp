//
//  HyperVTimeSyncUserClientInternal.hpp
//  Hyper-V time synchronization user client
//
//  Copyright © 2022-2025 Goldfish64. All rights reserved.
//

#ifndef HyperVTimeSyncUserClient_hpp
#define HyperVTimeSyncUserClient_hpp

#include "HyperVICUserClient.hpp"
#include "HyperVTimeSyncUserClient.h"

class HyperVTimeSyncUserClient : public HyperVICUserClient {
  OSDeclareDefaultStructors(HyperVTimeSyncUserClient);
  HVDeclareLogFunctions("timeuser");
  typedef HyperVICUserClient super;

public:
  //
  // IOService overrides.
  //
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  void stop(IOService *provider) APPLE_KEXT_OVERRIDE;

  //
  // User client methods.
  //
  IOReturn doTimeSync(UInt64 seconds, UInt32 microseconds);
};

#endif
