//
//  HyperVShutdownUserClientInternal.hpp
//  Hyper-V guest shutdown user client
//
//  Copyright © 2022-2025 Goldfish64. All rights reserved.
//

#ifndef HyperVShutdownUserClientInternal_hpp
#define HyperVShutdownUserClientInternal_hpp

#include "HyperVICUserClient.hpp"
#include "HyperVShutdownUserClient.h"

class HyperVShutdownUserClient : public HyperVICUserClient {
  OSDeclareDefaultStructors(HyperVShutdownUserClient);
  HVDeclareLogFunctions("shutuser");
  typedef HyperVICUserClient super;

private:
  //
  // Userspace communication methods.
  //
  IOReturn notifyShutdownClient(HyperVShutdownUserClientNotificationType type);
  IOReturn reportShutdownAbility(UInt32 arg);

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
  static IOReturn sDispatchMethodReportShutdownAbility(HyperVShutdownUserClient *target, void *ref, IOExternalMethodArguments *args);
#else
#if (defined(__i386__) && defined(__clang__))
  static IOReturn sMethodReportShutdownAbility(HyperVShutdownUserClient* that, UInt32 arg);
#else
  IOReturn methodReportShutdownAbility(UInt32 arg);
#endif
#endif

public:
  //
  // IOService overrides.
  //
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  void stop(IOService *provider) APPLE_KEXT_OVERRIDE;

  //
  // IOUserClient overrides.
  //
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
  IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *arguments,
                          IOExternalMethodDispatch *dispatch, OSObject *target,
                          void *reference) APPLE_KEXT_OVERRIDE;
#else
  IOExternalMethod *getTargetAndMethodForIndex(IOService **target, UInt32 index) APPLE_KEXT_OVERRIDE;
#endif

  //
  // User client methods.
  //
  bool canShutdown();
  void doShutdown(bool restart);
};

#endif
