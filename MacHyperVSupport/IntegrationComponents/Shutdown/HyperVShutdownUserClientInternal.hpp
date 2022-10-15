//
//  HyperVShutdownUserClientInternal.hpp
//  Hyper-V guest shutdown user client
//
//  Copyright © 2022 Goldfish64. All rights reserved.
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
  bool _isShutdownSupported = false;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
  static const IOExternalMethodDispatch sShutdownMethods[kHyperVShutdownUserClientMethodNumberOfMethods];
#else
#endif

  IOReturn notifyClientApplication(HyperVShutdownUserClientNotificationType type);

  //
  // Userspace external methods.
  //
  static IOReturn methodReportShutdownAbility(HyperVShutdownUserClient *target, void *ref, IOExternalMethodArguments *args);

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
#endif

  bool canShutdown();
  void doShutdown(bool restart);
};

#endif
