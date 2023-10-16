//
//  HyperVPCIBridge.cpp
//  Hyper-V PCI passthrough device support
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVPCIBridge.hpp"

OSDefineMetaClassAndStructors(HyperVPCIBridge, super);

extern const OSSymbol *gIOPlatformGetMessagedInterruptAddressKey;

bool HyperVPCIBridge::start(IOService *provider) {
  bool     result = false;
  IOReturn status;

  //
  // Get parent VMBus device object.
  //
  _hvDevice = OSDynamicCast(HyperVVMBusDevice, provider);
  if (_hvDevice == nullptr) {
    HVSYSLOG("Provider is not HyperVVMBusDevice");
    return false;
  }
  _hvDevice->retain();

  HVCheckDebugArgs();
  HVDBGLOG("Initializing Hyper-V PCI Bridge");

  if (HVCheckOffArg()) {
    HVSYSLOG("Disabling Hyper-V PCI Bridge due to boot arg");
    OSSafeReleaseNULL(_hvDevice);
    return false;
  }
  
  do {
    //
    // Install packet handlers.
    //
    status = _hvDevice->installPacketActions(this,
                                             OSMemberFunctionCast(HyperVVMBusDevice::PacketReadyAction, this, &HyperVPCIBridge::handlePacket),
                                             OSMemberFunctionCast(HyperVVMBusDevice::WakePacketAction, this, &HyperVPCIBridge::wakePacketHandler),
                                             kHyperVPCIBridgeResponsePacketSize);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to install packet handlers with status 0x%X", status);
      break;
    }

    //
    // Open VMBus channel.
    //
    status = _hvDevice->openVMBusChannel(kHyperVPCIBridgeRingBufferSize, kHyperVPCIBridgeRingBufferSize);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to open VMBus channel with status 0x%X", status);
      break;
    }

    //
    // Configure PCI lock.
    //
    _pciLock = IOSimpleLockAlloc();
    if (_pciLock == nullptr) {
      HVSYSLOG("Failed to allocate PCI lock");
      break;
    }

    //
    // Locate root PCI bus instance and register ourselves.
    //
    _hvPCIRoot = HyperVPCIRoot::getPCIRootInstance();
    if (_hvPCIRoot == nullptr) {
      HVSYSLOG("Failed to locate root PCI bus instance");
      break;
    }
    status = _hvPCIRoot->registerChildPCIBridge(this, &_pciBusNumber);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to register with root PCI bus instance with status 0x%X", status);
      break;
    }
  
    //
    // Setup PCI device.
    //
    status = connectPCIBus();
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to initialize PCI bus");
      break;
    }

    //
    // Call super::start() to initiate macOS PCI configuration on bridge.
    //
    if (!super::start(provider)) {
      HVSYSLOG("super::start() returned false");
      break;
    }

    result = true;
  } while (false);

  if (!result) {
    stop(provider);
  }
  return result;
}

IOReturn HyperVPCIBridge::callPlatformFunction(const OSSymbol *functionName, bool waitForFunction, void *param1, void *param2, void *param3, void *param4) {
  IOReturn status;
  HVDBGLOG("Platform function %s", functionName->getCStringNoCopy());

  //
  // Hook gIOPlatformGetMessagedInterruptAddressKey platform function.
  // This function returns MSI address and data for a given interrupt vector.
  //
  // ARGS: gIOPlatformGetMessagedInterruptAddressKey, ..., nub, NULL, vector number, (out) message address[3]
  //
  if (functionName == gIOPlatformGetMessagedInterruptAddressKey) {
    //
    // vector     = interrupt vector
    // message[0] = MSI addr lo
    // message[1] = MSI addr hi
    // message[2] = MSI data
    //
    uintptr_t vector = (uintptr_t)param3;
    UInt32 *message = (UInt32*)param4;
    HVDBGLOG("MSI vector: 0x%X", vector);

    HyperVPCIBridgeMessageCreateInterrupt pktInt;
    pktInt.header.type  = kHyperVPCIBridgeMessageTypeCreateInterruptMessage;
    pktInt.slot.slot    = 0;
    pktInt.vector       = vector;
    pktInt.vectorCount  = 1;
    pktInt.deliveryMode = 0;
    pktInt.cpuMask      = 1;

    HyperVPCIBridgeMessageCreateInterruptResponse pciStatus;
    status = _hvDevice->writeInbandPacket(&pktInt, sizeof (pktInt), true, &pciStatus, sizeof (pciStatus));
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to send interrupt creation message with status 0x%X", status);
      return status;
    }

    if (pciStatus.status != 0) {
      HVSYSLOG("Interrupt creation returned PCI status 0x%X", pciStatus.status);
      return kIOReturnIOError;
    }

    HVDBGLOG("Got MSI address 0x%llX and data 0x%X", pciStatus.address, pciStatus.data);
    message[0] = (UInt32) pciStatus.address;
    message[1] = (UInt32) (pciStatus.address << 32);
    message[2] = pciStatus.data;
    return kIOReturnSuccess;
  } else {
    return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
  }
}

bool HyperVPCIBridge::configure(IOService *provider) {
  bool result;

  //
  // Populate device memory with BARs. TODO: Verify if this is correct for multiple devices.
  //
  for (int i = 0; i < arrsize(_bars); i++) {
    if (_bars[i] == 0) {
      continue;
    }

    result = addBridgeMemoryRange(_bars[i], _barSizes[i], true);
    HVDBGLOG("Adding memory range 0x%llX of %llu bytes for BAR%u - %u", _bars[i], _barSizes[i], i, result);

    //
    // Skip over next bar if 64-bit.
    //
    if (_bars[i] > UINT32_MAX) {
      i++;
    }
  }

  return super::configure(provider);
}

UInt32 HyperVPCIBridge::configRead32(IOPCIAddressSpace space, UInt8 offset) {
  UInt32 offset32 = offset | (space.es.registerNumExtended << 8);
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset32);

  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return -1;
  }

  //
  // Perform 32-bit extended read.
  //
  return readPCIConfig(offset32, sizeof (UInt32));
}

void HyperVPCIBridge::configWrite32(IOPCIAddressSpace space, UInt8 offset, UInt32 data) {
  UInt32 offset32 = offset | (space.es.registerNumExtended << 8);
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset32);

  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return;
  }

  //
  // Perform 32-bit extended write.
  //
  writePCIConfig(offset32, sizeof (UInt32), data);
}

UInt16 HyperVPCIBridge::configRead16(IOPCIAddressSpace space, UInt8 offset) {
  UInt32 offset32 = offset | (space.es.registerNumExtended << 8);
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset32);

  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return -1;
  }

  //
  // Perform 16-bit extended read.
  //
  return (UInt16)readPCIConfig(offset32, sizeof (UInt16));
}

void HyperVPCIBridge::configWrite16(IOPCIAddressSpace space, UInt8 offset, UInt16 data) {
  UInt32 offset32 = offset | (space.es.registerNumExtended << 8);
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset32);
  
  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return;
  }

  //
  // Perform 16-bit extended write.
  //
  writePCIConfig(offset32, sizeof (UInt16), data);
}

UInt8 HyperVPCIBridge::configRead8(IOPCIAddressSpace space, UInt8 offset) {
  UInt32 offset32 = offset | (space.es.registerNumExtended << 8);
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset32);

  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return -1;
  }

  //
  // Perform 8-bit extended read.
  //
  return (UInt8)readPCIConfig(offset32, sizeof (UInt8));
}

void HyperVPCIBridge::configWrite8(IOPCIAddressSpace space, UInt8 offset, UInt8 data) {
  UInt32 offset32 = offset | (space.es.registerNumExtended << 8);
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset32);

  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return;
  }

  //
  // Perform 8-bit extended write.
  //
  writePCIConfig(offset32, sizeof (UInt8), data);
}

bool HyperVPCIBridge::initializeNub(IOPCIDevice *nub, OSDictionary *from) {
  //
  // Merge any injected properties into device tree.
  //
  IOReturn status = mergePropertiesFromDT(nub->getFunctionNumber(), from);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to merge device properties");
  }
  return super::initializeNub(nub, from);
}
