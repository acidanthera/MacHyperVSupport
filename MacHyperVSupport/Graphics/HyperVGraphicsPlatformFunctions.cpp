//
//  HyperVGraphicsPlatformFunctions.cpp
//  Hyper-V synthetic graphics driver platform functions
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "HyperVGraphics.hpp"

IOReturn HyperVGraphics::platformInitGraphics(VMBusVersion *outVersion, IOPhysicalAddress *outMemBase, UInt32 *outMemLength) {
  PE_Video consoleInfo;
  IOReturn status;

  if ((outVersion == nullptr) || (outMemBase == nullptr) || (outMemLength == nullptr)) {
    return kIOReturnBadArgument;
  }

  if (_fbReady) {
    *outVersion   = _gfxVersion;
    *outMemBase   = _gfxBase;
    *outMemLength = _gfxLength;
    HVDBGLOG("Graphics system already initialized");
    return kIOReturnSuccess;
  }

  //
  // Negotiate graphics system version.
  //
  VMBusVersion graphicsVersion;
  switch (_hvDevice->getVMBusVersion()) {
    case kVMBusVersionWIN7:
    case kVMBusVersionWS2008:
      graphicsVersion.value = kHyperVGraphicsVersionV3_0;
      break;

    case kVMBusVersionWIN8:
    case kVMBusVersionWIN8_1:
    case kVMBusVersionWIN10:
    case kVMBusVersionWIN10_V4_1: // TODO: Check if this is correct.
      graphicsVersion.value = kHyperVGraphicsVersionV3_2;
      break;

    default:
      graphicsVersion.value = kHyperVGraphicsVersionV3_5;
      break;
  }

  status = negotiateVersion(graphicsVersion);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Could not negotiate graphics version");
    return kIOReturnUnsupported;
  }
  _gfxVersion = graphicsVersion;

  //
  // Allocate graphics memory.
  //
  status = allocateGraphicsMemory(&_gfxBase, &_gfxLength);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to allocate graphics memory with status 0x%X", status);
    return status;
  }

  //
  // Send memory location to Hyper-V.
  //
  status = setGraphicsMemory(_gfxBase, _gfxLength);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to send graphics memory location with status 0x%X", status);
    return status;
  }

  //
  // Pull console info.
  //
  if (getPlatform()->getConsoleInfo(&consoleInfo) != kIOReturnSuccess) {
    HVSYSLOG("Failed to get console info");
    return kIOReturnIOError;
  }
  HVDBGLOG("Console is at 0x%X (%ux%u, bpp: %u, bytes/row: %u)",
           consoleInfo.v_baseAddr, consoleInfo.v_width, consoleInfo.v_height, consoleInfo.v_depth, consoleInfo.v_rowBytes);
  status = setScreenResolution(static_cast<UInt32>(consoleInfo.v_width), static_cast<UInt32>(consoleInfo.v_height));
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to set initial screen resolution with status 0x%X", status);
    return status;
  }

  *outVersion   = _gfxVersion;
  *outMemBase   = _gfxBase;
  *outMemLength = _gfxLength;

  //
  // Start refresh timer sending screen updates.
  //
  _timerEventSource->enable();
  _timerEventSource->setTimeoutMS(kHyperVGraphicsImageUpdateRefreshRateMS);
  _fbReady = true;

  HVDBGLOG("Graphics system initialized");
  return kIOReturnSuccess;
}

IOReturn HyperVGraphics::platformSetScreenResolution(UInt32 *inWidth, UInt32 *inHeight) {
  if ((inWidth == nullptr) || (inHeight == nullptr)) {
    return kIOReturnBadArgument;
  }
  return setScreenResolution(*inWidth, *inHeight);
}
