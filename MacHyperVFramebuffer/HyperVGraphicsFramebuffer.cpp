//
//  HyperVGraphicsFramebuffer.cpp
//  Hyper-V synthetic graphics framebuffer driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "HyperVGraphicsFramebuffer.hpp"

OSDefineMetaClassAndStructors(HyperVGraphicsFramebuffer, super);

bool HyperVGraphicsFramebuffer::start(IOService *provider) {


  HVCheckDebugArgs();
  HVDBGLOG("Initializing Hyper-V Synthetic Framebuffer");

  if (HVCheckOffArg()) {
    HVSYSLOG("Disabling Hyper-V Synthetic Framebuffer due to boot arg");

    return false;
  }

  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");

    return false;
  }

  HVDBGLOG("Initialized Hyper-V Synthetic Framebuffer");
  return true;
}

void HyperVGraphicsFramebuffer::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V Synthetic Framebuffer");

  super::stop(provider);
}
