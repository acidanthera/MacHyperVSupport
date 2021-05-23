
#ifndef _HV_SYNIC_PROCESSOR_H_
#define _HV_SYNIC_PROCESSOR_H_

#include <IOKit/IOLib.h>
#include <IOKit/IOInterruptEventSource.h>

class HyperVVMBusController;

class SynICProcessor : public OSObject {
  OSDeclareDefaultStructors(SynICProcessor)
  
private:
  UInt32 cpu;
  HyperVVMBusController *vmbus;

  IOInterruptEventSource *interruptEventSource;
  void handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count);
  
public:
  static SynICProcessor *syncICProcessor(UInt32 cpu, HyperVVMBusController *vmbus);
  
  bool setupInterrupt();
  void teardownInterrupt();
  void triggerInterrupt();
};

#endif
