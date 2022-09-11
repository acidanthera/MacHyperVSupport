//
//  HyperVInterruptController.hpp
//  Hyper-V synthetic interrupt controller.
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVInterruptController_hpp
#define HyperVInterruptController_hpp

#include <IOKit/IOInterruptController.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOService.h>

#include "HyperV.hpp"

#define kIOInterruptTypeHyperV 0x10000000

class HyperVInterruptController : public IOInterruptController {
  OSDeclareDefaultStructors(HyperVInterruptController);
  HVDeclareLogFunctions("intc");
  typedef IOInterruptController super;
  
private:
  UInt32 vectorCount = 0;
  
public:
  bool init(UInt32 numVectors);
  
  //
  // IOInterruptController overrides.
  //
  IOReturn handleInterrupt(void *refCon, IOService *nub, int source) APPLE_KEXT_OVERRIDE;
  int getVectorType(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) APPLE_KEXT_OVERRIDE;
};

#endif
