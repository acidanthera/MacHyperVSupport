//
//  HyperVInterruptController.cpp
//  Hyper-V synthetic interrupt controller.
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVInterruptController.hpp"

#ifndef kVectorCountKey
#define kVectorCountKey               "Vector Count"
#endif

#ifndef kInterruptControllerNameKey
#define kInterruptControllerNameKey   "InterruptControllerName"
#endif

OSDefineMetaClassAndStructors(HyperVInterruptController, super);

bool HyperVInterruptController::init(UInt32 numVectors) {
  if (!super::init()) {
    HVSYSLOG("Superclass init function failed");
    return false;
  }
  
  //
  // Allocate vectors.
  //
  vectorCount = numVectors;
  setProperty(kVectorCountKey, vectorCount, 32);
  HVDBGLOG("Initializing Hyper-V interrupt controller with %u vector slots", vectorCount);
  
  vectors = IONew(IOInterruptVector, vectorCount);
  if (vectors == nullptr) {
    HVSYSLOG("Failed to allocate vectors");
    return false;
  }
  bzero(vectors, sizeof (IOInterruptVector) * vectorCount);
  
  // Allocate lock for each vector.
  for (UInt32 i = 0; i < vectorCount; i++) {
    vectors[i].interruptLock = IOLockAlloc();
    if (vectors[i].interruptLock == nullptr) {
      HVSYSLOG("Failed to allocate vector %u lock", i);
      return false;
    }
  }
  
  //
  // Attach to platform expert.
  //
  attach(getPlatform());
  const OSSymbol *symName = copyName();
  setProperty(kInterruptControllerNameKey, (OSObject*) symName);
  getPlatform()->registerInterruptController((OSSymbol*) symName, this);
  symName->release();
  
  registerService();
  HVDBGLOG("Initialized Hyper-V interrupt controller");
  return true;
}

IOReturn HyperVInterruptController::handleInterrupt(void *refCon, IOService *nub, int source) {
  IOInterruptVector *vector;
  
  if (source < 0 || source > vectorCount) {
    return kIOReturnSuccess;
  }
  
  vector = &vectors[source];
  vector->interruptActive = 1;
  if (vector->interruptRegistered && !vector->interruptDisabledHard) {
    vector->handler(vector->target, vector->refCon, vector->nub, vector->source);
  }
  vector->interruptActive = 0;
  
  return kIOReturnSuccess;
}

int HyperVInterruptController::getVectorType(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) {
  return kIOInterruptTypeEdge | kIOInterruptTypeHyperV;
}
