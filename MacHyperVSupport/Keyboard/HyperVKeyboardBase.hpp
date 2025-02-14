//
//  HyperVKeyboardBase.hpp
//  Hyper-V keyboard base driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#ifndef HyperVKeyboardBase_hpp
#define HyperVKeyboardBase_hpp

#include <IOKit/hidsystem/IOHIKeyboard.h>

#include "HyperV.hpp"

class HyperVKeyboardBase : public IOHIKeyboard {
  OSDeclareAbstractStructors(HyperVKeyboardBase);
  typedef IOHIKeyboard super;

protected:
  //
  // IOHIKeyboard overrides.
  //
  virtual const unsigned char * defaultKeymapOfLength(UInt32 * length) APPLE_KEXT_OVERRIDE;
  virtual UInt32 maxKeyCodes() APPLE_KEXT_OVERRIDE;

public:
  //
  // IOHIKeyboard overrides.
  //
  virtual UInt32 deviceType() APPLE_KEXT_OVERRIDE;
  virtual UInt32 interfaceID() APPLE_KEXT_OVERRIDE;
};

#endif
