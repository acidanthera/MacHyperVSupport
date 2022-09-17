//
//  HyperVController.cpp
//  Hyper-V core controller driver
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVController.hpp"
#include <IOKit/IOMapper.h>

#include "HyperVVMBus.hpp"
#include "HyperVInterruptController.hpp"

//
// Hyper-V reported signature.
//
#define kHyperVGuestMinor         (((UInt64) getKernelMinorVersion()) << kHyperVMsrGuestIDMinorVersionShift)
#define kHyperVGuestMajor         (((UInt64) getKernelVersion()) << kHyperVMsrGuestIDMajorVersionShift)
#define kHyperVGuestSignature     (kHyperVGuestMinor | kHyperVGuestMajor)

OSDefineMetaClassAndStructors(HyperVController, super);

//
// Hack to get access to protected setMapperRequired().
//
struct IOMapperDisabler : public IOMapper {
  static void disableMapper() {
    setMapperRequired(false);
  }
};

bool HyperVController::start(IOService *provider) {
  HVCheckDebugArgs();
  
  if (!super::start(provider)) {
    HVSYSLOG("Superclass failed to start");
    return false;
  }
  
  bool result = false;
  do {
    //
    // Verify we are on Hyper-V.
    //
    if (!identifyHyperV()) {
      HVSYSLOG("This system is not Hyper-V, aborting...");
      break;
    }
    
    //
    // Disable I/O mapper.
    // With no PCI bus, the system will stall at waitForSystemMapper().
    //
    getPlatform()->removeProperty(kIOPlatformMapperPresentKey);
    IOMapperDisabler::disableMapper();
    
    //
    // Wait for HyperVPCIRoot to get registered.
    //
    // On certain macOS versions, there must be an IOPCIBridge class with an
    // IOACPIPlatformDevice parent, otherwise the system will panic.
    // This IOPCIBridge class must be loaded first.
    //
    OSDictionary *rootPciMatching = IOService::serviceMatching("HyperVPCIRoot");
    if (rootPciMatching == nullptr) {
      HVSYSLOG("Failed to create HyperVPCIRoot matching dictionary");
      break;
    }
    
    HVDBGLOG("Waiting for HyperVPCIRoot");
#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_6
    IOService *rootPciService = IOService::waitForService(rootPciMatching);
    if (rootPciService != nullptr) {
      rootPciService->retain();
    }
#else
    IOService *rootPciService = waitForMatchingService(rootPciMatching);
    rootPciMatching->release();
#endif
    
    if (rootPciService == nullptr) {
      HVSYSLOG("Failed to locate HyperVPCIRoot");
      break;
    }
    rootPciService->release();
    HVDBGLOG("HyperVPCIRoot is now loaded");
    
    //
    // Setup hypercalls and interrupts.
    //
    if (!initHypercalls()) {
      HVSYSLOG("Failed to initialize hypercalls");
      break;
    }
    if (!initInterrupts()) {
      HVSYSLOG("Failed to initialize interrupts");
      break;
    }
    
    //
    // Initialize VMBus root.
    //
    if (!initVMBus()) {
      HVSYSLOG("Failed to initialize VMBus");
      break;
    }
    
    result = true;
  } while (false);
  
  if (!result) {
    super::stop(provider);
  }
  return result;
}

bool HyperVController::identifyHyperV() {
  bool isHyperV = false;
  uint32_t regs[4];
  
  //
  // Verify we are in fact on Hyper-V.
  //
  do {
    do_cpuid(kHyperVCpuidMaxLeaf, regs);
    _hvMaxLeaf = regs[eax];
    if (_hvMaxLeaf < kHyperVCpuidLeafLimits) {
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
  
  _hvFeatures   = regs[eax];
  _hvPmFeatures = regs[ecx];
  _hvFeatures3  = regs[edx];
  
  //
  // Spec indicates we are supposed to indicate to Hyper-V what OS we are
  // before pulling the Hyper-V identity, but the FreeBSD drivers do this after.
  //
  wrmsr64(kHyperVMsrGuestID, kHyperVGuestSignature);
  HVDBGLOG("Reporting XNU %d.%d guest signature of 0x%llX to Hyper-V", getKernelVersion(), getKernelMinorVersion(), kHyperVGuestSignature);
  
  //
  // Get Hyper-V version.
  //
  do_cpuid(kHyperVCpuidLeafIdentity, regs);
  _hvMajorVersion = regs[ebx] >> 16;
  HVSYSLOG("Starting on Hyper-V %d.%d.%d SP%d",
           _hvMajorVersion, regs[ebx] & 0xFFFF, regs[eax], regs[ecx]);
  
  HVSYSLOG("Hyper-V features: 0x%b", _hvFeatures,
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
  HVSYSLOG("Hyper-V power features: 0x%b (C%u)",
           (_hvPmFeatures & ~CPUPM_HV_CSTATE_MASK),
           "\020"
           "\005C3HPET",      /* HPET is required for C3 state */
           CPUPM_HV_CSTATE(_hvPmFeatures));
  HVSYSLOG("Hyper-V additional features: 0x%b", _hvFeatures3,
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
  _hvRecommends = regs[eax];
  HVDBGLOG("Hyper-V recommendations: 0x%X, max spinlock attempts: 0x%X",
           _hvRecommends, regs[ebx]);
  
  do_cpuid(kHyperVCpuidLeafLimits, regs);
  HVDBGLOG("Hyper-V max virtual CPUs: %u, max logical CPUs: %u, max interrupt vectors: %u",
           regs[eax], regs[ebx], regs[ecx]);
  
  if (_hvMaxLeaf >= kHyperVCpuidLeafHwFeatures) {
    do_cpuid(kHyperVCpuidLeafHwFeatures, regs);
    HVDBGLOG("Hyper-V hardware features: 0x%X", regs[eax]);
  }
  
  return true;
}

bool HyperVController::initVMBus() {
  //
  // Allocate VMBus class.
  //
  _hvVMBus = OSTypeAlloc(HyperVVMBus);
  if (_hvVMBus == nullptr) {
    HVSYSLOG("Failed to allocate HyperVVMBus");
    return false;
  }
  
  //
  // Create dictionary and add interrupt information.
  //
  OSDictionary *dict = OSDictionary::withCapacity(2);
  if (dict == nullptr) {
    _hvVMBus->release();
    return false;
  }
  
  bool result = addInterruptProperties(dict, 0);
  if (!result) {
    dict->release();
    _hvVMBus->release();
    return false;
  }

  //
  // Initialize and attach VMBus class.
  //
  result = _hvVMBus->init(dict) && _hvVMBus->attach(this);
  dict->release();
  
  if (!result) {
    _hvVMBus->release();
    return false;
  }
  
  return true;
}

bool HyperVController::allocateDmaBuffer(HyperVDMABuffer *dmaBuf, size_t size) {
  IOBufferMemoryDescriptor *bufDesc;
  
  //
  // Create page-aligned DMA buffer and get physical address.
  //
  bufDesc = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
                                                             kIODirectionInOut | kIOMemoryPhysicallyContiguous,
                                                             size, 0xFFFFFFFFFFFFF000ULL);
  if (bufDesc == nullptr) {
    HVSYSLOG("Failed to allocate DMA buffer memory of %u bytes", size);
    return false;
  }
  bufDesc->prepare();
  
  dmaBuf->bufDesc  = bufDesc;
  dmaBuf->physAddr = bufDesc->getPhysicalAddress();
  dmaBuf->buffer   = (UInt8*) bufDesc->getBytesNoCopy();
  dmaBuf->size     = size;
  
  memset(dmaBuf->buffer, 0, dmaBuf->size);
  HVDBGLOG("Mapped buffer of %u bytes to 0x%llX", dmaBuf->size, dmaBuf->physAddr);
  return true;
}

void HyperVController::freeDmaBuffer(HyperVDMABuffer *dmaBuf) {
  IOBufferMemoryDescriptor *bufDesc = dmaBuf->bufDesc;

  bzero(dmaBuf, sizeof (*dmaBuf));
  if (bufDesc != nullptr) {
    bufDesc->complete();
    OSSafeReleaseNULL(bufDesc);
  }
}

bool HyperVController::addInterruptProperties(OSDictionary *dict, UInt32 interruptVector) {
  //
  // Create interrupt specifier dictionary.
  // This will be used to reference the Hyper-V interrupt controller service.
  //
  const OSSymbol *interruptControllerName = _hvInterruptController->copyName();
  OSArray *interruptControllers = OSArray::withCapacity(1);
  OSArray *interruptSpecifiers = OSArray::withCapacity(1);
  if (interruptControllerName == nullptr || interruptControllers == nullptr || interruptSpecifiers == nullptr) {
    OSSafeReleaseNULL(interruptControllerName);
    OSSafeReleaseNULL(interruptControllers);
    OSSafeReleaseNULL(interruptSpecifiers);
    return false;
  }
  interruptControllers->setObject(interruptControllerName);
  interruptControllerName->release();
  
  OSData *interruptSpecifierData = OSData::withBytes(&interruptVector, sizeof (interruptVector));
  interruptSpecifiers->setObject(interruptSpecifierData);
  interruptSpecifierData->release();
  
  //
  // Add interrupt information to dictionary.
  //
  bool result = dict->setObject(gIOInterruptControllersKey, interruptControllers) &&
                dict->setObject(gIOInterruptSpecifiersKey, interruptSpecifiers);
  interruptControllers->release();
  interruptSpecifiers->release();
  
  return result;
}
