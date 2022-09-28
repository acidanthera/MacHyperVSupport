//
//  HyperVUserClientInternal.hpp
//  Hyper-V userspace client
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVUserClientInternal_hpp
#define HyperVUserClientInternal_hpp

#include <IOKit/IOUserClient.h>

#include "HyperV.hpp"
#include "HyperVController.hpp"
#include "HyperVUserClient.h"

class HyperVUserClient : public IOUserClient {
  OSDeclareDefaultStructors(HyperVUserClient);
  HVDeclareLogFunctions("user");
  typedef IOUserClient super;

private:
  HyperVController                    *_hvController   = nullptr;
  HyperVUserClientNotificationMessage _notificationMsg = { };

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

  //
  // Other functions.
  //
  IOReturn notifyClientApplication(HyperVUserClientNotificationType type, void *data, UInt32 dataLength);
};

#endif
