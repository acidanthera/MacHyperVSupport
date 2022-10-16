//
//  HyperVFileCopyUserClientInternal.hpp
//  Hyper-V file copy user client
//
//  Copyright © 2022 flagers, Goldfish64. All rights reserved.
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

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
  static const IOExternalMethodDispatch sFileCopyMethods[kHyperVFileCopyUserClientMethodNumberOfMethods];
#else
#endif

  bool convertUnicodePath(UInt16 *inputStr, UInt8 *outputStr, size_t outputStrSize);
  IOReturn notifyClientApplication(HyperVFileCopyUserClientNotificationMessage *notificationMsg);

  //
  // Userspace external methods.
  //
  static IOReturn methodGetFilePath(HyperVFileCopyUserClient *target, void *ref, IOExternalMethodArguments *args);
  static IOReturn methodGetNextDataFragment(HyperVFileCopyUserClient *target, void *ref, IOExternalMethodArguments *args);
  static IOReturn methodCompleteIO(HyperVFileCopyUserClient *target, void *ref, IOExternalMethodArguments *args);

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
  
  IOReturn startFileCopy(UInt16 *fileName, UInt16 *filePath, HyperVFileCopyMessageFlags flags, UInt64 fileSize);
  IOReturn writeFileFragment(UInt64 offset, UInt8 *data, UInt32 dataSize);
  IOReturn completeFileCopy();
  IOReturn cancelFileCopy();
};

#endif
