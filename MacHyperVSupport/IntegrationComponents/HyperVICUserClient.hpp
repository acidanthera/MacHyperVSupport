//
//  HyperVICUserClient.hpp
//  Hyper-V IC user client base class
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef HyperVICUserClient_hpp
#define HyperVICUserClient_hpp

#include <IOKit/IOUserClient.h>

#include "HyperV.hpp"
#include "HyperVICService.hpp"

class HyperVICUserClient : public IOUserClient {
  OSDeclareDefaultStructors(HyperVICUserClient);
  HVDeclareLogFunctions("icuser");
  typedef IOUserClient super;

private:
  HyperVICService *_hvICProvider = nullptr;

protected:
  task_t        _task             = nullptr;
  IOLock        *_sleepLock       = nullptr;
  bool          _isSleeping       = false;
  IOReturn      _sleepStatus      = kIOReturnSuccess;
  mach_port_t   _notificationPort = MACH_PORT_NULL;

  void setICDebug(bool debug) { debugEnabled = debug; }
  IOReturn sleepThread();
  void wakeThread(IOReturn status);

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
  bool initWithTask(task_t owningTask, void *securityToken, UInt32 type, OSDictionary *properties) APPLE_KEXT_OVERRIDE;
  IOReturn clientClose() APPLE_KEXT_OVERRIDE;
  IOReturn registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon) APPLE_KEXT_OVERRIDE;
};

#endif
