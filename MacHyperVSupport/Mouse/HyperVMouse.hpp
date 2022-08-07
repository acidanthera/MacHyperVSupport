//
//  HyperVMouse.hpp
//  Hyper-V mouse driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVMouse_hpp
#define HyperVMouse_hpp

#include <IOKit/hid/IOHIDDevice.h>

#include "HyperVVMBusDevice.hpp"
#include "HyperVMouseRegs.hpp"

class HyperVMouse : public IOHIDDevice {
  OSDeclareDefaultStructors(HyperVMouse);
  HVDeclareLogFunctionsVMBusChild("mous");
  typedef IOHIDDevice super;

private:
  //
  // Parent VMBus device.
  //
  HyperVVMBusDevice       *hvDevice         = nullptr;
  IOInterruptEventSource  *interruptSource  = nullptr;

  //
  // HID structures.
  //
  HyperVMouseDeviceInfo   mouseInfo           = { };
  void                    *hidDescriptor      = nullptr;
  size_t                  hidDescriptorLength = 0;

  void freeStructures();
  void handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count);

  bool setupMouse();
  void handleProtocolResponse(HyperVMouseMessageProtocolResponse *response);
  void handleDeviceInfo(HyperVMouseMessageInitialDeviceInfo *deviceInfo);
  void handleInputReport(HyperVMouseMessageInputReport *inputReport);

protected:
  //
  // IOHIDDevice overrides.
  //
  bool handleStart(IOService *provider) APPLE_KEXT_OVERRIDE;
  void handleStop(IOService *provider) APPLE_KEXT_OVERRIDE;

public:  
  //
  // IOHIDDevice overrides.
  //
  OSString *newTransportString() const APPLE_KEXT_OVERRIDE;
  OSString *newManufacturerString() const APPLE_KEXT_OVERRIDE;
  OSString *newProductString() const APPLE_KEXT_OVERRIDE;
  OSNumber *newVendorIDNumber() const APPLE_KEXT_OVERRIDE;
  OSNumber *newProductIDNumber() const APPLE_KEXT_OVERRIDE;
  OSNumber *newVersionNumber() const APPLE_KEXT_OVERRIDE;

  IOReturn newReportDescriptor(IOMemoryDescriptor **descriptor) const APPLE_KEXT_OVERRIDE;
};

#endif /* HyperVMouse_hpp */
