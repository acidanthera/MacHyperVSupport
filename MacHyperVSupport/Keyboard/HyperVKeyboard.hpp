//
//  HyperVKeyboard.hpp
//  Hyper-V keyboard driver
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#ifndef HyperVKeyboard_hpp
#define HyperVKeyboard_hpp

#include <IOKit/hidsystem/IOHIKeyboard.h>

#include "HyperVVMBusDevice.hpp"
#include "HyperVKeyboardRegs.hpp"

class HyperVKeyboard : public IOHIKeyboard {
  OSDeclareDefaultStructors(HyperVKeyboard);
  HVDeclareLogFunctionsVMBusChild("kbd");
  typedef IOHIKeyboard super;

private:
  HyperVVMBusDevice *_hvDevice = nullptr;

  void handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength);
  IOReturn connectKeyboard();
  void dispatchUnicodeKeyboardEvent(UInt16 unicodeChar, bool isBreak);

protected:
  //
  // IOHIKeyboard overrides.
  //
  const unsigned char * defaultKeymapOfLength(UInt32 * length) APPLE_KEXT_OVERRIDE;
  UInt32 maxKeyCodes() APPLE_KEXT_OVERRIDE;

public:
  //
  // IOService overrides.
  //
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  void stop(IOService *provider) APPLE_KEXT_OVERRIDE;

  //
  // IOHIKeyboard overrides.
  //
  UInt32 deviceType() APPLE_KEXT_OVERRIDE;
  UInt32 interfaceID() APPLE_KEXT_OVERRIDE;
};

#endif
