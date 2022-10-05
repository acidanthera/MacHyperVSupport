//
//  HyperVFileCopy.hpp
//  Hyper-V file copy driver
//
//  Copyright © 2022 flagers. All rights reserved.
//

#ifndef HyperVFileCopy_hpp
#define HyperVFileCopy_hpp

#include "HyperVICService.hpp"
#include "HyperVFileCopyRegs.hpp"
#include <sys/syslimits.h>
#include <sys/utfconv.h>

class HyperVFileCopy : public HyperVICService {
  OSDeclareDefaultStructors(HyperVFileCopy);
  HVDeclareLogFunctionsVMBusChild("fcopy");
  typedef HyperVICService super;

protected:
  void handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) APPLE_KEXT_OVERRIDE;

private:
  UInt32 status;
  IOLock *lock = nullptr;
  bool isSleeping = false;
  IONotifier *_userClientNotify;
  bool isRegistered = false;
  int sleepForUserspace(UInt32 seconds = 0);
  void wakeForUserspace();
#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_6
  static bool _userClientAvailable(void *target, void *ref,
                                   IOService *newService);
#else
  static bool _userClientAvailable(void *target, void *ref,
                                   IOService *newService,
                                   IONotifier *notifier);
#endif

public:
  //
  // IOService overrides.
  //
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
  IOReturn callPlatformFunction(const OSSymbol *functionName,
                                bool waitForFunction, void *param1,
                                void *param2, void *param3,
                                void *param4) APPLE_KEXT_OVERRIDE;
};

#endif
