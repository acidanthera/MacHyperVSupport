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

#define super IOHIDDevice

#define HVSYSLOG(str, ...) HVSYSLOG_PRINT("HyperVMouse", true, hvDevice->getChannelId(), str, ## __VA_ARGS__)
#define HVDBGLOG(str, ...) \
  if (this->debugEnabled) HVDBGLOG_PRINT("HyperVMouse", true, hvDevice->getChannelId(), str, ## __VA_ARGS__)

class HyperVMouse : public IOHIDDevice {
  OSDeclareDefaultStructors(HyperVMouse);

private:
  //
  // Parent VMBus device.
  //
  HyperVVMBusDevice       *hvDevice;
  IOInterruptEventSource  *interruptSource;
  bool                    debugEnabled = false;

  //
  // HID structures.
  //
  HyperVMouseDeviceInfo   mouseInfo;
  void                    *hidDescriptor;
  size_t                  hidDescriptorLength;

  void handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count);

  bool setupMouse();
  void handleProtocolResponse(HyperVMouseMessageProtocolResponse *response);
  void handleDeviceInfo(HyperVMouseMessageInitialDeviceInfo *deviceInfo);
  void handleInputReport(HyperVMouseMessageInputReport *inputReport);

protected:
  //
  // IOHIDDevice overrides.
  //
  virtual bool handleStart(IOService *provider) APPLE_KEXT_OVERRIDE;
  virtual void handleStop(IOService *provider) APPLE_KEXT_OVERRIDE;

public:  
  //
  // IOHIDDevice overrides.
  //
  virtual OSString *newTransportString() const APPLE_KEXT_OVERRIDE;
  virtual OSString *newManufacturerString() const APPLE_KEXT_OVERRIDE;
  virtual OSString *newProductString() const APPLE_KEXT_OVERRIDE;
  virtual OSNumber *newVendorIDNumber() const APPLE_KEXT_OVERRIDE;
  virtual OSNumber *newProductIDNumber() const APPLE_KEXT_OVERRIDE;
  virtual OSNumber *newVersionNumber() const APPLE_KEXT_OVERRIDE;

  virtual IOReturn newReportDescriptor(IOMemoryDescriptor **descriptor) const APPLE_KEXT_OVERRIDE;
};

#endif /* HyperVMouse_hpp */
