//
//  HyperVPlatformProvider.cpp
//  Hyper-V platform functions provider
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_patcher.hpp>
#include <Headers/kern_util.hpp>

#include "HyperVPlatformProvider.hpp"
#include "HyperV.hpp"

#include <IOKit/IOPlatformExpert.h>

#define SYSLOG(str, ...) SYSLOG_PRINT("HyperVPlatformProvider", str, ## __VA_ARGS__)
#define DBGLOG(str, ...) DBGLOG_PRINT("HyperVPlatformProvider", str, ## __VA_ARGS__)

#define RB_HALT    0x08

HyperVPlatformProvider *HyperVPlatformProvider::instance;

void HyperVPlatformProvider::init() {
  DBGLOG("Initializing provider");
  
  //
  // Lilu is used for certain functions of child devices, register patcher callback.
  //
  lilu.onPatcherLoadForce([](void *user, KernelPatcher &patcher) {
    static_cast<HyperVPlatformProvider *>(user)->onLiluPatcherLoad(patcher);
  }, this);

  //
  // Patch setConsoleInfo to call our wrapper function instead.
  // 10.6 to 10.12 may pass garbage data to setConsoleInfo from IOPCIConfigurator::configure().
  //
  KernelVersion kernelVersion = getKernelVersion();
  if (kernelVersion >= KernelVersion::SnowLeopard && kernelVersion <= KernelVersion::Sierra) {
    setConsoleInfoAddr = OSMemberFunctionCast(mach_vm_address_t, IOService::getPlatform(), &IOPlatformExpert::setConsoleInfo);
    
    // Save start of function.
    lilu_os_memcpy(setConsoleInfoOrg, (void *)setConsoleInfoAddr, sizeof(setConsoleInfoOrg));

    // Patch to call wrapper.
#if defined(__i386__)
    uint64_t patched[2] {0x25FF | ((setConsoleInfoAddr + 8) << 16), (uint32_t)wrapSetConsoleInfo};
#elif defined(__x86_64__)
    uint64_t patched[2] {0x0225FF, (uintptr_t)wrapSetConsoleInfo};
#else
#error Unsupported arch
#endif
    if (MachInfo::setKernelWriting(true, KernelPatcher::kernelWriteLock) == KERN_SUCCESS) {
      lilu_os_memcpy((void *)setConsoleInfoAddr, patched, sizeof(patched));
      MachInfo::setKernelWriting(false, KernelPatcher::kernelWriteLock);
    }
    
    DBGLOG("Patched IOPlatformExpert::setConsoleInfo");
  }
}

IOReturn HyperVPlatformProvider::wrapSetConsoleInfo(IOPlatformExpert *that, PE_Video *consoleInfo, unsigned int op) {
  DBGLOG("op %X", op);
  
  // Fix arg here
  if (op == kPEBaseAddressChange && consoleInfo != NULL) {
    PE_Video consoleInfoCurrent;
    IOService::getPlatform()->getConsoleInfo(&consoleInfoCurrent);
    
    unsigned long baseAddr = consoleInfo->v_baseAddr;
    memcpy(consoleInfo, &consoleInfoCurrent, sizeof (*consoleInfo));
    consoleInfo->v_baseAddr = baseAddr;
  }
  
  // Restore original function.
  if (MachInfo::setKernelWriting(true, KernelPatcher::kernelWriteLock) == KERN_SUCCESS) {
    lilu_os_memcpy((void *)instance->setConsoleInfoAddr, instance->setConsoleInfoOrg, sizeof(instance->setConsoleInfoOrg));
    MachInfo::setKernelWriting(false, KernelPatcher::kernelWriteLock);
  }
  
  IOReturn result = FunctionCast(wrapSetConsoleInfo, instance->setConsoleInfoAddr)(that, consoleInfo, op);
  
  // Patch again if kPEBaseAddressChange was not the operation.
  if (op != kPEBaseAddressChange) {
#if defined(__i386__)
    uint64_t patched[2] {0x25FF | ((instance->setConsoleInfoAddr + 8) << 16), (uint32_t)wrapSetConsoleInfo};
#elif defined(__x86_64__)
    uint64_t patched[2] {0x0225FF, (uintptr_t)wrapSetConsoleInfo};
#else
#error Unsupported arch
#endif
    if (MachInfo::setKernelWriting(true, KernelPatcher::kernelWriteLock) == KERN_SUCCESS) {
      lilu_os_memcpy((void *)instance->setConsoleInfoAddr, patched, sizeof(patched));
      MachInfo::setKernelWriting(false, KernelPatcher::kernelWriteLock);
    }
  } else {
    DBGLOG("kPEBaseAddressChange specified, not patching again");
  }

  return result;
}

void HyperVPlatformProvider::onLiluPatcherLoad(KernelPatcher &patcher) {
  DBGLOG("Patcher loaded");

  KernelPatcher::RouteRequest requests[] = {
    { "_reboot", reboot, origReboot }
  };
  if (!patcher.routeMultiple(KernelPatcher::KernelID, requests, arrsize(requests))) {
    SYSLOG("Failed to route platform functions");
    patcher.clearError();
  }
}

int HyperVPlatformProvider::reboot(proc_t proc, reboot_args *args, int32_t *retval) {
  //
  // Ensure we actually shutdown if we initiated the syscall.
  // launchd on 10.6 seems to use its own zero flags when its killed, and calls this syscall from itself.
  //
  if (instance->isShuttingDown) {
    args->opt = RB_HALT;
  }
  return FunctionCast(reboot, instance->origReboot)(proc, args, retval);
}

bool HyperVPlatformProvider::canShutdownSystem() {
  return origReboot != 0;
}

void HyperVPlatformProvider::shutdownSystem() {
  DBGLOG("Shutdown initiated");
  
  //
  // Invoke reboot syscall to shutdown the system.
  //
  if (origReboot != 0) {
    isShuttingDown = true;
    
    reboot_args args;
    args.opt = RB_HALT;
    args.command = 0;

    //
    // This should not return.
    //
    reboot(kernproc, &args, NULL);
  }
}
