//
//  HyperVGraphics.cpp
//  Hyper-V synthetic graphics driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "HyperVGraphics.hpp"

OSDefineMetaClassAndStructors(HyperVGraphics, super);

bool HyperVGraphics::start(IOService *provider) {
  bool     result = false;
  IOReturn status;

  //
  // Get parent VMBus device object.
  //
  _hvDevice = OSDynamicCast(HyperVVMBusDevice, provider);
  if (_hvDevice == nullptr) {
    HVSYSLOG("Provider is not HyperVVMBusDevice");
    return false;
  }
  _hvDevice->retain();

  HVCheckDebugArgs();
  HVDBGLOG("Initializing Hyper-V Synthetic Graphics");

  if (HVCheckOffArg()) {
    HVSYSLOG("Disabling Hyper-V Synthetic Graphics due to boot arg");
    OSSafeReleaseNULL(_hvDevice);
    return false;
  }

  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    OSSafeReleaseNULL(_hvDevice);
    return false;
  }

  do {
    //
    // Initialize refresh timer.
    //
    _timerEventSource = IOTimerEventSource::timerEventSource(this,
                                                             OSMemberFunctionCast(IOTimerEventSource::Action, this, &HyperVGraphics::handleRefreshTimer));
    if (_timerEventSource == nullptr) {
      HVSYSLOG("Failed to create screen refresh timer event source");
      break;
    }
    status = getWorkLoop()->addEventSource(_timerEventSource);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to add screen refresh timer event source");
      break;
    }

    _gfxMsgCursorShapeLock = IOSimpleLockAlloc();
    if (_gfxMsgCursorShapeLock == nullptr) {
      HVSYSLOG("Failed to allocate cursor shape lock");
      break;
    }

    //
    // Install packet handler.
    //
    status = _hvDevice->installPacketActions(this, OSMemberFunctionCast(HyperVVMBusDevice::PacketReadyAction, this, &HyperVGraphics::handlePacket),
                                             nullptr, kHyperVGraphicsMaxPacketSize);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to install packet handler with status 0x%X", status);
      break;
    }

    //
    // Open VMBus channel and connect to graphics system.
    //
    status = _hvDevice->openVMBusChannel(kHyperVGraphicsRingBufferSize, kHyperVGraphicsRingBufferSize);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to open VMBus channel with status 0x%X", status);
      break;
    }

    status = connectGraphics();
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to connect to graphics device with status 0x%X", status);
      break;
    }

    //
    // Start refresh timer sending screen updates.
    //
    _timerEventSource->enable();
    _timerEventSource->setTimeoutMS(kHyperVGraphicsDIRTRefreshRateMS);
    _fbReady = true;

    registerService();
    HVDBGLOG("Initialized Hyper-V Synthetic Graphics");
    result = true;
  } while (false);

  if (!result) {
    stop(provider);
  }
  return result;
}

void HyperVGraphics::stop(IOService *provider) {
  HVDBGLOG("Hyper-V Synthetic Graphics is stopping");

  if (_timerEventSource != nullptr) {
    _timerEventSource->disable();
    OSSafeReleaseNULL(_timerEventSource);
  }

  if (_gfxMsgCursorShape != nullptr) {
    IOFree(_gfxMsgCursorShape, _gfxMsgCursorShapeSize);
    _gfxMsgCursorShape = nullptr;
  }
  if (_gfxMsgCursorShapeLock != nullptr) {
    IOSimpleLockFree(_gfxMsgCursorShapeLock);
    _gfxMsgCursorShapeLock = nullptr;
  }

  if (_hvDevice != nullptr) {
    _hvDevice->closeVMBusChannel();
    _hvDevice->uninstallPacketActions();
    OSSafeReleaseNULL(_hvDevice);
  }

  super::stop(provider);
}

IOReturn HyperVGraphics::callPlatformFunction(const OSSymbol *functionName, bool waitForFunction,
                                              void *param1, void *param2, void *param3, void *param4) {
  HVDBGLOG("Attempting to call platform function '%s'", functionName->getCStringNoCopy());

  // Get graphics version.
  if (functionName->isEqualTo(kHyperVGraphicsFunctionGetVersion)) {
    if (param1 == nullptr) {
      return kIOReturnBadArgument;
    }
    memcpy(param1, &_gfxVersion, sizeof (_gfxVersion));
    return kIOReturnSuccess;

  // Get graphics memory.
  } else if (functionName->isEqualTo(kHyperVGraphicsFunctionGetMemory)) {
    if (param1 == nullptr) {
      return kIOReturnBadArgument;
    }
    HyperVGraphicsFunctionGetMemoryResults *memResults = static_cast<HyperVGraphicsFunctionGetMemoryResults*>(param1);
    memResults->base = _gfxBase;
    memResults->length = _gfxLength;
    return kIOReturnSuccess;

  // Set resolution.
  } else if (functionName->isEqualTo(kHyperVGraphicsFunctionSetResolution)) {
    return updateScreenResolution(*((UInt32*)param1), *((UInt32*)param2), false);

  // Set hardware cursor.
  } else if (functionName->isEqualTo(kHyperVGraphicsFunctionSetCursor)) {
    HyperVGraphicsFunctionSetCursorParams *cursorParams = static_cast<HyperVGraphicsFunctionSetCursorParams*>(param1);
    if (cursorParams == nullptr) {
      return kIOReturnBadArgument;
    }
    return updateCursorShape(cursorParams->cursorData, cursorParams->width, cursorParams->height,
                             cursorParams->hotX, cursorParams->hotY);

  } else if (functionName->isEqualTo(kHyperVGraphicsFunctionSetCusorPosition)) {
    return updateCursorPosition(*((SInt32*)param1), *((SInt32*)param2), *((bool*)param3));
  } else {
    return kIOReturnUnsupported;
  }
}
