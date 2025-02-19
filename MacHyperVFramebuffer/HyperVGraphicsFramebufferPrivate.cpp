//
//  HyperVGraphicsFramebufferPrivate.cpp
//  Hyper-V synthetic graphics framebuffer driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "HyperVGraphicsFramebuffer.hpp"

#define kHyperVSupportedResolutionsKey    "SupportedResolutions"
#define kHyperVHeightKey                  "Height"
#define kHyperVWidthKey                   "Width"

IOReturn HyperVGraphicsFramebuffer::getGraphicsServiceVersion() {
  if (_hvGfxProvider == nullptr) {
    return kIOReturnUnsupported;
  }
  return _hvGfxProvider->callPlatformFunction(kHyperVGraphicsFunctionGetVersion, true, &_gfxVersion, nullptr, nullptr, nullptr);
}

IOReturn HyperVGraphicsFramebuffer::getGraphicsServiceMemory() {
  HyperVGraphicsFunctionGetMemoryResults memResults;
  IOReturn                               status;

  if (_hvGfxProvider == nullptr) {
    return kIOReturnUnsupported;
  }

  status = _hvGfxProvider->callPlatformFunction(kHyperVGraphicsFunctionGetMemory, true, &memResults, nullptr, nullptr, nullptr);
  if (status != kIOReturnSuccess) {
    return status;
  }
  _gfxBase  = memResults.base;
  _gfxLength = memResults.length;

  HVSYSLOG("Graphics memory located at %p length 0x%X", _gfxBase, _gfxLength);
  return kIOReturnSuccess;
}

IOReturn HyperVGraphicsFramebuffer::buildGraphicsModes() {
  OSArray *resArray = OSDynamicCast(OSArray, getProperty(kHyperVSupportedResolutionsKey));
  if (resArray == nullptr) {
    return buildFallbackMode();
  }

  //
  // Populate modes.
  //
  _gfxModesCount = resArray->getCount();
  _gfxModes = static_cast<HyperVGraphicsMode*>(IOMalloc(sizeof (*_gfxModes) * _gfxModesCount));
  if (_gfxModes == nullptr) {
    HVSYSLOG("Failed to allocate graphics modes");
    return kIOReturnNoResources;
  }

  for (UInt32 i = 0; i < _gfxModesCount; i++) {
    //
    // Get height/width for each mode.
    //
    OSDictionary *modeDict = OSDynamicCast(OSDictionary, resArray->getObject(i));
    if (modeDict == nullptr) {
      HVSYSLOG("Graphics mode %u is not a dictionary", i);
      IOFree(_gfxModes, sizeof (*_gfxModes) * _gfxModesCount);
      return buildFallbackMode();
    }

    OSNumber *width = OSDynamicCast(OSNumber, modeDict->getObject(kHyperVWidthKey));
    OSNumber *height = OSDynamicCast(OSNumber, modeDict->getObject(kHyperVHeightKey));
    if ((width == nullptr) || (height == nullptr)) {
      HVSYSLOG("Graphics mode %u is missing keys", i);
      IOFree(_gfxModes, sizeof (*_gfxModes) * _gfxModesCount);
      return buildFallbackMode();
    }

    _gfxModes[i].width = width->unsigned32BitValue();
    _gfxModes[i].height = height->unsigned32BitValue();

    //
    // Validate sizes are within range.
    //
    if (_gfxVersion.value == kHyperVGraphicsVersionV3_0) {
      if ((_gfxModes[i].width > kHyperVGraphicsMaxWidth2008) || (_gfxModes[i].height > kHyperVGraphicsMaxHeight2008)) {
        HVSYSLOG("Invalid screen resolution %ux%u at %u", _gfxModes[i].width, _gfxModes[i].height, i);
        IOFree(_gfxModes, sizeof (*_gfxModes) * _gfxModesCount);
        return buildFallbackMode();
      }
    }
    if ((_gfxModes[i].width < kHyperVGraphicsMinWidth) || (_gfxModes[i].height < kHyperVGraphicsMinHeight)
        || ((_gfxModes[i].width * _gfxModes[i].height * (_bitDepth / 8)) > _gfxLength)) {
      HVSYSLOG("Invalid screen resolution %ux%u at %u", _gfxModes[i].width, _gfxModes[i].height, i);
      IOFree(_gfxModes, sizeof (*_gfxModes) * _gfxModesCount);
      return buildFallbackMode();
    }
    HVDBGLOG("Added graphics mode %ux%u at %u", _gfxModes[i].width, _gfxModes[i].height, i);
  }

  return kIOReturnSuccess;
}

IOReturn HyperVGraphicsFramebuffer::buildFallbackMode() {
  HVSYSLOG("Graphics modes could not be loaded, using fallback");

  //
  // Use default 1024x768 mode if the modes could not be fetched.
  //
  _gfxModesCount = 1;
  _gfxModes = static_cast<HyperVGraphicsMode*>(IOMalloc(sizeof (*_gfxModes) * _gfxModesCount));
  if (_gfxModes == nullptr) {
    HVSYSLOG("Failed to allocate graphics modes");
    return kIOReturnNoResources;
  }
  _gfxModes[0] = { 1024, 768 };
  return kIOReturnSuccess;
}
