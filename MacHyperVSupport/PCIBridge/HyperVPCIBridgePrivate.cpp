//
//  HyperVPCIBridgePrivate.cpp
//  Hyper-V PCI passthrough device support
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVPCIBridge.hpp"

static HyperVPCIBridgeProtocolVersion usableVersions[] = {
  kHyperVPCIBridgeProtocolVersion1
};

bool HyperVPCIBridge::wakePacketHandler(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  return pktHeader->type == kVMBusPacketTypeCompletion;
}

void HyperVPCIBridge::handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  //
  // Handle inbound packet.
  //
  HVDBGLOG("Incoming packet type %u", pktHeader->type);
  switch (pktHeader->type) {
    case kVMBusPacketTypeDataInband:
      handleIncomingPCIMessage((HyperVPCIBridgeIncomingMessageHeader*)pktData, pktDataLength);
      _hvDevice->wakeTransaction(0);
      break;

    default:
      HVSYSLOG("Invalid packet type %X", pktHeader->type);
      break;
  }
}

void HyperVPCIBridge::handleIncomingPCIMessage(HyperVPCIBridgeIncomingMessageHeader *pciMsgHeader, UInt32 msgSize) {
  HyperVPCIBridgeIncomingMessageBusRelations *busRelations;
  HVDBGLOG("Incoming PCI message type 0x%X", pciMsgHeader->type);
  
  switch (pciMsgHeader->type) {
    //
    // Handle bus relations message v1.
    //
    case kHyperVPCIBridgeMessageTypeBusRelations:
      if (msgSize < sizeof (HyperVPCIBridgeIncomingMessageBusRelations)) {
        HVSYSLOG("Bus relations message size %u < %u", msgSize, sizeof (HyperVPCIBridgeIncomingMessageBusRelations));
        return;
      }
      busRelations = (HyperVPCIBridgeIncomingMessageBusRelations*) pciMsgHeader;
      if (msgSize < sizeof (HyperVPCIBridgeIncomingMessageBusRelations) + (busRelations->functionCount * sizeof (HyperVPCIFunctionDescription))) {
        HVSYSLOG("Bus relations message size %u < %u", msgSize,
                 sizeof (HyperVPCIBridgeIncomingMessageBusRelations) + (busRelations->functionCount * sizeof (HyperVPCIFunctionDescription)));
        return;
      }
      
      //
      // Store function list for later use.
      //
      if (_pciFunctions != nullptr) {
        IOFree(_pciFunctions, _pciFunctionsCount * sizeof (HyperVPCIFunctionDescription));
        _pciFunctions = nullptr;
        _pciFunctionsCount = 0;
      }
      
      _pciFunctionsCount = busRelations->functionCount;
      _pciFunctions = (HyperVPCIFunctionDescription*) IOMalloc(_pciFunctionsCount * sizeof (HyperVPCIFunctionDescription));
      for (int i = 0; i < _pciFunctionsCount; i++) {
        memcpy(&_pciFunctions[i], &busRelations->functions[i], sizeof (HyperVPCIFunctionDescription));
      }
      break;
      
    default:
      break;
  }
}

IOReturn HyperVPCIBridge::connectPCIBus() {
  IOReturn status;

  status = negotiateProtocolVersion();
  if (status != kIOReturnSuccess) {
    return status;
  }
  status = allocatePCIConfigWindow();
  if (status != kIOReturnSuccess) {
    return status;
  }
  status = queryBusRelations();
  if (status != kIOReturnSuccess) {
    return status;
  }
  status = enterPCID0();
  if (status != kIOReturnSuccess) {
    return status;
  }
  status = queryResourceRequirements();
  if (status != kIOReturnSuccess) {
    return status;
  }
  status = sendResourcesAssigned(0);
  if (status != kIOReturnSuccess) {
    return status;
  }

  return status;
}

IOReturn HyperVPCIBridge::negotiateProtocolVersion() {
  HyperVPCIBridgeMessageProtocolVersionRequest pktVersion;
  pktVersion.header.type = kHyperVPCIBridgeMessageTypeQueryProtocolVersion;

  // Attempt to find newest version host can support.
  for (int i = 0; i < arrsize(usableVersions); i++) {
    pktVersion.version = usableVersions[i];
    HVDBGLOG("Attempting to use version 0x%X", pktVersion.version);

    UInt32 pciStatus;
    if (_hvDevice->writeInbandPacket(&pktVersion, sizeof (pktVersion), true, &pciStatus, sizeof (pciStatus)) != kIOReturnSuccess) {
      return kIOReturnUnsupported;
    }
    HVDBGLOG("PCI return status is 0x%X", pciStatus);
    if (pciStatus == 0) {
      // Version is acceptable.
      currentPciVersion = pktVersion.version;
      HVDBGLOG("Using version 0x%X", currentPciVersion);
      return kIOReturnSuccess;
    }
  }

  return kIOReturnUnsupported;
}

IOReturn HyperVPCIBridge::allocatePCIConfigWindow() {
  //
  // Get HyperVModuleDevice instance used for allocating MMIO regions for Hyper-V.
  //
  OSDictionary *vmodMatching = IOService::serviceMatching("HyperVModuleDevice");
  if (vmodMatching == nullptr) {
    HVSYSLOG("Failed to create HyperVModuleDevice matching dictionary");
    return kIOReturnNotFound;
  }

  HVDBGLOG("Waiting for HyperVModuleDevice");
#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_6
  IOService *vmodService = IOService::waitForService(vmodMatching);
  if (vmodService != nullptr) {
    vmodService->retain();
  }
#else
  IOService *vmodService = waitForMatchingService(vmodMatching);
  vmodMatching->release();
#endif

  if (vmodService == nullptr) {
    HVSYSLOG("Failed to locate HyperVModuleDevice");
    return kIOReturnNotFound;
  }
  HVDBGLOG("Got instance of HyperVModuleDevice");
  _hvModuleDevice = OSDynamicCast(HyperVModuleDevice, vmodService);

  //
  // Allocate PCI config window. TODO: handle lack of high MMIO space.
  //
  _pciConfigSpace = _hvModuleDevice->allocateRange(kHyperVPCIBridgeWindowSize, PAGE_SIZE, false);
  if (_pciConfigSpace == 0) {
    HVSYSLOG("Could not allocate range for PCI config window");
    return kIOReturnNoResources;
  }

  _pciConfigMemoryDescriptor = IOMemoryDescriptor::withPhysicalAddress(_pciConfigSpace, kHyperVPCIBridgeWindowSize,
                                                                       static_cast<IODirection>(kIOMemoryDirectionInOut));
  if (_pciConfigMemoryDescriptor == nullptr) {
    HVSYSLOG("Could not create descriptor for PCI config window");
    return kIOReturnNoResources;
  }

  _pciConfigMemoryMap = _pciConfigMemoryDescriptor->map();
  if (_pciConfigMemoryMap == nullptr) {
    HVSYSLOG("Could not map PCI config window");
    return kIOReturnNoResources;
  }

  HVDBGLOG("PCI config window located @ phys 0x%llX", _pciConfigSpace);
  return kIOReturnSuccess;
}

IOReturn HyperVPCIBridge::queryBusRelations() {
  IOReturn status;

  //
  // Get list of functions from Hyper-V.
  //
  HyperVPCIBridgeMessageHeader pktQueryRelations;
  pktQueryRelations.type = kHyperVPCIBridgeMessageTypeQueryBusRelations;
  status = _hvDevice->writeInbandPacket(&pktQueryRelations, sizeof (pktQueryRelations), false);
  if (status != kIOReturnSuccess) {
    return status;
  }
  
  //
  // Response is not in the form of a normal completion, we'll
  // need to wait for an inband bus relations packet instead.
  //
  _hvDevice->sleepThreadZero();
  if (_pciFunctionsCount == 0) {
    HVSYSLOG("Bus does not contain any PCI functions");
    return kIOReturnUnsupported;
  }

  if (debugEnabled) {
    HVDBGLOG("Found %u PCI functions:", _pciFunctionsCount);
    for (int i = 0; i < _pciFunctionsCount; i++) {
      HVDBGLOG("Function %u with ID %04X:%04X:", i, _pciFunctions[i].vendorId, _pciFunctions[i].deviceId);
      HVDBGLOG("Subsystem ID %04X:%04X, Revision 0x%X", _pciFunctions[i].subVendorId, _pciFunctions[i].subDeviceId, _pciFunctions[i].revision);
      HVDBGLOG("Class 0x%X, Subclass 0x%X, Prog int 0x%X", _pciFunctions[i].baseClass, _pciFunctions[i].subClass, _pciFunctions[i].progInterface);
      HVDBGLOG("Slot 0x%X, Serial 0x%X", _pciFunctions[i].slot.slot, _pciFunctions[i].serialNumber);
    }
  }
  return kIOReturnSuccess;
}

IOReturn HyperVPCIBridge::enterPCID0() {
  IOReturn status;
  UInt32 pciStatus;

  //
  // Instruct Hyper-V to enable the PCI bridge using the allocated PCI config window.
  //
  HyperVPCIBridgeMessagePCIBusD0Entry pktD0;
  pktD0.header.type = kHyperVPCIBridgeMessageTypeBusD0Entry;
  pktD0.reserved    = 0;
  pktD0.mmioBase    = _pciConfigSpace;
  status = _hvDevice->writeInbandPacket(&pktD0, sizeof (pktD0), true, &pciStatus, sizeof (pciStatus));
  if (status != kIOReturnSuccess) {
    return status;
  }

  if (pciStatus != 0) {
    HVSYSLOG("Enter D0 returned error status 0x%X", pciStatus);
    return kIOReturnIOError;
  }

  HVDBGLOG("PCI bridge has entered D0 state using config space @ phys 0x%X", _pciConfigSpace);
  return kIOReturnSuccess;
}

IOReturn HyperVPCIBridge::queryResourceRequirements() {
  IOReturn status;

  //
  // Query resource requirements from Hyper-V.
  //
  HyperVPCIBridgeChildMessage pktChild;
  pktChild.header.type = kHyperVPCIBridgeMessageTypeQueryResourceRequirements;
  pktChild.slot.slot = 0; // TODO: Variable slots
  
  HyperVPCIBridgeQueryResourceRequirementsResponse pktReqResponse;
  status = _hvDevice->writeInbandPacket(&pktChild, sizeof (pktChild), true, &pktReqResponse, sizeof (pktReqResponse));
  if (status != kIOReturnSuccess) {
    return status;
  }

  //
  // Allocate and set BARs for PCI device.
  //
  memset(_bars, 0, sizeof (_bars));
  memset(_barSizes, 0, sizeof (_barSizes));
  status = kIOReturnSuccess;

  HVDBGLOG("Got resource requirements with status %u", pktReqResponse.status);
  for (int i = 0; i < kHyperVPCIBarCount; i++) {
    UInt64 barVal = pktReqResponse.probedBARs[i];

    //
    // Skip inactive BARs.
    //
    if (barVal == 0) {
      HVDBGLOG("BAR%u is zero", i);
      continue;
    }

    //
    // Abort on any I/O BARs found as these cannot be supported.
    //
    if (barVal & kHyperVPCIBarSpaceIO) {
      HVSYSLOG("BAR%u is not MMIO, unsupported device", i);
      status = kIOReturnUnsupported;
      break;
    }

    //
    // Check type of BAR (64-bit or 32-bit).
    // If 64-bit the next BAR is the high 32 bits, and this one is the low 32 bits.
    //
    bool isBar64Bit = barVal & kHyperVPCIBarMemoryType64Bit;
    if (isBar64Bit) {
      barVal |= ((UInt64)pktReqResponse.probedBARs[i + 1] << 32);
    }
    HVDBGLOG("BAR%u is %u-bit with initial value 0x%llX", i, isBar64Bit ? 64 : 32, barVal);

    //
    // Determine size of BAR and allocate it. TODO: Support 64-bit BAR allocation and fix macOS.
    //
    _barSizes[i] = getBarSize(barVal);
    HVDBGLOG("BAR%u requires %llu bytes", i, _barSizes[i]);
    _bars[i] = _hvModuleDevice->allocateRange(_barSizes[i], _barSizes[i], false);
    if (_bars[i] == 0) {
      HVSYSLOG("BAR%u could not be allocated, no more resources", i);
      status = kIOReturnNoResources;
      break;
    }
    HVDBGLOG("BAR%u will now be located @ phys 0x%llX", i, _bars[i]);

    //
    // Write new BAR to PCI device.
    //
    writePCIConfig(kIOPCIConfigBaseAddress0 + (i * sizeof (UInt32)), sizeof (UInt32), (UInt32)_bars[i]);
    if (isBar64Bit) {
      writePCIConfig(kIOPCIConfigBaseAddress0 + ((i + 1) * sizeof (UInt32)), sizeof (UInt32), (UInt32)(_bars[i] >> 32));
    }

    //
    // 64-bit BARs take two 32-bit BAR slots, so skip over the next one.
    //
    HVDBGLOG("BAR%u is now 0x%X", i, readPCIConfig(kIOPCIConfigBaseAddress0 + (i * sizeof (UInt32)), sizeof (UInt32)));
    if (isBar64Bit) {
      i++;
      HVDBGLOG("BAR%u is now 0x%X (high 32 bits)", i, readPCIConfig(kIOPCIConfigBaseAddress0 + (i * sizeof (UInt32)), sizeof (UInt32)));
    }
  }

  //
  // Enable memory I/O on device.
  //
  if (status == kIOReturnSuccess) {
    writePCIConfig(kIOPCIConfigCommand, sizeof (UInt16), readPCIConfig(kIOPCIConfigCommand, sizeof (UInt16)) | kIOPCICommandMemorySpace);
  }
  return status;
}

IOReturn HyperVPCIBridge::sendResourcesAssigned(UInt32 slot) {
  IOReturn status;

  //
  // Notify host that all required PCI resources have been assigned to the device.
  //
  HyperVPCIBridgeMessageResourcesAssigned pktRes;
  memset(&pktRes, 0, sizeof (pktRes));
  pktRes.header.type = kHyperVPCIBridgeMessageTypeResourcesAssigned;
  pktRes.slot.slot = 0; // TODO: Variable slots
  
  SInt32  pciStatus;
  
  status = _hvDevice->writeInbandPacket(&pktRes, sizeof (pktRes), true, &pciStatus, sizeof (pciStatus));
  if (status != kIOReturnSuccess) {
    return status;
  }
  
  HVDBGLOG("PCI status %X", pciStatus);
  
  return status;
}

UInt32 HyperVPCIBridge::readPCIConfig(UInt32 offset, UInt8 size) {
  UInt32 result;

  HVDBGLOG("Reading size %u from offset 0x%X", size, offset);

  //
  // Simulate special registers.
  //
  if (offset + size <= kIOPCIConfigCommand) {
    memcpy(&result, ((UInt8*)&_pciFunctions[0].vendorId) + offset, size);
  } else if (offset >= kIOPCIConfigRevisionID && offset + size <= kIOPCIConfigCacheLineSize) {
    memcpy(&result, ((UInt8*)&_pciFunctions[0].revision) + offset - kIOPCIConfigRevisionID, size);
  } else if (offset >= kIOPCIConfigSubSystemVendorID && offset + size <= kIOPCIConfigExpansionROMBase) {
    memcpy(&result, ((UInt8*)&_pciFunctions[0].subVendorId) + offset - kIOPCIConfigSubSystemVendorID, size);
  } else if (offset >= kIOPCIConfigExpansionROMBase && offset + size <= kIOPCIConfigCapabilitiesPtr) {
    //
    // Not implemented.
    //
    result = 0;
  } else if (offset >= kIOPCIConfigInterruptLine && offset + size <= kIOPCIConfigInterruptPin) {
    //
    // Physical interrupt lines are not supported.
    //
    result = 0;
  } else if (offset + size <= PAGE_SIZE) {
    volatile UInt8 *pciAddress = (UInt8*)_pciConfigMemoryMap->getAddress() + kHyperVPCIConfigPageOffset + offset;

    IOInterruptState ints = IOSimpleLockLockDisableInterrupt(_pciLock);
    switch (size) {
      case sizeof (UInt8):
        result = *pciAddress;
        break;

      case sizeof (UInt16):
        result = OSReadLittleInt16(pciAddress, 0);
        break;

      default:
        result = OSReadLittleInt32(pciAddress, 0);
        break;
    }
    IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);
  } else {
    HVDBGLOG("Attempted to read beyond PCI config space");
    result = -1;
  }

  HVDBGLOG("Result = 0x%X", result);
  return result;
}

void HyperVPCIBridge::writePCIConfig(UInt32 offset, UInt8 size, UInt32 value) {
  if (offset >= kIOPCIConfigSubSystemVendorID && offset + size <= kIOPCIConfigCapabilitiesPtr) {
    //
    // Read-only sections.
    //
    HVDBGLOG("Attempted to write value 0x%X to read-only offset 0x%X", value, offset);
  } else if (offset >= kIOPCIConfigCommand && offset + size <= PAGE_SIZE) {
    HVDBGLOG("Writing value 0x%X of size %u to offset 0x%X", value, size, offset);
    volatile UInt8 *pciAddress = (UInt8*)_pciConfigMemoryMap->getAddress() + kHyperVPCIConfigPageOffset + offset;

    IOInterruptState ints = IOSimpleLockLockDisableInterrupt(_pciLock);
    switch (size) {
      case sizeof (UInt8):
        *pciAddress = (UInt8)value;
        break;

      case sizeof (UInt16):
        OSWriteLittleInt16(pciAddress, 0, (UInt16)value);
        break;

      default:
        OSWriteLittleInt32(pciAddress, 0, value);
        break;
    }
    IOSimpleLockUnlockEnableInterrupt(_pciLock, ints);
  } else {
    HVDBGLOG("Attempted to write value 0x%X to out-of-bounds offset 0x%X", value, offset);
  }
}
