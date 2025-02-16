//
//  HyperVGraphics.hpp
//  Hyper-V synthetic graphics driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#ifndef HyperVGraphics_hpp
#define HyperVGraphics_hpp

#include <IOKit/IORangeAllocator.h>
#include <IOKit/IOService.h>

#include "HyperVVMBusDevice.hpp"
#include "HyperVGraphicsPlatformFunctions.hpp"
#include "HyperVGraphicsRegs.hpp"

class HyperVGraphics : public IOService {
  OSDeclareDefaultStructors(HyperVGraphics);
  HVDeclareLogFunctionsVMBusChild("gfx");
  typedef IOService super;

private:
  HyperVVMBusDevice  *_hvDevice              = nullptr;
  VMBusVersion       _currentGraphicsVersion = { };
  IOTimerEventSource *_timerEventSource      = nullptr;

  IORangeScalar _gfxMmioBase    = 0;
  IORangeScalar _gfxMmioLength  = 0;
  IORangeScalar _fbBaseAddress   = 0;
  IORangeScalar _fbTotalLength   = 0;
  IORangeScalar _fbInitialLength = 0;
  UInt32        _screenWidth    = 0;
  UInt32        _screenHeight   = 0;
  bool          _fbReady        = false;

  //bool wakePacketHandler(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength);
  void handleRefreshTimer(IOTimerEventSource *sender);
  void handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength);
  IOReturn sendGraphicsMessage(HyperVGraphicsMessage *gfxMessage, HyperVGraphicsMessageType responseType = kHyperVGraphicsMessageTypeError,
                               HyperVGraphicsMessage *gfxMessageResponse = nullptr);
  IOReturn negotiateVersion(VMBusVersion version);
  IOReturn connectGraphics();
  IOReturn allocateGraphicsMemory();
  IOReturn updateGraphicsMemoryLocation();
  IOReturn updateCursorShape(const UInt8 *cursorData, UInt32 width, UInt32 height, UInt32 hotX, UInt32 hotY);
  IOReturn updateScreenResolution(UInt32 width, UInt32 height, bool isBoot);

public:
  //
  // IOService overrides.
  //
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
  IOReturn callPlatformFunction(const OSSymbol *functionName, bool waitForFunction,
                                void *param1, void *param2, void *param3, void *param4) APPLE_KEXT_OVERRIDE;
};

#endif
