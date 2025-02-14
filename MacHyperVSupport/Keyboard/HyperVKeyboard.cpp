//
//  HyperVKeyboard.cpp
//  Hyper-V keyboard driver
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#include "HyperVKeyboard.hpp"
#include "HyperVADBMaps.hpp"

OSDefineMetaClassAndStructors(HyperVKeyboard, super);

bool HyperVKeyboard::start(IOService *provider) {
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
  HVDBGLOG("Initializing Hyper-V Synthetic Keyboard");

  if (HVCheckOffArg()) {
    HVSYSLOG("Disabling Hyper-V Synthetic Keyboard due to boot arg");
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
    // Install packet handler.
    //
    status = _hvDevice->installPacketActions(this, OSMemberFunctionCast(HyperVVMBusDevice::PacketReadyAction, this, &HyperVKeyboard::handlePacket),
                                             nullptr, kHyperVKeyboardResponsePacketSize);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to install packet handler with status 0x%X", status);
      break;
    }

    //
    // Open VMBus channel and connect to keyboard.
    //
    status = _hvDevice->openVMBusChannel(kHyperVKeyboardRingBufferSize, kHyperVKeyboardRingBufferSize);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to open VMBus channel with status 0x%X", status);
      break;
    }

    status = connectKeyboard();
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to connect to keyboard device with status 0x%X", status);
      break;
    }

    HVDBGLOG("Initialized Hyper-V Synthetic Keyboard");
    result = true;
  } while (false);

  if (!result) {
    stop(provider);
  }
  return result;
}

void HyperVKeyboard::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V Synthetic Keyboard");

  if (_hvDevice != nullptr) {
    _hvDevice->closeVMBusChannel();
    _hvDevice->uninstallPacketActions();
    OSSafeReleaseNULL(_hvDevice);
  }

  super::stop(provider);
}

IOReturn HyperVKeyboard::connectKeyboard() {
  HVDBGLOG("Connecting to keyboard interface");

  HyperVKeyboardMessageProtocolRequest requestMsg;
  requestMsg.header.type      = kHyperVKeyboardMessageTypeProtocolRequest;
  requestMsg.versionRequested = kHyperVKeyboardVersion;

  return _hvDevice->writeInbandPacket(&requestMsg, sizeof (requestMsg), true);
}

void HyperVKeyboard::dispatchUnicodeKeyboardEvent(UInt16 unicodeChar, bool isBreak) {
  UInt16 keyCode;
  UInt64 time;

  //
  // Ignore break codes as the simulated key goes down and back up within this function.
  //
  if (isBreak) {
    return;
  }

  if (unicodeChar >= arrsize(UnicodeToADBMap)) {
    HVDBGLOG("Unknown Unicode character 0x%X break: %u", unicodeChar, isBreak);
    return;
  }
  keyCode = UnicodeToADBMap[unicodeChar];
  HVDBGLOG("Handling Unicode character 0x%X keycode: 0x%X shift: %u", unicodeChar,
           keyCode, (keyCode & kADBUnicodeShift) ? 1 : 0);

  //
  // Simulate shift key press for shifted characters.
  //
  if (keyCode & kADBUnicodeShift) {
    clock_get_uptime(&time);
    dispatchKeyboardEvent(kADBKeyCodeShift, true, *(AbsoluteTime*)&time);
  }

  //
  // Simulate key press and release for character.
  //
  clock_get_uptime(&time);
  dispatchKeyboardEvent((UInt8)keyCode, true, *(AbsoluteTime*)&time);
  clock_get_uptime(&time);
  dispatchKeyboardEvent((UInt8)keyCode, false, *(AbsoluteTime*)&time);

  //
  // Simulate shift key release for shifted characters.
  //
  if (keyCode & kADBUnicodeShift) {
    clock_get_uptime(&time);
    dispatchKeyboardEvent(kADBKeyCodeShift, false, *(AbsoluteTime*)&time);
  }
}

inline UInt32 getKeyCode(HyperVKeyboardMessageKeystroke *keyEvent) {
  return PS2ToADBMapStock[keyEvent->makeCode + (keyEvent->isE0 ? kADBConverterExStart : 0)];
}

void HyperVKeyboard::handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  HyperVKeyboardMessage *keyboardMsg = (HyperVKeyboardMessage*) pktData;
  UInt64                time;

  switch (keyboardMsg->header.type) {
    case kHyperVKeyboardMessageTypeProtocolResponse:
      HVDBGLOG("Keyboard protocol status %u %u", keyboardMsg->protocolResponse.header.type, keyboardMsg->protocolResponse.status);
      break;

    case kHyperVKeyboardMessageTypeEvent:
      HVDBGLOG("Got make code 0x%X (E0: %u, E1: %u, break: %u, Unicode: %u)", keyboardMsg->keystroke.makeCode,
               keyboardMsg->keystroke.isE0, keyboardMsg->keystroke.isE1, keyboardMsg->keystroke.isBreak, keyboardMsg->keystroke.isUnicode);
      if (keyboardMsg->keystroke.isUnicode) {
        dispatchUnicodeKeyboardEvent(keyboardMsg->keystroke.makeCode, keyboardMsg->keystroke.isBreak);
      } else {
        clock_get_uptime(&time);
        dispatchKeyboardEvent(getKeyCode(&keyboardMsg->keystroke), !keyboardMsg->keystroke.isBreak, *(AbsoluteTime*)&time);
      }
      break;

    default:
      HVDBGLOG("Unknown message type %u, size %u", keyboardMsg->header.type, pktDataLength);
      break;
  }
}
