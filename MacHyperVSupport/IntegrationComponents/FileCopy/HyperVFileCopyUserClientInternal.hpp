//
//  HyperVFileCopyUserClientInternal.hpp
//  Hyper-V file copy user client
//
//  Copyright © 2022-2025 flagers, Goldfish64. All rights reserved.
//

#ifndef HyperVFileCopyUserClient_hpp
#define HyperVFileCopyUserClient_hpp

#include "HyperVICUserClient.hpp"
#include "HyperVFileCopyRegs.hpp"
#include "HyperVFileCopyUserClient.h"

class HyperVFileCopyUserClient : public HyperVICUserClient {
  OSDeclareDefaultStructors(HyperVFileCopyUserClient);
  HVDeclareLogFunctions("fcopyuser");
  typedef HyperVICUserClient super;

private:
  UInt8 _currentFileName[PATH_MAX];
  UInt8 _currentFilePath[PATH_MAX];
  UInt8 *_currentFileData = nullptr;

  //
  // Userspace communication methods.
  //
  bool convertUnicodePath(UInt16 *inputStr, UInt8 *outputStr, size_t outputStrSize);
  IOReturn notifyFileCopyClientApplication(HyperVFileCopyUserClientNotificationMessage *notificationMsg);
  IOReturn getFilePath(void *output, UInt32 *outputSize);
  IOReturn getNextDataFragment(IOMemoryDescriptor *memoryDesc, void *output, UInt32 *outputSize);
  IOReturn completeIO(IOReturn status);

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
  static IOReturn sDispatchMethodGetFilePath(HyperVFileCopyUserClient *target, void *ref, IOExternalMethodArguments *args);
  static IOReturn sDispatchMethodGetNextDataFragment(HyperVFileCopyUserClient *target, void *ref, IOExternalMethodArguments *args);
  static IOReturn sDispatchMethodCompleteIO(HyperVFileCopyUserClient *target, void *ref, IOExternalMethodArguments *args);
#else
#if (defined(__i386__) && defined(__clang__))
  static IOReturn sMethodGetFilePath(HyperVFileCopyUserClient *that, void *output, UInt32 *outputSize);
  static IOReturn sMethodGetNextDataFragment(HyperVFileCopyUserClient *that, void *output, UInt32 *outputSize);
  static IOReturn sMethodCompleteIO(HyperVFileCopyUserClient *that, UInt32 status);
#else
  IOReturn methodGetFilePath(void *output, UInt32 *outputSize);
  IOReturn methodGetNextDataFragment(void *output, UInt32 *outputSize);
  IOReturn methodCompleteIO(UInt32 status);
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
  IOReturn startFileCopy(UInt16 *fileName, UInt16 *filePath, HyperVFileCopyMessageFlags flags, UInt64 fileSize);
  IOReturn writeFileFragment(UInt64 offset, UInt8 *data, UInt32 dataSize);
  IOReturn completeFileCopy();
  IOReturn cancelFileCopy();
};

#endif
