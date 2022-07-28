//
//  HyperVShutdown.hpp
//  Hyper-V guest shutdown driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVShutdown_hpp
#define HyperVShutdown_hpp

#include "HyperVICService.hpp"

class HyperVShutdown : public HyperVICService {
  OSDeclareDefaultStructors(HyperVShutdown);
  typedef HyperVICService super;

private:
  bool handleShutdown(VMBusICMessageShutdownData *shutdownData);

protected:
  bool processMessage() APPLE_KEXT_OVERRIDE;

public:
  //
  // IOService overrides.
  //
  virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#endif
