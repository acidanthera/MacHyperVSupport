//
//  HyperVCPU.hpp
//  Hyper-V CPU disabler driver
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVCPU_hpp
#define HyperVCPU_hpp

#include <IOKit/IOService.h>
#include "HyperV.hpp"

class HyperVCPU : public IOService {
  OSDeclareDefaultStructors(HyperVCPU);
  HVDeclareLogFunctions("cpu");
  typedef IOService super;
  
public:
  //
  // IOService overrides.
  //
  IOService* probe(IOService *provider,SInt32 *score) APPLE_KEXT_OVERRIDE;
};

#endif
