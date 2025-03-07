//
//  HyperVSnapshotUserClientInternal.hpp
//  Hyper-V snapshot (VSS) user client
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#ifndef HyperVSnapshotUserClientInternal_hpp
#define HyperVSnapshotUserClientInternal_hpp

#include "HyperVICUserClient.hpp"
#include "HyperVSnapshotUserClient.h"

class HyperVSnapshotUserClient : public HyperVICUserClient {
  OSDeclareDefaultStructors(HyperVSnapshotUserClient);
  HVDeclareLogFunctions("snapuser");
  typedef HyperVICUserClient super;

private:
  //
  // Userspace communication methods.
  //
  IOReturn notifySnapshotClient(HyperVSnapshotUserClientNotificationType type);
  IOReturn reportSnapshotAbility(UInt32 magic, IOReturn snapshotStatus);
  IOReturn completeFreeze(UInt32 magic, IOReturn freezeStatus);
  IOReturn completeThaw(UInt32 magic, IOReturn thawStatus);

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
  static IOReturn sDispatchMethodReportSnapshotAbility(HyperVSnapshotUserClient *target, void *ref, IOExternalMethodArguments *args);
  static IOReturn sDispatchMethodCompleteFreeze(HyperVSnapshotUserClient *target, void *ref, IOExternalMethodArguments *args);
  static IOReturn sDispatchMethodCompleteThaw(HyperVSnapshotUserClient *target, void *ref, IOExternalMethodArguments *args);
#else
#if (defined(__i386__) && defined(__clang__))
  static IOReturn sMethodReportSnapshotAbility(HyperVSnapshotUserClient* that, UInt32 magic, IOReturn snapshotStatus);
  static IOReturn sMethodCompleteFreeze(HyperVSnapshotUserClient* that, UInt32 magic, IOReturn freezeStatus);
  static IOReturn sMethodCompleteThaw(HyperVSnapshotUserClient* that, UInt32 magic, IOReturn thawStatus);
#else
  IOReturn methodReportSnapshotAbility(UInt32 magic, IOReturn snapshotStatus);
  IOReturn methodCompleteFreeze(UInt32 magic, IOReturn freezeStatus);
  IOReturn methodCompleteThaw(UInt32 magic, IOReturn thawStatus);
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
  IOReturn checkSnapshotAbility();
  IOReturn doFreeze();
  IOReturn doThaw();
};
#endif
