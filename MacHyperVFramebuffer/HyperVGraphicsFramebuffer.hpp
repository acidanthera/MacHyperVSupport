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

class HyperVGraphicsFramebuffer : public IOFramebuffer {
  OSDeclareDefaultStructors(HyperVGraphicsFramebuffer);
  HVDeclareLogFunctions("gfxfb");
  typedef IOFramebuffer super;

private:
  IOService       *_hvGfxProvider = nullptr;
  IODisplayModeID _currentDisplayMode = 4;
  UInt8           *_cursorData = nullptr;
  size_t          _cursorDataSize = 16834;
  bool            _hasCursorHotspot = false;

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
