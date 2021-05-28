

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

UInt32 HyperVVMBusController::hypercallPostMessage(UInt32 connectionId, HyperVMessageType messageType, void *data, UInt32 size) {
  UInt64 status;
  
  if (size > kHyperVMessageSize) {
    return kHypercallStatusInvalidParameter;
  }

  //
  // Get per-CPU hypercall post message page.
  //
  HyperVDMABuffer *postPageBuffer = &cpuData.perCPUData[cpu_number()].postMessageDma;
  
  HypercallPostMessage *postMessage = (HypercallPostMessage*) postPageBuffer->buffer;
  postMessage->connectionId = connectionId;
  postMessage->reserved = 0;
  postMessage->messageType = messageType;
  postMessage->size = size;
  memcpy(&postMessage->data[0], data, size);
  
  //
  // Perform HvPostMessage hypercall.
  //
  // During hypercall, the calling processor will be suspended until the hypercall returns.
  // Linux disables preemption during this time, but unsure if that is needed due to the above.
  //
  asm volatile ("call *%3" : "=a" (status) : "c" (kHypercallTypePostMessage), "d" (postPageBuffer->physAddr), "m" (hypercallPage));
  return status & kHypercallStatusMask;
}

bool HyperVVMBusController::hypercallSignalEvent(UInt32 connectionId) {
  UInt64 status;
  
  //
  // Perform a fast version of HvSignalEvent hypercall.
  //
  asm volatile ("call *%3" : "=a" (status) : "c" (kHypercallTypeSignalEvent), "d" (connectionId), "m" (hypercallPage));
  return status == kHypercallStatusSuccess;
}
