//
//  SynIC.cpp
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
    hvPerCpuData->virtualCPUID = rdmsr64(kHyperVMsrVPIndex);
  } else {
    hvPerCpuData->virtualCPUID = 0;
  }
  
  //
  // Configure SynIC message and event flag buffers.
  //
  wrmsr64(kHyperVMsrSimp, kHyperVMsrSimpEnable |
          (rdmsr64(kHyperVMsrSimp) & kHyperVMsrSimpRsvdMask) |
          ((hvPerCpuData->messageDma.physAddr >> PAGE_SHIFT) << kHyperVMsrSimpPageShift));
  wrmsr64(kHyperVMsrSiefp, kHyperVMsrSiefpEnable |
          (rdmsr64(kHyperVMsrSiefp) & kHyperVMsrSiefpRsvdMask) |
          ((hvPerCpuData->eventFlagsDma.physAddr >> PAGE_SHIFT) << kHyperVMsrSiefpPageShift));
  
  //
  // Configure SynIC interrupt using the interrupt specified in ACPI for the VMBus device.
  //
  wrmsr64(kHyperVMsrSInt0 + kVMBusInterruptMessage,
          hvCPUData->interruptVector |
          (rdmsr64(kHyperVMsrSInt0 + kVMBusInterruptMessage) & kHyperVMsrSIntRsvdMask));
  wrmsr64(kHyperVMsrSInt0 + kVMBusInterruptTimer,
          hvCPUData->interruptVector |
          (rdmsr64(kHyperVMsrSInt0 + kVMBusInterruptTimer) & kHyperVMsrSIntRsvdMask));
  
  //
  // Enable the SynIC.
  //
  wrmsr64(kHyperVMsrSyncICControl, kHyperVMsrSyncICControlEnable | (rdmsr64(kHyperVMsrSyncICControl) & kHyperVMsrSyncICControlRsvdMask));
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
    if (!allocateDmaBuffer(&cpuData.perCPUData[i].postMessageDma, sizeof (HypercallPostMessage))) {
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
  //
  // Only EOM if there is a pending message.
  //
  cpuData.perCPUData[cpu].messages[kVMBusInterruptMessage].type = kHyperVMessageTypeNone;
  if (!cpuData.perCPUData[cpu].messages[kVMBusInterruptMessage].flags.messagePending) {
    return;
  }
  
  //
  // Change to desired CPU if needed.
  //
  if (cpu != cpu_number()) {
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
      
      pmCallbacks.ThreadBind(proc);
    } while (false);
    
    IOSimpleLockUnlock(preemptionLock);
    ml_set_interrupts_enabled(intsEnabled);
    DBGLOG("Changed to cpu %u", cpu_number());
  }
  
  //
  // We should now be on the correct CPU, so send EOM.
  //
  wrmsr64(kHyperVMsrEom, 0);
}



void HyperVVMBusController::handleSynICInterrupt(OSObject *target, void *refCon, IOService *nub, int source) {
  UInt32 cpuIndex = cpu_number();
  HyperVMessage *message;

  //
  // Handle timer messages.
  //
  message = &cpuData.perCPUData[cpuIndex].messages[kVMBusInterruptTimer];
  if (message->type == kHyperVMessageTypeTimerExpired) {
    message->type = kHyperVMessageTypeNone;
    
    if (message->flags.messagePending) {
      wrmsr64(kHyperVMsrEom, 0);
    }
   //cpuData.perCPUData[cpuIndex].synProc->triggerInterrupt();
  }
  
  //
  // Handle channel event flags.
  //
  for (UInt32 i = 1; i <= vmbusChannelHighest; i++) {
    if ((cpuData.perCPUData[cpuIndex].eventFlags[kVMBusInterruptMessage].flags[VMBUS_CHANNEL_EVENT_INDEX(i)] & VMBUS_CHANNEL_EVENT_MASK(i)) == 0 ||
        vmbusChannels[i].status != kVMBusChannelStatusOpen) {
      continue;
    }
    
    //
    // Clear event flag and trigger handler.
    //
    cpuData.perCPUData[cpuIndex].eventFlags[kVMBusInterruptMessage].flags[VMBUS_CHANNEL_EVENT_INDEX(i)] &= ~VMBUS_CHANNEL_EVENT_MASK(i);
    if (vectors[i].handler != NULL) {
      vectors[i].handler(vectors[i].target, vectors[i].refCon, vectors[i].nub, vectors[i].source);
    }
  }
  
  //
  // Handle normal messages.
  //
  message = &cpuData.perCPUData[cpuIndex].messages[kVMBusInterruptMessage];
  if (message->type != kHyperVMessageTypeNone) {
    cpuData.perCPUData[cpuIndex].synProc->triggerInterrupt();
  }
}

IOWorkLoop* HyperVVMBusController::getSynICWorkLoop() {
  return workloop;
}
