//
//  HyperVShutdown.hpp
//  Hyper-V guest shutdown driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVShutdown_hpp
#define HyperVShutdown_hpp

#include "HyperVICService.hpp"

typedef enum : UInt32 {
  kHyperVShutdownMessageTypeShutdownRequested = 0x66697368,
  kHyperVShutdownMessageTypePerformShutdown   = 0x66697369
} HyperVShutdownMessageType;

class HyperVShutdown : public HyperVICService {
  OSDeclareDefaultStructors(HyperVShutdown);
  HVDeclareLogFunctionsVMBusChild("shut");
  typedef HyperVICService super;

private:
  IOService *userClientInstance = nullptr;
  
  bool handleShutdown(VMBusICMessageShutdownData *shutdownData);

protected:
  bool processMessage() APPLE_KEXT_OVERRIDE;

public:
  //
  // IOService overrides.
  //
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
  bool open(IOService *forClient, IOOptionBits options = 0, void *arg = nullptr) APPLE_KEXT_OVERRIDE;
  void close(IOService *forClient, IOOptionBits options = 0) APPLE_KEXT_OVERRIDE;
};

#endif
