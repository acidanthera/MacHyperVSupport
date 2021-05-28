//
//  SynICProcessor.hpp
//  Hyper-V SynIC per-processor interrupt handling
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef SynICProcessor_hpp
#define SynICProcessor_hpp

#include <IOKit/IOLib.h>
#include <IOKit/IOInterruptEventSource.h>

class HyperVVMBusController;

class SynICProcessor : public OSObject {
  OSDeclareDefaultStructors(SynICProcessor)
  
private:
  UInt32                  cpu;
  HyperVVMBusController   *vmbus;
  IOInterruptEventSource  *interruptEventSource;
  
  void handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count);
  
public:
  static SynICProcessor *syncICProcessor(UInt32 cpu, HyperVVMBusController *vmbus);
  
  bool setupInterrupt();
  void teardownInterrupt();
  void triggerInterrupt();
};

#endif
