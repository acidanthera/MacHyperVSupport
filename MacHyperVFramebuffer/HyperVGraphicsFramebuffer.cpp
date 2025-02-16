//
//  HyperVGraphicsFramebuffer.cpp
//  Hyper-V synthetic graphics framebuffer driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "HyperVGraphicsFramebuffer.hpp"
#include "HyperVGraphicsPlatformFunctions.hpp"

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
  IODeviceMemory  *deviceMemory;
  IOReturn        status;

  //
  // Get instance of graphics service.
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
  IODeviceMemory *deviceMemory;

  HVDBGLOG("Getting aperature for type 0x%X", aperture);
  if (aperture != kIOFBSystemAperture) {
    return nullptr;
  }

  deviceMemory = getProvider()->getDeviceMemoryWithIndex(0);
  if (deviceMemory != nullptr) {
    deviceMemory->retain();
    HVDBGLOG("Got device memory at %p (%u bytes)", deviceMemory->getPhysicalAddress(), deviceMemory->getLength());
  }
  return deviceMemory;
}

const char* HyperVGraphicsFramebuffer::getPixelFormats() {
  return nullptr;
}

IOItemCount HyperVGraphicsFramebuffer::getDisplayModeCount() {
  return arrsize(graphicsModes);
}

IOReturn HyperVGraphicsFramebuffer::getDisplayModes(IODisplayModeID *allDisplayModes) {
  //
  // Display mode IDs are just array index+1.
  //
  for (int i = 0; i < arrsize(graphicsModes); i++) {
    allDisplayModes[i] = i + 1;
  }
  return kIOReturnSuccess;
}

IOReturn HyperVGraphicsFramebuffer::getInformationForDisplayMode(IODisplayModeID displayMode, IODisplayModeInformation *info) {
  if (displayMode == 0 || displayMode >= arrsize(graphicsModes)) {
    return kIOReturnBadArgument;
  }

  //
  // Return information on display mode.
  // All modes are always 60 Hz and 32 bits.
  //
  HVDBGLOG("Got information for mode ID %u %ux%u", displayMode,
           graphicsModes[displayMode - 1].width, graphicsModes[displayMode - 1].height);
  bzero(info, sizeof (*info));
  info->nominalWidth  = graphicsModes[displayMode - 1].width;
  info->nominalHeight = graphicsModes[displayMode - 1].height;;
  info->refreshRate   = 60 << 16;
  info->maxDepthIndex = 0;

  return kIOReturnSuccess;
}

UInt64 HyperVGraphicsFramebuffer::getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth) {
  //
  // Obsolete method that always returns zero.
  //
  return 0;
}

IOReturn HyperVGraphicsFramebuffer::getPixelInformation(IODisplayModeID displayMode, IOIndex depth, IOPixelAperture aperture, IOPixelInformation *pixelInfo) {
  if (displayMode == 0 || displayMode >= arrsize(graphicsModes) || depth != 0) {
    return kIOReturnBadArgument;
  }
  if (aperture != kIOFBSystemAperture) {
    return kIOReturnUnsupportedMode;
  }

  //
  // Return pixel information on display mode.
  //
  HVDBGLOG("Got pixel information for mode ID %u %ux%u", displayMode,
           graphicsModes[displayMode - 1].width, graphicsModes[displayMode - 1].height);
  bzero(pixelInfo, sizeof (*pixelInfo));

  pixelInfo->bytesPerRow          = graphicsModes[displayMode - 1].width * (32 / 8);
  pixelInfo->bitsPerPixel         = 32;
  pixelInfo->pixelType            = kIORGBDirectPixels;
  pixelInfo->bitsPerComponent     = 8;
  pixelInfo->componentCount       = 3;
  pixelInfo->componentMasks[0]    = 0xFF0000;
  pixelInfo->componentMasks[1]    = 0x00FF00;
  pixelInfo->componentMasks[2]    = 0x0000FF;
  pixelInfo->activeWidth          = graphicsModes[displayMode - 1].width;
  pixelInfo->activeHeight         = graphicsModes[displayMode - 1].height;

  //if (videoDepth == 32) {
    strlcpy(&pixelInfo->pixelFormat[0], &pixelFormatString32[0], sizeof (IOPixelEncoding));
 // } else if (videoDepth == 16) {
 //   strlcpy(&pixelInfo->pixelFormat[0], &pixelFormatString16[0], sizeof (IOPixelEncoding));
 // }

  return kIOReturnSuccess;
}

IOReturn HyperVGraphicsFramebuffer::getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth) {
  *displayMode = _currentDisplayMode;
  *depth       = 0;

  HVDBGLOG("Got current display mode ID %u", _currentDisplayMode);
  return kIOReturnSuccess;
}

IOReturn HyperVGraphicsFramebuffer::setDisplayMode(IODisplayModeID displayMode, IOIndex depth) {
  if (displayMode >= arrsize(graphicsModes)) {
    return kIOReturnBadArgument;
  }

  UInt32 width = graphicsModes[displayMode - 1].width;
  UInt32 height = graphicsModes[displayMode - 1].height;

  HVDBGLOG("Setting display mode to ID %u (%ux%u)", displayMode, width, height);
  _currentDisplayMode = displayMode;

  //
  // Instruct graphics service to change resolution.
  //
  return _hvGfxProvider->callPlatformFunction(kHyperVGraphicsFunctionSetResolution, true,
                                              &width, &height, nullptr, nullptr);
}
