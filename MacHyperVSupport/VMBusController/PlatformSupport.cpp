//
//  PlatformSupport.cpp
//  Hyper-V VMBus additional platform logic
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVVMBusController.hpp"
#include "HyperVVMBusInternal.hpp"

#define RB_HALT    0x08


static HyperVVMBusController *callbackVMBusController = NULL;

void HyperVVMBusController::onLiluPatcherLoad(KernelPatcher &patcher) {
  DBGLOG("Patcher loaded");
  callbackVMBusController = this;

  KernelPatcher::RouteRequest requests[] = {
    { "_reboot", reboot, origReboot }
  };
  if (!patcher.routeMultiple(KernelPatcher::KernelID, requests, arrsize(requests))) {
    SYSLOG("Failed to route platform functions");
    patcher.clearError();
  }
}

int HyperVVMBusController::reboot(proc_t proc, reboot_args *args, int32_t *retval) {
  //
  // Ensure we actually shutdown if we initiated the syscall.
  // launchd on 10.6 seems to use its own zero flags when its killed, and calls this syscall from itself.
  //
  if (callbackVMBusController->isShuttingDown) {
    args->opt = RB_HALT;
  }
  return FunctionCast(reboot, callbackVMBusController->origReboot)(proc, args, retval);
}

bool HyperVVMBusController::canShutdownSystem() {
  return origReboot != 0;
}

void HyperVVMBusController::shutdownSystem() {
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
