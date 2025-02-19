//
//  HyperVGraphicsFramebuffer.hpp
//  Hyper-V synthetic graphics framebuffer driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#ifndef MacHyperVFramebuffer_hpp
#define MacHyperVFramebuffer_hpp

#include <IOKit/graphics/IOFramebuffer.h>

#include "HyperV.hpp"
#include "HyperVGraphicsPlatformFunctions.hpp"
#include "HyperVGraphicsRegs.hpp"

typedef struct {
  UInt32 width;
  UInt32 height;
} HyperVGraphicsMode;

class HyperVGraphicsFramebuffer : public IOFramebuffer {
  OSDeclareDefaultStructors(HyperVGraphicsFramebuffer);
  HVDeclareLogFunctions("gfxfb");
  typedef IOFramebuffer super;

private:
  IOService           *_hvGfxProvider = nullptr;
  IOPhysicalAddress   _gfxBase        = 0;
  UInt32              _gfxLength      = 0;
  HyperVGraphicsMode  *_gfxModes      = nullptr;
  IOItemCount         _gfxModesCount  = 0;
  VMBusVersion        _gfxVersion     = { };
  UInt32              _bitDepth       = 32; // TODO: Make dynamic
  IODisplayModeID _currentDisplayMode = 4;
  UInt8           *_cursorData = nullptr;
  size_t          _cursorDataSize = 16834;
  bool            _hasCursorHotspot = false;

  IOReturn getGraphicsServiceVersion();
  IOReturn getGraphicsServiceMemory();
  IOReturn buildGraphicsModes();
  IOReturn buildFallbackMode();

public:
  //
  // IOService overrides.
  //
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  void stop(IOService *provider) APPLE_KEXT_OVERRIDE;

  //
  // IOFramebuffer overrides.
  //
  IOReturn enableController() APPLE_KEXT_OVERRIDE;
  bool isConsoleDevice() APPLE_KEXT_OVERRIDE;
  IODeviceMemory* getApertureRange(IOPixelAperture aperture) APPLE_KEXT_OVERRIDE;
  const char* getPixelFormats() APPLE_KEXT_OVERRIDE;
  IOItemCount getDisplayModeCount() APPLE_KEXT_OVERRIDE;
  IOReturn getDisplayModes(IODisplayModeID *allDisplayModes) APPLE_KEXT_OVERRIDE;
  IOReturn getInformationForDisplayMode(IODisplayModeID displayMode, IODisplayModeInformation *info) APPLE_KEXT_OVERRIDE;
  UInt64 getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth) APPLE_KEXT_OVERRIDE;
  IOReturn getPixelInformation(IODisplayModeID displayMode, IOIndex depth,
                               IOPixelAperture aperture, IOPixelInformation *pixelInfo) APPLE_KEXT_OVERRIDE;
  IOReturn getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth) APPLE_KEXT_OVERRIDE;
  IOReturn setDisplayMode(IODisplayModeID displayMode, IOIndex depth) APPLE_KEXT_OVERRIDE;
  IOReturn getAttribute(IOSelect attribute, uintptr_t *value) APPLE_KEXT_OVERRIDE;
  IOReturn setCursorImage(void *cursorImage) APPLE_KEXT_OVERRIDE;
  IOReturn setCursorState(SInt32 x, SInt32 y, bool visible) APPLE_KEXT_OVERRIDE;
};

#endif
