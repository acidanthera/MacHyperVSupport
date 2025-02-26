//
//  HyperVPlatformProvider.cpp
//  Hyper-V platform functions provider
//
//  Copyright © 2021-2025 Goldfish64. All rights reserved.
//

#include "HyperVPlatformProvider.hpp"

#include <Headers/kern_api.hpp>
#include <Headers/kern_patcher.hpp>
#include <Headers/kern_util.hpp>

HyperVPlatformProvider *HyperVPlatformProvider::_instance;

void HyperVPlatformProvider::init() {
  HVCheckDebugArgs();
  HVDBGLOG("Initializing provider");

  //
  // Lilu is used for function hooking/patching, register patcher callback.
  //
  lilu.onPatcherLoadForce([](void *user, KernelPatcher &patcher) {
    static_cast<HyperVPlatformProvider *>(user)->onLiluPatcherLoad(patcher);
  }, this);
}

void HyperVPlatformProvider::onLiluPatcherLoad(KernelPatcher &patcher) {
  HVDBGLOG("Patcher loaded");
}
