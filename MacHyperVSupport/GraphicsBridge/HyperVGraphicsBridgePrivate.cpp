//
//  HyperVGraphicsBridgePrivate.cpp
//  Hyper-V synthetic graphics bridge
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVGraphicsBridge.hpp"

static const VMBusVersion graphicsVersions[] = {
  kHyperVGraphicsVersionV3_2,
  kHyperVGraphicsVersionV3_0
};

void HyperVGraphicsBridge::fillFakePCIDeviceSpace() {
  //
  // Fill PCI device config space.
  //
  // PCI bridge will contain a single PCI graphics device
  // with the framebuffer memory at BAR0. The vendor/device ID is
  // the same as what a generation 1 Hyper-V VM uses for the
  // emulated graphics.
  //
  bzero(_fakePCIDeviceSpace, sizeof (_fakePCIDeviceSpace));

  OSWriteLittleInt16(_fakePCIDeviceSpace, kIOPCIConfigVendorID, kHyperVPCIVendorMicrosoft);
  OSWriteLittleInt16(_fakePCIDeviceSpace, kIOPCIConfigDeviceID, kHyperVPCIDeviceHyperVVideo);
  OSWriteLittleInt32(_fakePCIDeviceSpace, kIOPCIConfigRevisionID, 0x3000000);
  OSWriteLittleInt16(_fakePCIDeviceSpace, kIOPCIConfigSubSystemVendorID, kHyperVPCIVendorMicrosoft);
  OSWriteLittleInt16(_fakePCIDeviceSpace, kIOPCIConfigSubSystemID, kHyperVPCIDeviceHyperVVideo);

  OSWriteLittleInt32(_fakePCIDeviceSpace, kIOPCIConfigBaseAddress0, (UInt32)_consoleInfo.v_baseAddr);
}
