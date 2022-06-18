//
//  HyperVModuleDevice.hpp
//  Hyper-V module device driver (ACPI resources for VMBus on Gen2)
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVModuleDevice_hpp
#define HyperVModuleDevice_hpp

#include <IOKit/IOService.h>
#include <IOKit/IORangeAllocator.h>

#include "HyperV.hpp"

class HyperVModuleDevice : public IOService {
  OSDeclareDefaultStructors(HyperVModuleDevice);
  
private:
  bool debugEnabled = false;
  
  
public:
  IORangeAllocator *rangeAllocator;
  //
  // IOService overrides.
  //
  virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#endif
