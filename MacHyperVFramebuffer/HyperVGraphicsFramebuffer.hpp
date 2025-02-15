//
//  HyperVGraphicsFramebuffer.hpp
//  Hyper-V synthetic graphics framebuffer driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#ifndef MacHyperVFramebuffer_hpp
#define MacHyperVFramebuffer_hpp

#include <IOKit/IOService.h>

#include "HyperV.hpp"

class HyperVGraphicsFramebuffer : public IOService {
  OSDeclareDefaultStructors(HyperVGraphicsFramebuffer);
  HVDeclareLogFunctions("gfxfb");
  typedef IOService super;

public:
  //
  // IOService overrides.
  //
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#endif
