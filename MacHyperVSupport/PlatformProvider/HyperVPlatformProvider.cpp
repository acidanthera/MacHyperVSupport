//
//  HyperVPlatformProvider.cpp
//  Hyper-V platform functions provider
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>

#include "HyperVPlatformProvider.hpp"
#include "HyperV.hpp"

#define SYSLOG(str, ...) SYSLOG_PRINT("HyperVPlatformProvider", str, ## __VA_ARGS__)
#define DBGLOG(str, ...) DBGLOG_PRINT("HyperVPlatformProvider", str, ## __VA_ARGS__)

#define RB_HALT    0x08

HyperVPlatformProvider *HyperVPlatformProvider::instance;

void HyperVPlatformProvider::init() {
  DBGLOG("Initializing provider");
  
  //
  // Lilu is used for certain functions of child devices, register patcher callback.
  //
  lilu.onPatcherLoad([](void *user, KernelPatcher &patcher) {
    static_cast<HyperVPlatformProvider *>(user)->onLiluPatcherLoad(patcher);
  }, this);
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
