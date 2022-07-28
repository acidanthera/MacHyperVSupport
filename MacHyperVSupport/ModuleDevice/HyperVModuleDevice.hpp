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
  typedef IOService super;
  
private:
  bool debugEnabled = false;
  IORangeAllocator *rangeAllocatorLow;
  IORangeAllocator *rangeAllocatorHigh;
  
public:
  //
  // IOService overrides.
  //
  virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
  
  IORangeScalar allocateRange(IORangeScalar size, IORangeScalar alignment, bool highMemory);
  void freeRange(IORangeScalar start, IORangeScalar size);
};

#endif
