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
  HyperVVMBusDevice   *_hvDevice          = nullptr;
  IOWorkLoop          *_workLoop          = nullptr;
  IOCommandGate       *_cmdGate           = nullptr;
  IOTimerEventSource  *_timerEventSource  = nullptr;
  

  VMBusVersion       _gfxVersion          = { };
  UInt32             _bitDepth            = 32; // TODO: Not all version support 32-bit
  IOPhysicalAddress  _gfxBase             = 0;
  UInt32             _gfxLength           = 0;
  UInt32        _screenWidth    = 0;
  UInt32        _screenHeight   = 0;
  bool          _fbReady        = false;
  
  IOSimpleLock              *_gfxMsgCursorShapeLock = nullptr;
  HyperVGraphicsMessage     *_gfxMsgCursorShape     = nullptr;
  size_t                    _gfxMsgCursorShapeSize  = 0;

  void handleRefreshTimer(IOTimerEventSource *sender);
  void handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength);
  
  //
  // Internal functions.
  //
  inline UInt32 getScreenDepth() { return (_gfxVersion.value == kHyperVGraphicsVersionV3_0) ? kHyperVGraphicsBitDepth2008 : kHyperVGraphicsBitDepth; }
  IOReturn sendGraphicsMessage(HyperVGraphicsMessage *gfxMessage, HyperVGraphicsMessageType responseType = kHyperVGraphicsMessageTypeError,
                               HyperVGraphicsMessage *gfxMessageResponse = nullptr);
  IOReturn negotiateVersion(VMBusVersion version);
  IOReturn allocateGraphicsMemory(IOPhysicalAddress *outBase, UInt32 *outLength);
  IOReturn refreshFramebufferImage();
  IOReturn setGraphicsMemory(IOPhysicalAddress base, UInt32 length);
  IOReturn setScreenResolution(UInt32 width, UInt32 height, bool waitForAck = true);
  IOReturn setScreenResolutionGated(UInt32 *width, UInt32 *height, bool *waitForAck);

  //
  // Platform functions.
  //
  IOReturn platformInitGraphics(VMBusVersion *outVersion, IOPhysicalAddress *outMemBase, UInt32 *outMemLength);
  IOReturn platformSetScreenResolution(UInt32 *inWidth, UInt32 *inHeight);
  
  

  IOReturn updateCursorShape(const UInt8 *cursorData, UInt32 width, UInt32 height, UInt32 hotX, UInt32 hotY, bool featureChange = false);
  IOReturn updateCursorPosition(SInt32 x, SInt32 y, bool isVisible, bool featureChange = false);

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
