//
//  HyperVKeyboard.hpp
//  Hyper-V keyboard driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVKeyboard_hpp
#define HyperVKeyboard_hpp

#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/hidsystem/IOHIKeyboard.h>

#include "HyperVVMBusDevice.hpp"
#include "HyperVKeyboardRegs.hpp"

#define super IOHIKeyboard

#define HVSYSLOG(str, ...) HVSYSLOG_PRINT("HyperVKeyboard", true, hvDevice->getChannelId(), str, ## __VA_ARGS__)
#define HVDBGLOG(str, ...) \
  if (this->debugEnabled) HVDBGLOG_PRINT("HyperVKeyboard", true, hvDevice->getChannelId(), str, ## __VA_ARGS__)

class HyperVKeyboard : public IOHIKeyboard {
  OSDeclareDefaultStructors(HyperVKeyboard);

private:
  HyperVVMBusDevice       *hvDevice;
  IOInterruptEventSource  *interruptSource;
  bool                    debugEnabled = false;
  
  void handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count);
  bool connectKeyboard();
  
protected:
  virtual const unsigned char * defaultKeymapOfLength(UInt32 * length) APPLE_KEXT_OVERRIDE;
  virtual UInt32 maxKeyCodes() APPLE_KEXT_OVERRIDE;
  
public:
  //
  // IOService overrides.
  //
  virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  
  //
  // IOHIKeyboard overrides.
  //
  virtual UInt32 deviceType() APPLE_KEXT_OVERRIDE;
  virtual UInt32 interfaceID() APPLE_KEXT_OVERRIDE;
};

#endif
