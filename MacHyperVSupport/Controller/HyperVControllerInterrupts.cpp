//
//  HyperVControllerInterrupts.cpp
//  Hyper-V SynIC and interrupt handling
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#include "HyperVController.hpp"
#include "HyperVInterruptController.hpp"
#include "VMBus.hpp"

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_6
//
// PM versions.
//
// Versions 10.7.0 and above are all the same 102.
//
#define kPMDispatchVersion10_6_0    17
#define kPMDispatchVersion10_6_1    17
#define kPMDispatchVersion10_6_2    18
#define kPMDispatchVersion10_6_3    19
#define kPMDispatchVersion10_6_4    20
#define kPMDispatchVersion10_6_5    21
#define kPMDispatchVersion10_6_6    21
#define kPMDispatchVersion10_6_7    21
#define kPMDispatchVersion10_6_8    23

static UInt32 pmVersionsSnowLeopard[] = {
  kPMDispatchVersion10_6_0,
  kPMDispatchVersion10_6_1,
  kPMDispatchVersion10_6_2,
  kPMDispatchVersion10_6_3,
  kPMDispatchVersion10_6_4,
  kPMDispatchVersion10_6_5,
  kPMDispatchVersion10_6_6,
  kPMDispatchVersion10_6_7,
  kPMDispatchVersion10_6_8
};
#endif

//
// External functions from mp.c
//
extern "C" {
  void mp_rendezvous_no_intrs(void (*action_func)(void*), void *arg);
}

extern "C" void initCPUSyncIC(void *cpuData) {
  HyperVCPUData *hvCPUData = &(static_cast<HyperVCPUData*>(cpuData)[cpu_number()]);
  
  //
  // Get processor ID.
  //
  if (*hvCPUData->supportsHvVpIndex) {
    hvCPUData->virtualCPUIndex = rdmsr64(kHyperVMsrVPIndex);
  } else {
    hvCPUData->virtualCPUIndex = 0;
  }
  
  //
  // Configure SynIC message and event flag buffers.
  //
  wrmsr64(kHyperVMsrSimp, kHyperVMsrSimpEnable |
          (rdmsr64(kHyperVMsrSimp) & kHyperVMsrSimpRsvdMask) |
          ((hvCPUData->messageDma.physAddr >> PAGE_SHIFT) << kHyperVMsrSimpPageShift));
  wrmsr64(kHyperVMsrSiefp, kHyperVMsrSiefpEnable |
          (rdmsr64(kHyperVMsrSiefp) & kHyperVMsrSiefpRsvdMask) |
          ((hvCPUData->eventFlagsDma.physAddr >> PAGE_SHIFT) << kHyperVMsrSiefpPageShift));
  
  //
  // Configure SynIC interrupt using the interrupt specified in ACPI for the VMBus device.
  //
  wrmsr64(kHyperVMsrSInt0 + kVMBusInterruptMessage,
          *hvCPUData->interruptVector |
          (rdmsr64(kHyperVMsrSInt0 + kVMBusInterruptMessage) & kHyperVMsrSIntRsvdMask));
  wrmsr64(kHyperVMsrSInt0 + kVMBusInterruptTimer,
          *hvCPUData->interruptVector |
          (rdmsr64(kHyperVMsrSInt0 + kVMBusInterruptTimer) & kHyperVMsrSIntRsvdMask));
  
  //
  // Enable the SynIC.
  //
  wrmsr64(kHyperVMsrSyncICControl, kHyperVMsrSyncICControlEnable | (rdmsr64(kHyperVMsrSyncICControl) & kHyperVMsrSyncICControlRsvdMask));
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_6
extern "C" void doAllCpuSyncICEOM(void *cpu) {
  int currentCpuIndex  = cpu_number();
  int *desiredCpuIndex = (int*)cpu;
  
  if (currentCpuIndex == *desiredCpuIndex) {
    wrmsr64(kHyperVMsrEom, 0);
  }
}
#endif

bool HyperVController::allocateInterruptBuffers() {
  //
  // Allocate per-CPU buffers.
  //
  _cpuDataCount = real_ncpus;
  _cpuData      = IONew(HyperVCPUData, _cpuDataCount);
  if (_cpuData == nullptr) {
    return false;
  }
  bzero(_cpuData, sizeof (HyperVCPUData) * _cpuDataCount);
  
  for (UInt32 i = 0; i < _cpuDataCount; i++) {
    if (!allocateDmaBuffer(&_cpuData[i].messageDma, PAGE_SIZE)) {
      return false;
    }
    if (!allocateDmaBuffer(&_cpuData[i].eventFlagsDma, PAGE_SIZE)) {
      return false;
    }
    if (!allocateDmaBuffer(&_cpuData[i].postMessageDma, sizeof (HypercallPostMessage))) {
      return false;
    }
    
    //
    // Setup message and event interrupts.
    //
    _cpuData[i].interruptVector   = &_interruptVector;
    _cpuData[i].supportsHvVpIndex = &_supportsHvVpIndex;
    _cpuData[i].messages          = (HyperVMessage*)    _cpuData[i].messageDma.buffer;
    _cpuData[i].eventFlags        = (HyperVEventFlags*) _cpuData[i].eventFlagsDma.buffer;
    HVDBGLOG("Allocated data for CPU %u", i);
  }
  
  return true;
}

bool HyperVController::initInterrupts() {
  bool   foundVector       = false;
  UInt32 vector            = 0;
  
  //
  // Get vector for our interrupt.
  //
  HVDBGLOG("Waiting for AppleAPICInterruptController");
  auto apicMatching = serviceMatching("AppleAPICInterruptController");
  if (apicMatching == nullptr) {
    HVSYSLOG("Failed to create AppleAPICInterruptController matching dictionary");
    return false;
  }
  
#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_6
  IOService *apicService = IOService::waitForService(apicMatching);
  if (apicService != nullptr) {
    apicService->retain();
  }
#else
  IOService *apicService = waitForMatchingService(apicMatching);
  apicMatching->release();
#endif
  
  if (apicService == nullptr) {
    HVSYSLOG("Failed to locate AppleAPICInterruptController");
    return false;
  }

  auto vectorBase = OSDynamicCast(OSNumber, apicService->getProperty("Base Vector Number"));
  if (vectorBase != nullptr) {
    vector = vectorBase->unsigned32BitValue();
  }
  apicService->release();
  
  auto intArray = OSDynamicCast(OSArray, getProvider()->getProperty("IOInterruptSpecifiers"));
  if (intArray != nullptr) {
    auto intData = OSDynamicCast(OSData, intArray->getObject(0));
    if (intData != nullptr) {
      vector += *((UInt32*)intData->getBytesNoCopy());
      foundVector = true;
    }
  }
  
  if (!foundVector) {
    HVSYSLOG("Failed to get VMBus device interrupt vector");
    return false;
  }
  _interruptVector = vector;
  HVDBGLOG("VMBus device interrupt vector: 0x%X", _interruptVector);
  
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_6
  //
  // Get PM callbacks.
  //
  UInt32 pmVersion = PM_DISPATCH_VERSION;
  if (getKernelVersion() == KernelVersion::SnowLeopard) {
    if (getKernelMinorVersion() < arrsize(pmVersionsSnowLeopard)) {
      pmVersion = pmVersionsSnowLeopard[getKernelMinorVersion()];
    }
  }
  
  pmKextRegister(pmVersion, NULL, &_pmCallbacks);
  if (_pmCallbacks.LCPUtoProcessor == nullptr || _pmCallbacks.ThreadBind == nullptr) {
    HVSYSLOG("PM callbacks are invalid");
    return false;
  }
  _preemptionLock = IOSimpleLockAlloc();
  if (_preemptionLock == nullptr) {
    return false;
  }
#endif
  
  //
  // Allocate buffers for interrupts.
  //
  if (!allocateInterruptBuffers()) {
    return false;
  }
  
  //
  // Initialize interrupt controller that incoming interrupts will be directed to.
  //
  // Interrupt 0 is used for the VMBus controller.
  // All other interrupts are used for VMBus children devices.
  //
  _hvInterruptController = OSTypeAlloc(HyperVInterruptController);
  if (!_hvInterruptController->init(kVMBusMaxChannels)) {
    HVSYSLOG("Failed to initialize interrupt controller");
    return false;
  }
  
  //
  // Setup direct interrupt handler.
  // Hyper-V triggers a single "hardware" interrupt for all events.
  //
  if (getProvider()->registerInterrupt(0, this, OSMemberFunctionCast(IOInterruptAction, this, &HyperVController::handleInterrupt)) != kIOReturnSuccess) {
    HVSYSLOG("Failed to setup interrupt handler");
    return false;
  }
  
  //
  // Setup SynIC interrupts on all processors.
  //
  mp_rendezvous_no_intrs(initCPUSyncIC, _cpuData);
  return true;
}

void HyperVController::handleInterrupt(OSObject *target, void *refCon, IOService *nub, int source) {
  UInt32 cpuIndex = cpu_number();
  HyperVMessage *message;

  //
  // Handle timer messages.
  //
  message = getPendingMessage(cpuIndex, kVMBusInterruptTimer);
  if (message->type == kHyperVMessageTypeTimerExpired) {
    message->type = kHyperVMessageTypeNone;
    
    if (message->flags.messagePending) {
      wrmsr64(kHyperVMsrEom, 0);
    }
  }
  
  //
  // Handle VMBus channel events.
  //
  //
  // On Windows Server 2008 R2 and older, both the global event flags and the RX event flags need to be checked.
  // On Windows 8/Server 2012 and newer, each channel has its own bit in the global event flags.
  //
  if (_useLegacyEventFlags && sync_test_and_clear_bit(0, _cpuData[cpuIndex].eventFlags[kVMBusInterruptMessage].flags32)) {
    //
    // Check each channel for pending interrupt and invoke handler.
    //
    for (UInt32 i = 1; i < kVMBusMaxChannels; i++) {
      if (sync_test_and_clear_bit(i, _vmbusRxEventFlags->flags32)) {
        _hvInterruptController->handleInterrupt(nullptr, nullptr, i);
      }
    }
  } else {
    //
    // Check each channel for pending interrupt and invoke handler.
    //
    for (UInt32 i = 1; i < kVMBusMaxChannels; i++) {
      if (sync_test_and_clear_bit(i, _cpuData[cpuIndex].eventFlags[kVMBusInterruptMessage].flags32)) {
        _hvInterruptController->handleInterrupt(nullptr, nullptr, i);
      }
    }
  }
  
  //
  // Handle VMBus management messages.
  //
  message = getPendingMessage(cpuIndex, kVMBusInterruptMessage);
  if (message->type != kHyperVMessageTypeNone) {
    _hvInterruptController->handleInterrupt(nullptr, nullptr, 0);
  }
}

bool HyperVController::enableInterrupts(HyperVEventFlags *legacyEventFlags) {
  disableInterrupts();
  
  _vmbusRxEventFlags   = legacyEventFlags;
  _useLegacyEventFlags = _vmbusRxEventFlags != nullptr;
  
  //
  // Store VMBus event information and enable interrupts.
  //
  return getProvider()->enableInterrupt(0) == kIOReturnSuccess;
}

void HyperVController::disableInterrupts() {
  //
  // Disable interrupts and clear VMBus event information.
  //
  getProvider()->disableInterrupt(0);
  _useLegacyEventFlags = false;
  _vmbusRxEventFlags   = nullptr;
}

void HyperVController::sendSynICEOM(UInt32 cpu) {
  //
  // Only EOM if there is a pending message.
  //
  _cpuData[cpu].messages[kVMBusInterruptMessage].type = kHyperVMessageTypeNone;
  if (!_cpuData[cpu].messages[kVMBusInterruptMessage].flags.messagePending) {
    return;
  }
  
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_6
  //
  // Change to desired CPU if needed.
  //
  if (cpu != cpu_number()) {
    if (!IOSimpleLockTryLock(_preemptionLock)) {
      HVSYSLOG("Failed to disable preemption");
      return;
    }
    
    bool intsEnabled = ml_set_interrupts_enabled(false);
    
    do {
      processor_t proc = _pmCallbacks.LCPUtoProcessor(cpu);
      if (proc == nullptr) {
        break;
      }
      
      _pmCallbacks.ThreadBind(proc);
    } while (false);
    
    IOSimpleLockUnlock(_preemptionLock);
    ml_set_interrupts_enabled(intsEnabled);
    HVDBGLOG("Changed to cpu %u", cpu_number());
  }
  
  //
  // We should now be on the correct CPU, so send EOM.
  //
  wrmsr64(kHyperVMsrEom, 0);
#else
  //
  // PM kext API does not exist on 10.4, call function on all CPUs.
  //
  mp_rendezvous_no_intrs(doAllCpuSyncICEOM, &cpu);
#endif
}
