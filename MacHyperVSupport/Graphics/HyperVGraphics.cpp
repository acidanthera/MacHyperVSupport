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
    // Initialize work loop and command gate.
    //
    _workLoop = IOWorkLoop::workLoop();
    if (_workLoop == nullptr) {
      HVSYSLOG("Failed to create work loop");
      break;
    }
    _cmdGate = IOCommandGate::commandGate(this);
    if (_cmdGate == nullptr) {
      HVSYSLOG("Failed to create command gate");
      break;
    }
    status = _workLoop->addEventSource(_cmdGate);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to add command gate event source with status 0x%X", status);
      break;
    }

    //
    // Initialize refresh timer.
    //
    _timerEventSource = IOTimerEventSource::timerEventSource(this,
                                                             OSMemberFunctionCast(IOTimerEventSource::Action, this, &HyperVGraphics::handleRefreshTimer));
    if (_timerEventSource == nullptr) {
      HVSYSLOG("Failed to create screen refresh timer event source");
      break;
    }
    status = _workLoop->addEventSource(_timerEventSource);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to add screen refresh timer event source with status 0x%X", status);
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
    _workLoop->removeEventSource(_timerEventSource);
    OSSafeReleaseNULL(_timerEventSource);
  }
  if (_cmdGate != nullptr) {
    _workLoop->removeEventSource(_cmdGate);
    OSSafeReleaseNULL(_cmdGate);
  }
  OSSafeReleaseNULL(_workLoop);

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

  if (functionName->isEqualTo(kHyperVGraphicsPlatformFunctionInit)) {
    return platformInitGraphics(static_cast<VMBusVersion*>(param1), static_cast<IOPhysicalAddress*>(param2), static_cast<UInt32*>(param3));
  } else if (functionName->isEqualTo(kHyperVGraphicsPlatformFunctionSetResolution)) {
    return platformSetScreenResolution(static_cast<UInt32*>(param1), static_cast<UInt32*>(param2));
  } else {
    HVDBGLOG("Called unknown platform function");
  }

  return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}
