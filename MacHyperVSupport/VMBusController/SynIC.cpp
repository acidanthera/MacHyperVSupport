//
//  Interrups.cpp
//  MacHyperVServices
//
//  Hyper-V SynIC and interrupt handling
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVVMBusController.hpp"
#include "HyperVVMBusInternal.hpp"

#include <IOKit/IOPlatformExpert.h>

//
// External functions from mp.c
//
extern unsigned int  real_ncpus;    /* real number of cpus */

extern "C" {
  int  cpu_number(void);
  void mp_rendezvous_no_intrs(void (*action_func)(void*), void *arg);
}

extern "C" void initSyncIC(void *cpuData) {
  int cpuIndex = cpu_number();
  HyperVCPUData    *hvCPUData    = (HyperVCPUData*) cpuData;
  HyperVPerCPUData *hvPerCpuData = &hvCPUData->perCPUData[cpuIndex];
  
  //
  // Get processor ID.
  //
  if (hvCPUData->supportsHvVpIndex) {
    hvPerCpuData->virtualCPUID = rdmsr64(MSR_HV_VP_INDEX);
  } else {
    hvPerCpuData->virtualCPUID = 0;
  }
  
  //
  // Configure SynIC message and event flag buffers.
  //
  wrmsr64(MSR_HV_SIMP, MSR_HV_SIMP_ENABLE |
          (rdmsr64(MSR_HV_SIMP) & MSR_HV_SIMP_RSVD_MASK) |
          ((hvPerCpuData->messageDma.physAddr >> PAGE_SHIFT) << MSR_HV_SIMP_PGSHIFT));
  wrmsr64(MSR_HV_SIEFP, MSR_HV_SIEFP_ENABLE |
          (rdmsr64(MSR_HV_SIEFP) & MSR_HV_SIEFP_RSVD_MASK) |
          ((hvPerCpuData->eventFlagsDma.physAddr >> PAGE_SHIFT) << MSR_HV_SIEFP_PGSHIFT));
  
  //
  // Configure SynIC interrupt. We use the thermal interrupt on the LAPIC for this.
  //
  wrmsr64(MSR_HV_SINT0 + VMBUS_SINT_MESSAGE,
          hvCPUData->interruptVector |
          (rdmsr64(MSR_HV_SINT0 + VMBUS_SINT_MESSAGE) & MSR_HV_SINT_RSVD_MASK));
  wrmsr64(MSR_HV_SINT0 + VMBUS_SINT_TIMER,
          hvCPUData->interruptVector |
          (rdmsr64(MSR_HV_SINT0 + VMBUS_SINT_TIMER) & MSR_HV_SINT_RSVD_MASK));
  
  //
  // Enable the SynIC.
  //
  wrmsr64(MSR_HV_SCONTROL, MSR_HV_SCTRL_ENABLE | (rdmsr64(MSR_HV_SCONTROL) & MSR_HV_SCTRL_RSVD_MASK));
}

bool HyperVVMBusController::allocateSynICBuffers() {
  //
  // Allocate per-CPU buffers.
  //
  cpuData.perCPUDataCount = real_ncpus;
  cpuData.perCPUData = (HyperVPerCPUData*) IOMalloc(sizeof (HyperVPerCPUData) * cpuData.perCPUDataCount);
  if (cpuData.perCPUData == NULL) {
    return false;
  }
  
  for (UInt32 i = 0; i < cpuData.perCPUDataCount; i++) {
    if (!allocateDmaBuffer(&cpuData.perCPUData[i].messageDma, PAGE_SIZE)) {
      return false;
    }
    if (!allocateDmaBuffer(&cpuData.perCPUData[i].eventFlagsDma, PAGE_SIZE)) {
      return false;
    }
    
    //
    // Setup message and event interrupts.
    //
    cpuData.perCPUData[i].messages = (HyperVMessage*) cpuData.perCPUData[i].messageDma.buffer;
    cpuData.perCPUData[i].eventFlags = (HyperVEventFlags*) cpuData.perCPUData[i].eventFlagsDma.buffer;
    cpuData.perCPUData[i].synProc = SynICProcessor::syncICProcessor(i, this);
    cpuData.perCPUData[i].synProc->setupInterrupt();
  }
  
  return true;
}

bool HyperVVMBusController::initSynIC() {
  bool   foundVector       = false;
  bool   interruptsStarted = false;
  UInt32 vector            = 0;
  
  //
  // Get vector for our interrupt.
  //
  auto matching = serviceMatching("AppleAPICInterruptController");
  if (matching != NULL) {
    IOService *apicService = waitForMatchingService(matching);
    if (apicService != NULL) {
      auto vectorBase = OSDynamicCast(OSNumber, apicService->getProperty("Base Vector Number"));
      if (vectorBase != NULL) {
        vector = vectorBase->unsigned32BitValue();
      }
      apicService->release();
    }
    matching->release();
  }
  
  auto intArray = OSDynamicCast(OSArray, getProvider()->getProperty("IOInterruptSpecifiers"));
  if (intArray != NULL) {
    auto intData = OSDynamicCast(OSData, intArray->getObject(0));
    if (intData != NULL) {
      vector += *((UInt32*)intData->getBytesNoCopy());
      foundVector = true;
    }
  }
  
  if (!foundVector) {
    SYSLOG("Failed to get VMBus device interrupt vector");
    return false;
  }
  cpuData.interruptVector = vector;
  DBGLOG("VMBus device interrupt vector: 0x%X", cpuData.interruptVector);

  //
  // Setup workloop and command gate.
  // This helps with message locking.
  //
  workloop = IOWorkLoop::workLoop();
  if (workloop == NULL) {
    return false;
  }
  cmdGate = IOCommandGate::commandGate(this);
  if (cmdGate == NULL) {
    return false;
  }
  workloop->addEventSource(cmdGate);
  
  //
  // Get PM callbacks.
  //
  pmKextRegister(PM_DISPATCH_VERSION, NULL, &pmCallbacks);
  if (pmCallbacks.LCPUtoProcessor == NULL || pmCallbacks.ThreadBind == NULL ) {
    SYSLOG("PM callbacks are invalid");
    return false;
  }
  preemptionLock = IOSimpleLockAlloc();
  if (preemptionLock == NULL) {
    return false;
  }
  
  //
  // Allocate buffers for SynIC.
  //
  if (!allocateSynICBuffers()) {
    return false;
  }
  
  //
  // We use multiple interrupt handlers, a direct interrupt for the SynIC,
  // and multiple workloop interrupts triggered by the direct interrupt handler.
  //
  do {
    if (getProvider()->registerInterrupt(0, this, OSMemberFunctionCast(IOInterruptAction, this, &HyperVVMBusController::handleSynICInterrupt)) != kIOReturnSuccess ||
        getProvider()->enableInterrupt(0) != kIOReturnSuccess) {
      break;
    }
    
    interruptsStarted = true;
  } while (false);
  
  if (!interruptsStarted) {
    //OSSafeReleaseNULL(interruptMessageSource);
    OSSafeReleaseNULL(workloop);
    SYSLOG("Failed to setup interrupt handlers");
    return false;
  }

  //
  // Setup SynIC on all processors.
  //
  mp_rendezvous_no_intrs(initSyncIC, &cpuData);
  
  //
  // Register ourselves as an interrupt controller.
  // We are the interrupt controller for child devices.
  //
  interruptControllerName = OSSymbol::withCString(kHyperVVMBusInterruptControllerName);
  
  vectors = (IOInterruptVector*)IOMalloc(128 * sizeof (IOInterruptVector));
  memset(vectors, 0, 128 * sizeof (IOInterruptVector));
  
  for (int i = 0; i < 128; i++) {
    vectors[i].interruptLock = IOLockAlloc();
  }
  
  getPlatform()->registerInterruptController((OSSymbol *)interruptControllerName, this);
  return true;
}

void HyperVVMBusController::sendSynICEOM(UInt32 cpu) {
  if (!IOSimpleLockTryLock(preemptionLock)) {
    SYSLOG("Failed to disable preemption");
    return;
  }
  
  bool intsEnabled = ml_set_interrupts_enabled(false);
  
  do {
    processor_t proc = pmCallbacks.LCPUtoProcessor(cpu);
    if (proc == NULL) {
      break;
    }
    
    if (cpu != cpu_number()) {
      DBGLOG("Changed from cpu %u to %u", cpu_number(), cpu);
    }
    pmCallbacks.ThreadBind(proc);

    
  } while (false);
  
  IOSimpleLockUnlock(preemptionLock);
  ml_set_interrupts_enabled(intsEnabled);
  
  //
  // We should now be on the correct CPU, so send EOM.
  //
  wrmsr64(MSR_HV_EOM, 0);
}



void HyperVVMBusController::handleSynICInterrupt(OSObject *target, void *refCon, IOService *nub, int source) {
  UInt32 cpuIndex = cpu_number();
  HyperVMessage *message;

  //
  // Handle timer messages.
  //
  message = &cpuData.perCPUData[cpuIndex].messages[VMBUS_SINT_TIMER];
  if (message->type == HYPERV_MSGTYPE_TIMER_EXPIRED) {
    message->type = HYPERV_MSGTYPE_NONE;
   cpuData.perCPUData[cpuIndex].synProc->triggerInterrupt();
  }
  
  //
  // Handle channel event flags.
  //
  for (UInt32 i = 1; i <= vmbusChannelHighest; i++) {
    if ((cpuData.perCPUData[cpuIndex].eventFlags[VMBUS_SINT_MESSAGE].flags[VMBUS_CHANNEL_EVENT_INDEX(i)] & VMBUS_CHANNEL_EVENT_MASK(i)) == 0 ||
        vmbusChannels[i].status != kVMBusChannelStatusOpen) {
      continue;
    }
    
    //
    // Clear event flag and trigger handler.
    //
    cpuData.perCPUData[cpuIndex].eventFlags[VMBUS_SINT_MESSAGE].flags[VMBUS_CHANNEL_EVENT_INDEX(i)] &= ~VMBUS_CHANNEL_EVENT_MASK(i);
    if (vectors[i].handler != NULL) {
      vectors[i].handler(vectors[i].target, vectors[i].refCon, vectors[i].nub, vectors[i].source);
    }
  }
  
  //
  // Handle normal messages.
  //
  message = &cpuData.perCPUData[cpuIndex].messages[VMBUS_SINT_MESSAGE];
  if (message->type != HYPERV_MSGTYPE_NONE) {
    cpuData.perCPUData[cpuIndex].synProc->triggerInterrupt();
  }
}

IOWorkLoop* HyperVVMBusController::getSynICWorkLoop() {
  return workloop;
}
