//
//  HyperVController.hpp
//  Hyper-V core controller driver
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVController_hpp
#define HyperVController_hpp

#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOService.h>

#include "HyperV.hpp"

extern "C" {
#include <i386/cpuid.h>
#include <i386/proc_reg.h>
#include <i386/pmCPU.h>
}

typedef struct {
  UInt32                  *interruptVector;
  bool                    *supportsHvVpIndex;
  
  UInt64                  interruptCounter;
  UInt64                  virtualCPUIndex;
  
  HyperVDMABuffer         messageDma;
  HyperVDMABuffer         eventFlagsDma;
  
  HyperVMessage           *messages;
  volatile HyperVEventFlags        *eventFlags; //TODO: testing
  
  HyperVDMABuffer         postMessageDma;
} HyperVCPUData;

class HyperVInterruptController;
class HyperVVMBus;
class HyperVUserClient;

class HyperVController : public IOService {
  OSDeclareDefaultStructors(HyperVController);
  HVDeclareLogFunctions("ctrl");
  typedef IOService super;
  
private:
  //
  // Hyper-V reported features.
  //
  UInt32 _hvMaxLeaf      = 0;
  UInt32 _hvFeatures     = 0;
  UInt32 _hvPmFeatures   = 0;
  UInt32 _hvFeatures3    = 0;
  UInt16 _hvMajorVersion = 0;
  UInt32 _hvRecommends   = 0;
  
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_6
  pmCallBacks_t _pmCallbacks     = { };
  IOSimpleLock  *_preemptionLock = nullptr;
#endif
  
  //
  // Hypercall page.
  //
  void                *hypercallPage = nullptr;
  IOMemoryDescriptor  *hypercallDesc = nullptr;
  
  //
  // Interrupt and event data.
  //
  UInt32            _cpuDataCount        = 0;
  HyperVCPUData     *_cpuData            = nullptr;
  UInt32            _interruptVector     = 0;
  bool              _supportsHvVpIndex   = false;
  bool              _useLegacyEventFlags = false;
  HyperVEventFlags  *_vmbusRxEventFlags  = nullptr;
  
  HyperVInterruptController *_hvInterruptController = nullptr;
  HyperVVMBus               *_hvVMBus               = nullptr;
  HyperVUserClient          *_userClientInstance    = nullptr;
  
  //
  // Misc functions.
  //
  bool identifyHyperV();
  bool initVMBus();
  
  //
  // Hypercalls/interrupts.
  //
  bool initHypercalls();
  void destroyHypercalls();
  void freeHypercallPage();
  bool allocateInterruptBuffers();
  bool initInterrupts();
  void destroySynIC();
  void handleInterrupt(OSObject *target, void *refCon, IOService *nub, int source);
  
public:
  //
  // IOService functions.
  //
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  
  //
  // Misc functions.
  //
  bool allocateDmaBuffer(HyperVDMABuffer *dmaBuf, size_t size);
  void freeDmaBuffer(HyperVDMABuffer *dmaBuf);
  bool addInterruptProperties(OSDictionary *dict, UInt32 interruptVector);
  
  //
  // Hypercalls/interrupts.
  //
  UInt32 hypercallPostMessage(UInt32 connectionId, HyperVMessageType messageType, void *data, UInt32 size);
  bool hypercallSignalEvent(UInt32 connectionId);
  bool enableInterrupts(HyperVEventFlags *legacyEventFlags = nullptr);
  void disableInterrupts();
  void sendSynICEOM(UInt32 cpu);
  
  //
  // Time reference counter.
  //
  inline bool isTimeRefCounterSupported() { return (_hvFeatures & kHyperVCpuidMsrTimeRefCnt); }
  inline UInt64 readTimeRefCounter() { return isTimeRefCounterSupported() ? rdmsr64(kHyperVMsrTimeRefCount) : 0; }

  //
  // Messages.
  //
  inline HyperVMessage* getPendingMessage(UInt32 cpuIndex, UInt32 messageIndex) {
    return &_cpuData[cpuIndex].messages[messageIndex];
  }
  inline void clearPendingMessage(UInt32 cpuIndex, UInt32 messageIndex) {
    _cpuData[cpuIndex].messages[messageIndex].type = kHyperVMessageTypeNone;
  }
};

#endif
