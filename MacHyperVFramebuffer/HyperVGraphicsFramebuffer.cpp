//
//  HyperVGraphicsFramebuffer.cpp
//  Hyper-V synthetic graphics framebuffer driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "HyperVGraphicsFramebuffer.hpp"

OSDefineMetaClassAndStructors(HyperVGraphicsFramebuffer, super);

static char const pixelFormatString16[] = IO16BitDirectPixels "\0";
static char const pixelFormatString32[] = IO32BitDirectPixels "\0";

typedef struct {
  UInt32 width;
  UInt32 height;
} HyperVGraphicsMode;

//
// List of all default graphics modes.
// TODO: Hyper-V on Windows 10 and newer can directly specify what modes are supported.
//
static const HyperVGraphicsMode graphicsModes[] = {
  { 640,  480  },
  { 800,  600  },
  { 1024, 768  },
  { 1152, 864  },
  { 1280, 720  },
  { 1280, 1024 },
  { 1440, 900  },
  { 1600, 900  },
  { 1600, 1200 },
};

bool HyperVGraphicsFramebuffer::start(IOService *provider) {
  HVCheckDebugArgs();
  HVDBGLOG("Initializing Hyper-V Synthetic Framebuffer");

  if (HVCheckOffArg()) {
    HVSYSLOG("Disabling Hyper-V Synthetic Framebuffer due to boot arg");
    return false;
  }

  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    return false;
  }

  HVDBGLOG("Initialized Hyper-V Synthetic Framebuffer");
  return true;
}

void HyperVGraphicsFramebuffer::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V Synthetic Framebuffer");

  OSSafeReleaseNULL(_hvGfxProvider);
  super::stop(provider);
}

IOReturn HyperVGraphicsFramebuffer::enableController() {
  HVDBGLOG("Enabling controller");

  //
  // Get instance of graphics driver service.
  // This cannot link against the main kext due to macOS requirements, as this kext
  // must be in /L/E on newer macOS versions, but the main one will be injected.
  //
  OSDictionary *gfxProvMatching = IOService::serviceMatching("HyperVGraphics");
  if (gfxProvMatching == nullptr) {
    HVSYSLOG("Failed to create HyperVGraphics matching dictionary");
    return kIOReturnIOError;
  }

  HVDBGLOG("Waiting for HyperVGraphics");
#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_6
  _hvGfxProvider = IOService::waitForService(gfxProvMatching);
  if (_hvGfxProvider != nullptr) {
    _hvGfxProvider->retain();
  }
#else
  _hvGfxProvider = waitForMatchingService(gfxProvMatching);
  gfxProvMatching->release();
#endif

  if (_hvGfxProvider == nullptr) {
    HVSYSLOG("Failed to locate HyperVGraphics");
    return kIOReturnIOError;
  }
  HVDBGLOG("Got instance of HyperVGraphics");

  return kIOReturnSuccess;
}

bool HyperVGraphicsFramebuffer::isConsoleDevice() {
  HVDBGLOG("start");
  return true;
}

IODeviceMemory* HyperVGraphicsFramebuffer::getApertureRange(IOPixelAperture aperture) {
  return nullptr;
}

const char* HyperVGraphicsFramebuffer::getPixelFormats() {
  return nullptr;
}

IOItemCount HyperVGraphicsFramebuffer::getDisplayModeCount() {
  return 1;
}

IOReturn HyperVGraphicsFramebuffer::getDisplayModes(IODisplayModeID *allDisplayModes) {
  //allDisplayModes[0] = kUEFIDisplayModeID;
  return kIOReturnUnsupported;
}

IOReturn HyperVGraphicsFramebuffer::getInformationForDisplayMode(IODisplayModeID displayMode, IODisplayModeInformation *info) {

  return kIOReturnUnsupported;
}

UInt64 HyperVGraphicsFramebuffer::getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth) {
  //
  // Obsolete method that always returns zero.
  //
  return 0;
}

IOReturn HyperVGraphicsFramebuffer::getPixelInformation(IODisplayModeID displayMode, IOIndex depth, IOPixelAperture aperture, IOPixelInformation *pixelInfo) {


  return kIOReturnUnsupported;
}

IOReturn HyperVGraphicsFramebuffer::getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth) {

  return kIOReturnUnsupported;
}
