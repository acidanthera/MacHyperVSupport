//
//  HyperVMouse.hpp
//  Hyper-V mouse driver
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#ifndef HyperVMouse_hpp
#define HyperVMouse_hpp

#include <IOKit/hid/IOHIDDevice.h>

#include "HyperVVMBusDevice.hpp"
#include "HyperVMouseRegs.hpp"

class HyperVMouse : public IOHIDDevice {
  OSDeclareDefaultStructors(HyperVMouse);
  HVDeclareLogFunctionsVMBusChild("mouse");
  typedef IOHIDDevice super;

private:
  HyperVVMBusDevice *_hvDevice = nullptr;

  //
  // HID structures.
  //
  HyperVMouseDeviceInfo _mouseInfo           = { };
  void                  *_hidDescriptor      = nullptr;
  size_t                _hidDescriptorLength = 0;

  void handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength);
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

#endif
