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

#define  PAD_(t)  (sizeof(uint64_t) <= sizeof(t) \
     ? 0 : sizeof(uint64_t) - sizeof(t))
#define  PADL_(t)  0
#define  PADR_(t)  PAD_(t)

struct reboot_args {
  char opt_l_[PADL_(int)];
  int opt;
  char opt_r_[PADR_(int)];
  char command_l_[PADL_(user_addr_t)];
  user_addr_t command;
  char command_r_[PADR_(user_addr_t)];
};

class HyperVPlatformProvider {
private:
  //
  // Global instance.
  //
  static HyperVPlatformProvider *instance;
  
  //
  // Shutdown functions.
  //
  bool isShuttingDown = false;
  mach_vm_address_t origReboot {};
  static int reboot(proc_t proc, reboot_args *args, __unused int32_t *retval);
  
  //
  // IOPlatformExpert::setConsoleInfo wrapping
  //
  mach_vm_address_t setConsoleInfoAddr;
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
  
  //
  // Additional platform functions.
  //
  bool canShutdownSystem();
  void shutdownSystem();
  
};

#undef DBGLOG
#undef SYSLOG

#endif
