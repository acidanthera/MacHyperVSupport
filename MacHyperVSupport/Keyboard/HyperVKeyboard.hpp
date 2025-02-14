//
//  HyperVKeyboard.hpp
//  Hyper-V keyboard driver
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#ifndef HyperVKeyboard_hpp
#define HyperVKeyboard_hpp

#include <IOKit/hidsystem/IOHIKeyboard.h>

#include "HyperVKeyboardBase.hpp"
#include "HyperVVMBusDevice.hpp"
#include "HyperVKeyboardRegs.hpp"

class HyperVKeyboard : public HyperVKeyboardBase {
  OSDeclareDefaultStructors(HyperVKeyboard);
  HVDeclareLogFunctionsVMBusChild("kbd");
  typedef HyperVKeyboardBase super;

private:
  HyperVVMBusDevice *_hvDevice = nullptr;

  void handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength);
  IOReturn connectKeyboard();
  void dispatchUnicodeKeyboardEvent(UInt16 unicodeChar, bool isBreak);

public:
  //
  // IOService overrides.
  //
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#endif
