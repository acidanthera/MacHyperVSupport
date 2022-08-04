//
//  HyperVPlatformProvider.hpp
//  Hyper-V platform functions provider
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVPlatformProvider_hpp
#define HyperVPlatformProvider_hpp

#include <IOKit/IOService.h>
#include <Headers/kern_patcher.hpp>

class HyperVPlatformProvider {
private:
  //
  // Global instance.
  //
  static HyperVPlatformProvider *instance;
  
  //
  // IOPlatformExpert::setConsoleInfo wrapping
  //
  mach_vm_address_t setConsoleInfoAddr = 0;
  uint64_t setConsoleInfoOrg[2] {};
  static IOReturn wrapSetConsoleInfo(IOPlatformExpert *that, PE_Video * consoleInfo, unsigned int op);
  
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
    if (instance == NULL) {
      instance = new HyperVPlatformProvider;
      if (instance != NULL) {
        instance->init();
      }
    }
    
    return instance;
  }
};

#endif
