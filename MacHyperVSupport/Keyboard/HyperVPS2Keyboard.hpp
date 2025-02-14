//
//  HyperVPS2Keyboard.hpp
//  Hyper-V PS/2 keyboard driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#ifndef HyperVPS2Keyboard_hpp
#define HyperVPS2Keyboard_hpp

#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include <IOKit/IOInterruptEventSource.h>

#include "HyperVKeyboardBase.hpp"

class HyperVPS2Keyboard : public HyperVKeyboardBase {
  OSDeclareDefaultStructors(HyperVPS2Keyboard);
  HVDeclareLogFunctions("ps2kbd");
  typedef HyperVKeyboardBase super;

private:
  IOACPIPlatformDevice    *_acpiDevice = nullptr;
  IOWorkLoop              *_workLoop = nullptr;
  IOInterruptEventSource  *_intEventSource = nullptr;
  bool                    _keystrokeE0 = false;

  void interruptOccurred(OSObject *owner, IOInterruptEventSource *sender, int count);
  IOReturn connectPS2Keyboard();
  void writeCommandPort(UInt8 byte);
  bool readDataPort(UInt8 *byte);
  void writeDataPort(UInt8 byte);
  void flushDataPort();
  IOReturn resetPS2Controller();
  IOReturn resetPS2Keyboard();

public:
  //
  // IOService overrides.
  //
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#endif
