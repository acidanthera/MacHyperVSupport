//
//  HyperVGraphics.cpp
//  Hyper-V synthetic graphics driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "HyperVGraphics.hpp"
#include "HyperVGraphicsPlatformFunctions.hpp"

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
    registerService();

    //
    // Start refresh timer sending screen updates.
    //
    _timerEventSource->enable();
    _timerEventSource->setTimeoutMS(kHyperVGraphicsDIRTRefreshRateMS);
    _fbReady = true;

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

  if (_hvDevice != nullptr) {
    _hvDevice->closeVMBusChannel();
    _hvDevice->uninstallPacketActions();
    OSSafeReleaseNULL(_hvDevice);
  }

  super::stop(provider);
}

IOReturn HyperVGraphics::callPlatformFunction(const OSSymbol *functionName, bool waitForFunction,
                                              void *param1, void *param2, void *param3, void *param4) {
  HVDBGLOG("Attempting to call function '%s'", functionName->getCStringNoCopy());

  if (functionName->isEqualTo(kHyperVGraphicsFunctionSetResolution)) {
    return updateScreenResolution(*((UInt32*)param1), *((UInt32*)param2), false);
  } else if (functionName->isEqualTo(kHyperVGraphicsFunctionSetCursor)) {
    HyperVGraphicsFunctionSetCursorParams *cursorParams = static_cast<HyperVGraphicsFunctionSetCursorParams*>(param1);
    if (cursorParams == nullptr) {
      return kIOReturnBadArgument;
    }
    return updateCursorShape(cursorParams->cursorData, cursorParams->width, cursorParams->height,
                             cursorParams->hotX, cursorParams->hotY);
  } else {
    return kIOReturnUnsupported;
  }
}
