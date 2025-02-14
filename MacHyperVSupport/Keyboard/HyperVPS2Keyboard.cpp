//
//  HyperVPS2Keyboard.cpp
//  Hyper-V PS/2 keyboard driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "HyperVPS2Keyboard.hpp"
#include "HyperVPS2KeyboardRegs.hpp"
#include "HyperVADBMaps.hpp"

#include <architecture/i386/pio.h>

OSDefineMetaClassAndStructors(HyperVPS2Keyboard, super);

bool HyperVPS2Keyboard::start(IOService *provider) {
  bool     result = false;
  IOReturn status;

  //
  // Get parent ACPI platform device object.
  //
  _acpiDevice = OSDynamicCast(IOACPIPlatformDevice, provider);
  if (_acpiDevice == nullptr) {
    HVSYSLOG("Provider is not IOACPIPlatformDevice");
    return false;
  }
  _acpiDevice->retain();

  HVCheckDebugArgs();
  HVDBGLOG("Initializing Hyper-V PS/2 Keyboard");

  if (HVCheckOffArg()) {
    HVSYSLOG("Disabling Hyper-V PS/2 Keyboard due to boot arg");
    OSSafeReleaseNULL(_acpiDevice);
    return false;
  }

  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    OSSafeReleaseNULL(_acpiDevice);
    return false;
  }

  do {
    //
    // Configure work loop and interrupt handler.
    //
    _workLoop = IOWorkLoop::workLoop();
    if (_workLoop == nullptr) {
      break;
    }
    _intEventSource = IOInterruptEventSource::interruptEventSource(this, OSMemberFunctionCast(IOInterruptEventAction, this, &HyperVPS2Keyboard::interruptOccurred), _acpiDevice, 0);
    if (_intEventSource == nullptr) {
      break;
    }
    _workLoop->addEventSource(_intEventSource);
    _intEventSource->enable();

    //
    // Connect to PS/2 interface.
    //
    status = connectPS2Keyboard();
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to connect to PS/2 keyboard device with status 0x%X", status);
      break;
    }

    HVDBGLOG("Initialized Hyper-V PS/2 Keyboard");
    result = true;
  } while (false);
  
  

  if (!result) {
    stop(provider);
  }
  return result;
}

void HyperVPS2Keyboard::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V PS/2 Keyboard");

  if (_intEventSource != nullptr) {
    _workLoop->removeEventSource(_intEventSource);
    OSSafeReleaseNULL(_intEventSource);
  }
  OSSafeReleaseNULL(_workLoop);
  OSSafeReleaseNULL(_acpiDevice);

  super::stop(provider);
}

inline UInt32 getKeyCode(UInt8 makeCode, bool isE0) {
  return PS2ToADBMapStock[makeCode + (isE0 ? kADBConverterExStart : 0)];
}

void HyperVPS2Keyboard::interruptOccurred(OSObject *owner, IOInterruptEventSource *sender, int count) {
  UInt8 data;
  UInt64 time;
  bool isBreak;
  bool isE0;

  //
  // Read inbound keycode.
  //
  data = inb(kHyperVPS2DataPort);

  //
  // Check if E0, read second keycode of keystroke.
  //
  isE0 = _keystrokeE0;
  if (_keystrokeE0) {
    _keystrokeE0 = false;
  } else {
    _keystrokeE0 = data == kHyperVPS2KeyboardE0;
    if (_keystrokeE0) {
      return;
    }
  }

  //
  // Check if break code.
  //
  isBreak = (data & kHyperVPS2KeyboardScancodeBreak) != 0;
  if (isBreak) {
    data &= ~(kHyperVPS2KeyboardScancodeBreak);
  }

  //
  // Dispatch to HID system.
  //
  clock_get_uptime(&time);
  dispatchKeyboardEvent(getKeyCode(data, isE0), !isBreak, *(AbsoluteTime*)&time);
}

IOReturn HyperVPS2Keyboard::connectPS2Keyboard() {
  IOReturn status;

  //
  // Reset PS/2 controller and keyboard.
  //
  status = resetPS2Controller();
  if (status != kIOReturnSuccess) {
    return status;
  }
  status = resetPS2Keyboard();
  if (status != kIOReturnSuccess) {
    return status;
  }

  return kIOReturnSuccess;
}

void HyperVPS2Keyboard::writeCommandPort(UInt8 byte) {
  while (inb(kHyperVPS2CommandPort) & kHyperVPS2StatusInputBusy) {
    IODelay(kHyperVPS2Delay);
  }
  IODelay(kHyperVPS2Delay);
  outb(kHyperVPS2CommandPort, byte);
}

bool HyperVPS2Keyboard::readDataPort(UInt8 *byte) {
  UInt32 timeout = 20000;
  while ((timeout > 0) && !(inb(kHyperVPS2CommandPort) & kHyperVPS2StatusOutputBusy)) {
    timeout--;
    IODelay(kHyperVPS2Delay);
  }
  if (timeout == 0) {
    HVSYSLOG("Timeout while reading from data port");
    return false;
  }

  *byte = inb(kHyperVPS2DataPort);
  return true;
}

void HyperVPS2Keyboard::writeDataPort(UInt8 byte) {
  while (inb(kHyperVPS2CommandPort) & kHyperVPS2StatusInputBusy) {
    IODelay(kHyperVPS2Delay);
  }
  IODelay(kHyperVPS2Delay);
  outb(kHyperVPS2DataPort, byte);
}

void HyperVPS2Keyboard::flushDataPort() {
  while (inb(kHyperVPS2CommandPort) & kHyperVPS2StatusOutputBusy) {
    IODelay(kHyperVPS2Delay);
    inb(kHyperVPS2DataPort);
    IODelay(kHyperVPS2Delay);
  }
}

IOReturn HyperVPS2Keyboard::resetPS2Controller() {
  UInt8 data;

  //
  // Disable both PS/2 devices.
  //
  writeCommandPort(kHyperVPS2CommandDisableKeyboardPort);
  writeCommandPort(kHyperVPS2CommandDisableMousePort);
  flushDataPort();

  //
  // Disable all devices on controller.
  //
  writeCommandPort(kHyperVPS2CommandReadByte);
  if (!readDataPort(&data)) {
    return kIOReturnTimeout;
  }
  HVDBGLOG("Read PS/2 configuration: 0x%X", data);
  data &= ~(kHyperVPS2ConfigEnableKeyboardIRQ | kHyperVPS2ConfigEnableMouseIRQ | kHyperVPS2ConfigKeyboardDisableClock);
  data |= kHyperVPS2ConfigMouseDisableClock;
  writeCommandPort(kHyperVPS2CommandWriteByte);
  writeDataPort(data);
  HVDBGLOG("Wrote new PS/2 configuration: 0x%X", data);

  //
  // Self-test controller.
  //
  writeCommandPort(kHyperVPS2CommandTestController);
  if (!readDataPort(&data)) {
    return kIOReturnTimeout;
  }
  if (data != kHyperVPS2SelfTestPassResult) {
    HVSYSLOG("PS/2 controller failed self-test: 0x%X", data);
    return kIOReturnIOError;
  }
  HVDBGLOG("PS/2 controller passed self-test");

  //
  // Self-test keyboard port.
  //
  writeCommandPort(kHyperVPS2CommandTestKeyboardPort);
  if (!readDataPort(&data)) {
    return kIOReturnTimeout;
  }
  if (data != kHyperVPS2KeyboardPortPassResult) {
    HVSYSLOG("PS/2 keyboard port failed self-test: 0x%X", data);
    return kIOReturnIOError;
  }
  HVDBGLOG("PS/2 keyboard port passed self-test");

  //
  // Enable keyboard device only, no need to support the PS/2 mouse.
  // The synthetic mouse device is supported on all versions of Hyper-V.
  //
  writeCommandPort(kHyperVPS2CommandEnableKeyboardPort);
  writeCommandPort(kHyperVPS2CommandReadByte);
  if (!readDataPort(&data)) {
    return kIOReturnTimeout;
  }
  HVDBGLOG("Read PS/2 configuration: 0x%X", data);
  data |= kHyperVPS2ConfigEnableKeyboardIRQ;
  writeCommandPort(kHyperVPS2CommandWriteByte);
  writeDataPort(data);
  HVDBGLOG("Wrote new PS/2 configuration: 0x%X", data);

  HVDBGLOG("PS/2 controller reset successfully");
  return kIOReturnSuccess;
}

IOReturn HyperVPS2Keyboard::resetPS2Keyboard() {
  UInt8 data;

  //
  // Reset to defaults and enable keyboard.
  //
  writeDataPort(kHyperVPS2KeyboardCommandSetDefaults);
  if (!readDataPort(&data)) {
    return kIOReturnTimeout;
  }
  if (data != kHyperVPS2KeyboardAck) {
    HVSYSLOG("Keyboard device returned invalid ack value 0x%X", data);
    return kIOReturnIOError;
  }
  writeDataPort(kHyperVPS2KeyboardCommandEnable);
  if (!readDataPort(&data)) {
    return kIOReturnTimeout;
  }
  if (data != kHyperVPS2KeyboardAck) {
    HVSYSLOG("Keyboard device returned invalid ack value 0x%X", data);
    return kIOReturnIOError;
  }

  HVDBGLOG("PS/2 keyboard device reset successfully");
  return kIOReturnSuccess;
}
