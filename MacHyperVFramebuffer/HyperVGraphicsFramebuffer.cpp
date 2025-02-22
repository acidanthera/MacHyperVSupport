//
//  HyperVGraphicsFramebuffer.cpp
//  Hyper-V synthetic graphics framebuffer driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include <IOKit/pci/IOPCIDevice.h>

#include "HyperVGraphicsFramebuffer.hpp"

OSDefineMetaClassAndStructors(HyperVGraphicsFramebuffer, super);

static char const pixelFormatString16[] = IO16BitDirectPixels "\0";
static char const pixelFormatString32[] = IO32BitDirectPixels "\0";

//
// Hotspot was silently added in macOS 10.5.6, no struct version changes.
//
#define kHyperVCursorHotspotMinimumMinor  6

bool HyperVGraphicsFramebuffer::start(IOService *provider) {
  IOPCIDevice *pciDevice;

  HVCheckDebugArgs();
  HVDBGLOG("Initializing Hyper-V Synthetic Framebuffer");

  if (HVCheckOffArg()) {
    HVSYSLOG("Disabling Hyper-V Synthetic Framebuffer due to boot arg");
    return false;
  }

  //
  // Get parent PCI device.
  //
  pciDevice = OSDynamicCast(IOPCIDevice, provider);
  if (pciDevice == nullptr) {
    HVSYSLOG("Provider is not IOPCIDevice");
    return false;
  }

  //
  // Check OS version to determine if hotspot is supported.
  // Hotspot was silently added in macOS 10.5.6.
  //
  if (getKernelVersion() < KernelVersion::Leopard) {
    _hasCursorHotspot = false;
  } else if ((getKernelVersion() == KernelVersion::Leopard) && (getKernelMinorVersion() < kHyperVCursorHotspotMinimumMinor)) {
    _hasCursorHotspot = false;
  } else {
    _hasCursorHotspot = true;
  }

  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    return false;
  }

  //
  // Add model to PCI device.
  //
  pciDevice->setProperty("model", "Hyper-V Graphics");

  HVDBGLOG("Initialized Hyper-V Synthetic Framebuffer");
  return true;
}

void HyperVGraphicsFramebuffer::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V Synthetic Framebuffer");

  if (_cursorData != nullptr) {
    IOFree(_cursorData, _cursorDataSize);
    _cursorData = nullptr;
  }
  if (_gfxModes != nullptr) {
    IOFree(_gfxModes, sizeof (*_gfxModes) * _gfxModesCount);
    _gfxModes = nullptr;
  }
  OSSafeReleaseNULL(_hvGfxProvider);

  super::stop(provider);
}

IOReturn HyperVGraphicsFramebuffer::enableController() {
  IOReturn    status;

  //
  // Get instance of graphics service.
  // This cannot link against the main kext due to macOS requirements, as this kext
  // must be in /L/E on newer macOS versions, but the main one will be injected.
  //
  OSDictionary *gfxProvMatching = IOService::serviceMatching("HyperVGraphics");
  if (gfxProvMatching == nullptr) {
    HVSYSLOG("Failed to create HyperVGraphics matching dictionary");
    return kIOReturnNoResources;
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
    return kIOReturnNotFound;
  }
  _hvGfxProvider->retain();
  HVDBGLOG("Got instance of HyperVGraphics");

  //
  // Initialize graphics service.
  //
  status = initGraphicsService();
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to initialize graphics service with status 0x%X", status);
    return status;
  }

  //
  // Get modes.
  //
  status = buildGraphicsModes();
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to build graphics modes with status 0x%X", status);
    return status;
  }

  return kIOReturnSuccess;
}

bool HyperVGraphicsFramebuffer::isConsoleDevice() {
  HVDBGLOG("start");
  return true;
}

IODeviceMemory* HyperVGraphicsFramebuffer::getApertureRange(IOPixelAperture aperture) {
  HVDBGLOG("Getting aperature for type 0x%X", aperture);
  if (aperture != kIOFBSystemAperture) {
    return nullptr;
  }
  return IODeviceMemory::withRange(_gfxBase, _gfxLength);
}

const char* HyperVGraphicsFramebuffer::getPixelFormats() {
  return nullptr;
}

IOItemCount HyperVGraphicsFramebuffer::getDisplayModeCount() {
  return _gfxModesCount;
}

IOReturn HyperVGraphicsFramebuffer::getDisplayModes(IODisplayModeID *allDisplayModes) {
  //
  // Display mode IDs are just array index+1.
  //
  for (int i = 0; i < _gfxModesCount; i++) {
    allDisplayModes[i] = i + 1;
  }
  return kIOReturnSuccess;
}

IOReturn HyperVGraphicsFramebuffer::getInformationForDisplayMode(IODisplayModeID displayMode, IODisplayModeInformation *info) {
  if ((displayMode == 0) || (displayMode > _gfxModesCount)) {
    return kIOReturnBadArgument;
  }

  //
  // Return information on display mode.
  // All modes are always 60 Hz and 32 bits.
  //
  HVDBGLOG("Got information for mode ID %u %ux%u", displayMode,
           _gfxModes[displayMode - 1].width, _gfxModes[displayMode - 1].height);
  bzero(info, sizeof (*info));
  info->nominalWidth  = _gfxModes[displayMode - 1].width;
  info->nominalHeight = _gfxModes[displayMode - 1].height;;
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
  if ((displayMode == 0) || (displayMode > _gfxModesCount) || (depth != 0)) {
    return kIOReturnBadArgument;
  }
  if (aperture != kIOFBSystemAperture) {
    return kIOReturnUnsupportedMode;
  }

  //
  // Return pixel information on display mode.
  //
  HVDBGLOG("Got pixel information for mode ID %u %ux%u", displayMode,
           _gfxModes[displayMode - 1].width, _gfxModes[displayMode - 1].height);
  bzero(pixelInfo, sizeof (*pixelInfo));

  pixelInfo->bytesPerRow          = _gfxModes[displayMode - 1].width * (getScreenDepth() / kHyperVGraphicsBitsPerByte);
  pixelInfo->bitsPerPixel         = getScreenDepth();
  pixelInfo->pixelType            = kIORGBDirectPixels;
  pixelInfo->bitsPerComponent     = 8;
  pixelInfo->componentCount       = 3;
  pixelInfo->componentMasks[0]    = 0xFF0000;
  pixelInfo->componentMasks[1]    = 0x00FF00;
  pixelInfo->componentMasks[2]    = 0x0000FF;
  pixelInfo->activeWidth          = _gfxModes[displayMode - 1].width;
  pixelInfo->activeHeight         = _gfxModes[displayMode - 1].height;

  if (getScreenDepth() == 32) {
    memcpy(&pixelInfo->pixelFormat[0], &pixelFormatString32[0], sizeof (IOPixelEncoding));
  } else if (getScreenDepth() == 16) {
    memcpy(&pixelInfo->pixelFormat[0], &pixelFormatString16[0], sizeof (IOPixelEncoding));
  }

  return kIOReturnSuccess;
}

IOReturn HyperVGraphicsFramebuffer::getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth) {
  *displayMode = _currentDisplayMode;
  *depth       = 0;

  HVDBGLOG("Got current display mode ID %u", _currentDisplayMode);
  return kIOReturnSuccess;
}

IOReturn HyperVGraphicsFramebuffer::setDisplayMode(IODisplayModeID displayMode, IOIndex depth) {
  if ((displayMode == 0) || (displayMode > _gfxModesCount)) {
    return kIOReturnBadArgument;
  }

  UInt32 width = _gfxModes[displayMode - 1].width;
  UInt32 height = _gfxModes[displayMode - 1].height;

  HVDBGLOG("Setting display mode to ID %u (%ux%u)", displayMode, width, height);
  _currentDisplayMode = displayMode;

  //
  // Instruct graphics service to change resolution.
  //
  return _hvGfxProvider->callPlatformFunction(kHyperVGraphicsPlatformFunctionSetResolution, true,
                                              &width, &height, nullptr, nullptr);
}

IOReturn HyperVGraphicsFramebuffer::getAttribute(IOSelect attribute, uintptr_t *value) {
  //
  // Report that a hardware cursor is supported.
  //
  if (attribute == kIOHardwareCursorAttribute) {
    if (value != nullptr) {
      *value = 1;
    }
    HVDBGLOG("Hardware cursor supported");
    return kIOReturnSuccess;
  }

  return super::getAttribute(attribute, value);
}

IOReturn HyperVGraphicsFramebuffer::setCursorImage(void *cursorImage) {
  IOHardwareCursorDescriptor  cursorDescriptor;
  IOHardwareCursorInfo        cursorInfo;

  //
  // Allocate cursor data if needed.
  //
  if (_cursorData == nullptr) {
    _cursorData = static_cast<UInt8*>(IOMalloc(_cursorDataSize));
    if (_cursorData == nullptr) {
      HVSYSLOG("Failed to allocate memory for hardware cursor");
      return kIOReturnUnsupported;
    }
  }

  //
  // Setup cursor descriptor / info structures and convert the cursor image.
  //
  bzero(&cursorDescriptor, sizeof (cursorDescriptor));
  cursorDescriptor.majorVersion = kHardwareCursorDescriptorMajorVersion;
  cursorDescriptor.minorVersion = kHardwareCursorDescriptorMinorVersion;
  cursorDescriptor.width        = kHyperVGraphicsCursorMaxWidth;
  cursorDescriptor.height       = kHyperVGraphicsCursorMaxHeight;
  cursorDescriptor.bitDepth     = 32U;

  bzero(&cursorInfo, sizeof (cursorInfo));
  cursorInfo.majorVersion       = kHardwareCursorInfoMajorVersion;
  cursorInfo.minorVersion       = kHardwareCursorInfoMinorVersion;
  cursorInfo.hardwareCursorData = _cursorData;

  if (!convertCursorImage(cursorImage, &cursorDescriptor, &cursorInfo)) {
    HVSYSLOG("Failed to convert hardware cursor image");
    return kIOReturnUnsupported;
  }
  if ((cursorInfo.cursorWidth == 0) || (cursorInfo.cursorHeight == 0)) {
    HVSYSLOG("Converted hardware cursor image is invalid size");
    return kIOReturnUnsupported;
  }
  HVDBGLOG("Converted hardware cursor image at %p (%ux%u)", _cursorData, cursorInfo.cursorWidth, cursorInfo.cursorHeight);

  HyperVGraphicsPlatformFunctionSetCursorShapeParams cursorParams = { };
  cursorParams.cursorData = _cursorData;
  cursorParams.width      = cursorInfo.cursorWidth;
  cursorParams.height     = cursorInfo.cursorHeight;
  cursorParams.hotX       = _hasCursorHotspot ? cursorInfo.cursorHotSpotX : 0;
  cursorParams.hotY       = _hasCursorHotspot ? cursorInfo.cursorHotSpotY : 0;
  return _hvGfxProvider->callPlatformFunction(kHyperVGraphicsPlatformFunctionSetCursorShape, true, &cursorParams, nullptr, nullptr, nullptr);
}

IOReturn HyperVGraphicsFramebuffer::setCursorState(SInt32 x, SInt32 y, bool visible) {
  return _hvGfxProvider->callPlatformFunction(kHyperVGraphicsPlatformFunctionSetCursorPosition, true, &x, &y, &visible, nullptr);
}

void HyperVGraphicsFramebuffer::flushCursor() {
  _hvGfxProvider->callPlatformFunction(kHyperVGraphicsPlatformFunctionSetCursorShape, true, nullptr, nullptr, nullptr, nullptr);
}
