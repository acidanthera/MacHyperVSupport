//
//  HyperVGraphicsBridge.cpp
//  Hyper-V synthetic graphics bridge
//
//  Copyright © 2021-2025 Goldfish64. All rights reserved.
//

#include "HyperVGraphicsBridge.hpp"
#include "HyperVGraphics.hpp"
#include "HyperVPCIRoot.hpp"

OSDefineMetaClassAndStructors(HyperVGraphicsBridge, super);

bool HyperVGraphicsBridge::start(IOService *provider) {
  PE_Video consoleInfo = { };
  HyperVPCIRoot *hvPCIRoot;
  IOReturn status;

  HVCheckDebugArgs();
  HVDBGLOG("Initializing Hyper-V Synthetic Graphics Bridge");

  if (HVCheckOffArg()) {
    HVSYSLOG("Disabling Hyper-V Synthetic Graphics Bridge due to boot arg");
    return false;
  }

  //
  // Pull console info.
  //
  if (getPlatform()->getConsoleInfo(&consoleInfo) != kIOReturnSuccess) {
    HVSYSLOG("Failed to get console info");
    return false;
  }
  HVDBGLOG("Console is at 0x%X (%ux%u, bpp: %u, bytes/row: %u)",
         consoleInfo.v_baseAddr, consoleInfo.v_height, consoleInfo.v_width, consoleInfo.v_depth, consoleInfo.v_rowBytes);
  _fbInitialBase   = (UInt32)consoleInfo.v_baseAddr;
  _fbInitialLength = (UInt32)(consoleInfo.v_height * consoleInfo.v_rowBytes);

  //
  // Ensure parent is HyperVGraphics object.
  //
  if (OSDynamicCast(HyperVGraphics, provider) == nullptr) {
    HVSYSLOG("Provider is not HyperVGraphics");
    return false;
  }

  //
  // Locate root PCI bus instance.
  //
  hvPCIRoot = HyperVPCIRoot::getPCIRootInstance();
  if (hvPCIRoot == nullptr) {
    HVSYSLOG("Failed to find root PCI bridge instance");
    return false;
  }

  //
  // Do not start on Gen1 VMs.
  //
  if (!hvPCIRoot->isHyperVGen2()) {
    HVDBGLOG("Not starting on Hyper-V Gen1 VM");
    return false;
  }

  //
  // Allocate PCI lock and register with root PCI bridge.
  //
  _pciLock = IOSimpleLockAlloc();
  if (_pciLock == nullptr) {
    HVSYSLOG("Failed to allocate PCI lock");
    return false;
  }

  status = hvPCIRoot->registerChildPCIBridge(this, &_pciBusNumber);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to register with root PCI bus instance");
    IOSimpleLockFree(_pciLock);
    return false;
  }

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
  OSWriteLittleInt32(_fakePCIDeviceSpace, kIOPCIConfigBaseAddress0, (UInt32)_fbInitialBase);

  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    IOSimpleLockFree(_pciLock);
    return false;
  }

  HVDBGLOG("Initialized Hyper-V Synthetic Graphics Bridge");
  return true;
}

void HyperVGraphicsBridge::stop(IOService *provider) {
  HVDBGLOG("Hyper-V Synthetic Graphics Bridge is stopping");

  if (_pciLock != nullptr) {
    IOSimpleLockFree(_pciLock);
  }

  super::stop(provider);
}

bool HyperVGraphicsBridge::configure(IOService *provider) {
  //
  // Add framebuffer memory range to bridge.
  //
  HVDBGLOG("Adding framebuffer memory 0x%X length 0x%X to PCI bridge", _fbInitialBase, _fbInitialLength);
  addBridgeMemoryRange(_fbInitialBase, _fbInitialLength, true);
  return super::configure(provider);
}

UInt32 HyperVGraphicsBridge::configRead32(IOPCIAddressSpace space, UInt8 offset) {
  UInt32 data;
  IOInterruptState ints;

  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return 0xFFFFFFFF;
  }

  ints = IOSimpleLockLockDisableInterrupt(_pciLock);
  data = OSReadLittleInt32(_fakePCIDeviceSpace, offset);
  IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);

  HVDBGLOG("Read 32-bit value %u from offset 0x%X", data, offset);
  return data;
}

void HyperVGraphicsBridge::configWrite32(IOPCIAddressSpace space, UInt8 offset, UInt32 data) {
  IOInterruptState ints;
  
  if (space.es.deviceNum != 0 || space.es.functionNum != 0
      || (offset > kIOPCIConfigurationOffsetBaseAddress0 && offset <= kIOPCIConfigurationOffsetBaseAddress5)
      || offset == kIOPCIConfigurationOffsetExpansionROMBase) {
    return;
  }
  HVDBGLOG("Writing 32-bit value %u to offset 0x%X", data, offset);

  //
  // Return BAR0 size if requested.
  //
  if (offset == kIOPCIConfigurationOffsetBaseAddress0 && data == 0xFFFFFFFF) {
    OSWriteLittleInt32(_fakePCIDeviceSpace, offset, (0xFFFFFFFF - _fbInitialLength) + 1);
    return;
  }

  ints = IOSimpleLockLockDisableInterrupt(_pciLock);
  OSWriteLittleInt32(_fakePCIDeviceSpace, offset, data);
  IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);
}

UInt16 HyperVGraphicsBridge::configRead16(IOPCIAddressSpace space, UInt8 offset) {
  UInt16 data;
  IOInterruptState ints;

  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return 0xFFFF;
  }

  ints = IOSimpleLockLockDisableInterrupt(_pciLock);
  data = OSReadLittleInt16(_fakePCIDeviceSpace, offset);
  IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);

  HVDBGLOG("Read 16-bit value %u from offset 0x%X", data, offset);
  return data;
}

void HyperVGraphicsBridge::configWrite16(IOPCIAddressSpace space, UInt8 offset, UInt16 data) {
  IOInterruptState ints;

  if (space.es.deviceNum != 0 || space.es.functionNum != 0
      || (offset >= kIOPCIConfigurationOffsetBaseAddress0 && offset <= kIOPCIConfigurationOffsetBaseAddress5)
      || offset == kIOPCIConfigurationOffsetExpansionROMBase) {
    return;
  }
  HVDBGLOG("Writing 16-bit value %u to offset 0x%X", data, offset);

  ints = IOSimpleLockLockDisableInterrupt(_pciLock);
  OSWriteLittleInt16(_fakePCIDeviceSpace, offset, data);
  IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);
}

UInt8 HyperVGraphicsBridge::configRead8(IOPCIAddressSpace space, UInt8 offset) {
  UInt8 data;
  IOInterruptState ints;

  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return 0xFF;
  }

  ints = IOSimpleLockLockDisableInterrupt(_pciLock);
  data = _fakePCIDeviceSpace[offset];
  IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);

  HVDBGLOG("Read 8-bit value %u from offset 0x%X", data, offset);
  return data;
}

void HyperVGraphicsBridge::configWrite8(IOPCIAddressSpace space, UInt8 offset, UInt8 data) {
  IOInterruptState ints;

  if (space.es.deviceNum != 0 || space.es.functionNum != 0
      || (offset >= kIOPCIConfigurationOffsetBaseAddress0 && offset <= kIOPCIConfigurationOffsetBaseAddress5)
      || offset == kIOPCIConfigurationOffsetExpansionROMBase) {
    return;
  }
  HVDBGLOG("Writing 8-bit value %u to offset 0x%X", data, offset);

  ints = IOSimpleLockLockDisableInterrupt(_pciLock);
  _fakePCIDeviceSpace[offset] = data;
  IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);
}
