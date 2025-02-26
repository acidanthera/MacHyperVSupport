//
//  HyperVPlatformProvider.hpp
//  Hyper-V platform functions provider
//
//  Copyright © 2021-2025 Goldfish64. All rights reserved.
//

#ifndef HyperVPlatformProvider_hpp
#define HyperVPlatformProvider_hpp

#include <IOKit/IOService.h>
#include <Headers/kern_patcher.hpp>

#include "HyperV.hpp"

class HyperVPlatformProvider {
  HVDeclareLogFunctionsNonIOKit("prov", "HyperVPlatformProvider");

private:
  //
  // Global instance.
  //
  static HyperVPlatformProvider *_instance;

  //
  // Initialization function.
  //
  void init();
  void onLiluPatcherLoad(KernelPatcher &patcher);

public:
  //
  // Instance creator.
  //
  static HyperVPlatformProvider *getInstance() {
    if (_instance == nullptr) {
      _instance = new HyperVPlatformProvider;
      if (_instance != nullptr) {
        _instance->init();
      }
    }

    return _instance;
  }
};

#endif
