//
//  SynICProcessor.cpp
//  MacHyperVServices
//
//  Created by John Davis on 5/4/21.
//

#include "SynICProcessor.h"
#include "HyperVVMBusController.hpp"

OSDefineMetaClassAndStructors(SynICProcessor, OSObject);

SynICProcessor *SynICProcessor::syncICProcessor(UInt32 cpu, HyperVVMBusController *vmbus) {
  SynICProcessor *me = new SynICProcessor;
  if (me == NULL) {
    return NULL;
  }
  
  me->cpu = cpu;
  me->vmbus = vmbus;
  return me;
}

bool SynICProcessor::setupInterrupt() {
  interruptEventSource = IOInterruptEventSource::interruptEventSource(this, OSMemberFunctionCast(IOInterruptEventAction, this, &SynICProcessor::handleInterrupt));
  if (interruptEventSource == NULL) {
    return false;
  }
  interruptEventSource->enable();
  vmbus->getSynICWorkLoop()->addEventSource(interruptEventSource);
  return true;
}

void SynICProcessor::teardownInterrupt() {
  vmbus->getSynICWorkLoop()->removeEventSource(interruptEventSource);
  interruptEventSource->disable();
  interruptEventSource->release();
}

void SynICProcessor::triggerInterrupt() {
  interruptEventSource->interruptOccurred(0, 0, 0);
}

void SynICProcessor::handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count) {
  //
  // Process message on main VMBus class.
  //
  vmbus->processIncomingVMBusMessage(cpu);
}
