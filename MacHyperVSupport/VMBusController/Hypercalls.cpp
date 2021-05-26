

#include "HyperVVMBusController.hpp"
#include "HyperVVMBusInternal.hpp"

#include <mach/vm_prot.h>

extern vm_map_t kernel_map;

bool HyperVVMBusController::initHypercalls() {
  UInt64 hvHypercall;
  UInt64 hypercallPhysAddr;
  
  //
  // Allocate hypercall memory.
  // This must be a page that can be executed.
  //
  if (vm_allocate(kernel_map, reinterpret_cast<vm_address_t *>(&hypercallPage), PAGE_SIZE, VM_FLAGS_ANYWHERE) != KERN_SUCCESS) {
    return false;
  }
  if (vm_protect(kernel_map, reinterpret_cast<vm_address_t>(hypercallPage), PAGE_SIZE, false, VM_PROT_ALL) != KERN_SUCCESS) {
    freeHypercallPage();
    return false;
  }
  
  hypercallDesc = IOMemoryDescriptor::withAddress(hypercallPage, PAGE_SIZE, kIODirectionInOut);
  if (hypercallDesc == NULL) {
    freeHypercallPage();
    return false;
  }
  hypercallDesc->prepare();
  hypercallPhysAddr = hypercallDesc->getPhysicalAddress();

  //
  // Setup hypercall page with Hyper-V.
  //
  hvHypercall = rdmsr64(kHyperVMsrHypercall);
  DBGLOG("Hypercall MSR current value: 0x%llX", hvHypercall);
  DBGLOG("Allocated hypercall page to phys 0x%llX", hypercallPhysAddr);
  
  hvHypercall = ((hypercallPhysAddr << PAGE_SHIFT) >> kHyperVMsrHypercallPageShift)
                | (hvHypercall & kHyperVMsrHypercallRsvdMask) | kHyperVMsrHypercallEnable;
  wrmsr64(kHyperVMsrHypercall, hvHypercall);
  
  hvHypercall = rdmsr64(kHyperVMsrHypercall);
  DBGLOG("Hypercall MSR new value: 0x%llX", hvHypercall);
  
  //
  // Verify hypercalls are enabled.
  //
  if ((hvHypercall & kHyperVMsrHypercallEnable) == 0) {
    SYSLOG("Hypercalls failed to be enabled!");
    freeHypercallPage();
    return false;
  }
  SYSLOG("Hypercalls are now enabled");
  
  return true;
}

void HyperVVMBusController::destroyHypercalls() {
  UInt64 hvHypercall;
  
  //
  // Disable hypercalls.
  //
  hvHypercall = rdmsr64(kHyperVMsrHypercall);
  wrmsr64(kHyperVMsrHypercall, hvHypercall & kHyperVMsrHypercallRsvdMask);
  freeHypercallPage();
  
  SYSLOG("Hypercalls are now disabled");
}

void HyperVVMBusController::freeHypercallPage() {
  if (hypercallDesc != NULL) {
    hypercallDesc->complete();
    hypercallDesc->release();
    hypercallDesc = NULL;
  }
  
  if (hypercallPage != NULL) {
    vm_deallocate(kernel_map, reinterpret_cast<vm_address_t>(hypercallPage), PAGE_SIZE);
    hypercallPage = NULL;
  }
}

UInt64 HyperVVMBusController::executeHypercallMem(UInt64 value, UInt64 inPhysAddr, UInt64 outPhysAddr) {
  UInt64 status;
  
  asm volatile ("mov %0, %%r8" : : "r" (outPhysAddr): "r8");
  asm volatile ("call *%3" : "=a" (status) : "c" (value), "d" (inPhysAddr), "m" (hypercallPage));
  
  return status;
}

UInt64 HyperVVMBusController::hypercallPostMessage(UInt64 msgAddr) {
  UInt64 status = 0;
  
  //
  // Multiple hypercalls may fail due to lack of resources on the host
  // side, just try again if that happens.
  //
  for (int i = 0; i < kHyperVHypercallRetryCount; i++) {
    status = executeHypercallMem(HYPERCALL_POST_MESSAGE, msgAddr, 0);
    if (status == HYPERCALL_STATUS_SUCCESS) {
      break;
    }
    
    IODelay(5);
  }
  
  if (status) {
    DBGLOG("Failed hypercall status %u", status);
  }
  
  return status;
}

UInt64 HyperVVMBusController::hypercallSignalEvent(UInt64 addr) {
  return executeHypercallMem(HYPERCALL_SIGNAL_EVENT, addr, 0);
}
