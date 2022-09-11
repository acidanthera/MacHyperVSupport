//
//  HyperVVMBusInterrupts.cpp
//  Hyper-V VMBus controller
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVVMBus.hpp"

class VMBusInterruptProcessor : public OSObject {
  OSDeclareDefaultStructors(VMBusInterruptProcessor)
  
private:
  UInt32                  _cpuIndex              = 0;
  HyperVVMBus             *_vmbus                = nullptr;
  IOInterruptEventSource  *_interruptEventSource = nullptr;
  
  void handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count);
  
public:
  static VMBusInterruptProcessor *vmbusInterruptProcessor(UInt32 cpuIndex, HyperVVMBus *vmbus);
  
  bool setupInterrupt();
  void teardownInterrupt();
  void triggerInterrupt();
};

OSDefineMetaClassAndStructors(VMBusInterruptProcessor, OSObject);

VMBusInterruptProcessor *VMBusInterruptProcessor::vmbusInterruptProcessor(UInt32 cpuIndex, HyperVVMBus *vmbus) {
  VMBusInterruptProcessor *me = new VMBusInterruptProcessor;
  if (me == nullptr) {
    return nullptr;
  }
  
  me->_cpuIndex = cpuIndex;
  me->_vmbus    = vmbus;
  return me;
}

bool VMBusInterruptProcessor::setupInterrupt() {
  _interruptEventSource = IOInterruptEventSource::interruptEventSource(this, OSMemberFunctionCast(IOInterruptEventAction, this, &VMBusInterruptProcessor::handleInterrupt));
  if (_interruptEventSource == nullptr) {
    return false;
  }
  _interruptEventSource->enable();
  _vmbus->getWorkLoop()->addEventSource(_interruptEventSource);
  return true;
}

void VMBusInterruptProcessor::teardownInterrupt() {
  _vmbus->getWorkLoop()->removeEventSource(_interruptEventSource);
  _interruptEventSource->disable();
  _interruptEventSource->release();
}

void VMBusInterruptProcessor::triggerInterrupt() {
  _interruptEventSource->interruptOccurred(0, 0, 0);
}

void VMBusInterruptProcessor::handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count) {
  //
  // Process message on main VMBus class.
  //
  _vmbus->processIncomingVMBusMessage(_cpuIndex);
}

void HyperVVMBus::handleDirectInterrupt(OSObject *target, void *refCon, IOService *nub, int source) {
  vmbusInterruptProcs[cpu_number()]->triggerInterrupt();
}

bool HyperVVMBus::allocateInterruptEventSources() {
  vmbusInterruptProcsCount = real_ncpus;
  
  vmbusInterruptProcs = IONew(VMBusInterruptProcessor*, vmbusInterruptProcsCount);
  bzero(vmbusInterruptProcs, sizeof (VMBusInterruptProcessor*) * vmbusInterruptProcsCount);
  for (UInt32 cpuIndex = 0; cpuIndex < vmbusInterruptProcsCount; cpuIndex++) {
    VMBusInterruptProcessor *vmbusInterruptProcessor = VMBusInterruptProcessor::vmbusInterruptProcessor(cpuIndex, this);
    if (vmbusInterruptProcessor == nullptr) {
      return false;
    }
    vmbusInterruptProcessor->setupInterrupt();
    vmbusInterruptProcs[cpuIndex] = vmbusInterruptProcessor;
  }
  
  if (registerInterrupt(0, this, OSMemberFunctionCast(IOInterruptAction, this, &HyperVVMBus::handleDirectInterrupt)) != kIOReturnSuccess
      || enableInterrupt(0) != kIOReturnSuccess) {
    HVSYSLOG("Failed to setup interrupt handler");
    return false;
  }
  
  return true;
}
