//
//  HyperVShutdownUserClientInternal.hpp
//  Hyper-V guest shutdown userspace client
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVShutdownUserClientInternal_hpp
#define HyperVShutdownUserClientInternal_hpp

#include <IOKit/IOUserClient.h>

#include "HyperVShutdown.hpp"
#include "HyperVShutdownUserClient.h"

class HyperVShutdownUserClient : public IOUserClient {
  OSDeclareDefaultStructors(HyperVShutdownUserClient);
  HVDeclareLogFunctions("shut");
  typedef IOUserClient super;
  
private:
  HyperVShutdown                    *hvShutdown;
  HyperVShutdownNotificationMessage notificationMsg;
  
  IOReturn notifyShutdown();
  
public:
  //
  // IOService overrides.
  //
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
  IOReturn message(UInt32 type, IOService *provider, void *argument = NULL) APPLE_KEXT_OVERRIDE;
  
  //
  // IOUserClient overrides.
  //
  IOReturn clientClose() APPLE_KEXT_OVERRIDE;
  IOReturn registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon) APPLE_KEXT_OVERRIDE;
};

#endif
