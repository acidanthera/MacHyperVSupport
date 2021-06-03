//
//  HyperVVMBus.cpp
//  Hyper-V VMBus controller
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVVMBusController.hpp"
#include "HyperVVMBusInternal.hpp"

#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOMapper.h>

//
// Hyper-V reported signature.
//
#define kHyperVGuestMinor         (((UInt64) getKernelMinorVersion()) << kHyperVMsrGuestIDMinorVersionShift)
#define kHyperVGuestMajor         (((UInt64) getKernelVersion()) << kHyperVMsrGuestIDMajorVersionShift)
#define kHyperVGuestSignature     (kHyperVGuestMinor | kHyperVGuestMajor)

OSDefineMetaClassAndStructors(HyperVVMBusController, super);

//
// Hack to get access to protected setMapperRequired().
//
struct IOMapperDisabler : public IOMapper {
  static void disableMapper() {
    setMapperRequired(false);
  }
};

bool HyperVVMBusController::identifyHyperV() {
  bool isHyperV = false;
  UInt32 regs[4];
  
  //
  // Verify we are in fact on Hyper-V.
  //
  do {
    do_cpuid(kHyperVCpuidMaxLeaf, regs);
    hvMaxLeaf = regs[eax];
    if (hvMaxLeaf < kHyperVCpuidLeafLimits) {
      break;
    }
    
    do_cpuid(kHyperVCpuidLeafInterface, regs);
    if (regs[eax] != kHyperVCpuidLeafInterfaceSig) {
      break;
    }
    
    do_cpuid(kHyperVCpuidLeafFeatures, regs);
    if ((regs[eax] & kHyperVCpuidMsrHypercall) == 0) {
      break;
    }
    
    isHyperV = true;
  } while (false);
  
  if (!isHyperV) {
    return false;
  }
  
  hvFeatures   = regs[eax];
  hvPmFeatures = regs[ecx];
  hvFeatures3  = regs[edx];
  
  //
  // Spec indicates we are supposed to indicate to Hyper-V what OS we are
  // before pulling the Hyper-V identity, but the FreeBSD drivers do this after.
  //
  wrmsr64(kHyperVMsrGuestID, kHyperVGuestSignature);
  DBGLOG("Reporting XNU %d.%d guest signature of 0x%llX to Hyper-V", getKernelVersion(), getKernelMinorVersion(), kHyperVGuestSignature);
  
  //
  // Get Hyper-V version.
  //
  do_cpuid(kHyperVCpuidLeafIdentity, regs);
  hvMajorVersion = regs[ebx] >> 16;
  SYSLOG("Starting on Hyper-V %d.%d.%d SP%d",
         hvMajorVersion, regs[ebx] & 0xFFFF, regs[eax], regs[ecx]);
  
  SYSLOG("Hyper-V features: 0x%b", hvFeatures,
         "\020"
         "\001VPRUNTIME"    /* MSR_HV_VP_RUNTIME */
         "\002TMREFCNT"     /* MSR_HV_TIME_REF_COUNT */
         "\003SYNIC"        /* MSRs for SynIC */
         "\004SYNTM"        /* MSRs for SynTimer */
         "\005APIC"         /* MSR_HV_{EOI,ICR,TPR} */
         "\006HYPERCALL"    /* MSR_HV_{GUEST_OS_ID,HYPERCALL} */
         "\007VPINDEX"      /* MSR_HV_VP_INDEX */
         "\010RESET"        /* MSR_HV_RESET */
         "\011STATS"        /* MSR_HV_STATS_ */
         "\012REFTSC"       /* MSR_HV_REFERENCE_TSC */
         "\013IDLE"         /* MSR_HV_GUEST_IDLE */
         "\014TMFREQ"       /* MSR_HV_{TSC,APIC}_FREQUENCY */
         "\015DEBUG");      /* MSR_HV_SYNTH_DEBUG_ */
  SYSLOG("Hyper-V power features: 0x%b (C%u)",
         (hvPmFeatures & ~CPUPM_HV_CSTATE_MASK),
         "\020"
         "\005C3HPET",      /* HPET is required for C3 state */
         CPUPM_HV_CSTATE(hvPmFeatures));
  SYSLOG("Hyper-V additional features: 0x%b", hvFeatures3,
         "\020"
         "\001MWAIT"        /* MWAIT */
         "\002DEBUG"        /* guest debug support */
         "\003PERFMON"      /* performance monitor */
         "\004PCPUDPE"      /* physical CPU dynamic partition event */
         "\005XMMHC"        /* hypercall input through XMM regs */
         "\006IDLE"         /* guest idle support */
         "\007SLEEP"        /* hypervisor sleep support */
         "\010NUMA"         /* NUMA distance query support */
         "\011TMFREQ"       /* timer frequency query (TSC, LAPIC) */
         "\012SYNCMC"       /* inject synthetic machine checks */
         "\013CRASH"        /* MSRs for guest crash */
         "\014DEBUGMSR"     /* MSRs for guest debug */
         "\015NPIEP"        /* NPIEP */
         "\016HVDIS");      /* disabling hypervisor */
  
  do_cpuid(kHyperVCpuidLeafRecommends, regs);
  hvRecommends = regs[eax];
  DBGLOG("Hyper-V recommendations: 0x%X, max spinlock attempts: 0x%X",
         hvRecommends, regs[ebx]);
  
  do_cpuid(kHyperVCpuidLeafLimits, regs);
  DBGLOG("Hyper-V max virtual CPUs: %u, max logical CPUs: %u, max interrupt vectors: %u",
         regs[eax], regs[ebx], regs[ecx]);
  
  if (hvMaxLeaf >= kHyperVCpuidLeafHwFeatures) {
    do_cpuid(kHyperVCpuidLeafHwFeatures, regs);
    DBGLOG("Hyper-V hardware features: 0x%X", regs[eax]);
  }
  
  return true;
}

bool HyperVVMBusController::allocateDmaBuffer(HyperVDMABuffer *dmaBuf, size_t size) {
  IOBufferMemoryDescriptor  *bufDesc;
  
  //
  // Create DMA buffer with required specifications and get physical address.
  //
  bufDesc = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
                                                             kIODirectionInOut | kIOMemoryPhysicallyContiguous | kIOMapInhibitCache | kIOMemoryMapperNone,
                                                             size, 0xFFFFFFFFFFFFF000ULL);
  if (bufDesc == NULL) {
    SYSLOG("Failed to allocate DMA buffer memory of %u bytes", size);
    return false;
  }
  bufDesc->prepare();
  
  dmaBuf->bufDesc  = bufDesc;
  dmaBuf->physAddr = bufDesc->getPhysicalAddress();
  dmaBuf->buffer   = bufDesc->getBytesNoCopy();
  dmaBuf->size     = size;
  
  memset(dmaBuf->buffer, 0, dmaBuf->size);
  DBGLOG("Mapped buffer of %u bytes to 0x%llX", dmaBuf->size, dmaBuf->physAddr);
  return true;
}

void HyperVVMBusController::freeDmaBuffer(HyperVDMABuffer *dmaBuf) {
  dmaBuf->bufDesc->complete();
  dmaBuf->bufDesc->release();
  
  memset(dmaBuf, 0, sizeof (*dmaBuf));
}

bool HyperVVMBusController::start(IOService *provider) {
  if (!super::start(provider)) {
    return false;
  }
  
  //
  // Verify we are on Hyper-V.
  //
  if (!identifyHyperV()) {
    SYSLOG("This system is not Hyper-V, aborting...");
    super::stop(provider);
    return false;
  }
  
  //
  // Wait for HyperVPCIRoot to get registered.
  //
  // On certain macOS versions, there must be an IOPCIBridge class with an
  // IOACPIPlatformDevice parent, otherwise the system will panic.
  // This IOPCIBridge class must be loaded first.
  //
  OSDictionary *pciMatching = IOService::serviceMatching("HyperVPCIRoot");
  if (pciMatching == NULL) {
    SYSLOG("Failed to create HyperVPCIRoot matching dictionary");
    super::stop(provider);
    return false;
  }
  
  DBGLOG("Waiting for HyperVPCIRoot");
  IOService *pciService = waitForMatchingService(pciMatching);
  pciMatching->release();
  if (pciService == NULL) {
    SYSLOG("Failed to locate HyperVPCIRoot");
    super::stop(provider);
    return false;
  }
  pciService->release();
  DBGLOG("HyperVPCIRoot is now loaded");
  
  //
  // Disable I/O mapper.
  // With no PCI bus, the system will stall at waitForSystemMapper().
  //
  getPlatform()->removeProperty(kIOPlatformMapperPresentKey);
  IOMapperDisabler::disableMapper();
  
  //
  // Setup hypercalls.
  //
  initHypercalls();
  
  initSynIC();
      
  
  allocateVMBusBuffers();
  
  cmdGate = IOCommandGate::commandGate(this);
  workloop->addEventSource(cmdGate);
  
  //cmdGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBus::connectVMBus));
  
  connectVMBus();
  scanVMBus();

  
  return true;
}
