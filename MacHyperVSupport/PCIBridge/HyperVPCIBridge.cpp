//
//  HyperVPCIBridge.cpp
//  Hyper-V PCI passthrough device support
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVPCIBridge.hpp"
#include <Headers/kern_api.hpp>
#include "HyperVPCIRoot.hpp"

OSDefineMetaClassAndStructors(HyperVPCIBridge, super);

bool HyperVPCIBridge::start(IOService *provider) {
  //
  // Get parent VMBus device object.
  //
  hvDevice = OSDynamicCast(HyperVVMBusDevice, provider);
  if (hvDevice == NULL) {
    return false;
  }
  hvDevice->retain();
  
  debugEnabled = checkKernelArgument("-hvpcidbg");
  hvDevice->setDebugMessagePrinting(checkKernelArgument("-hvpcimsgdbg"));
  
  //
  // Configure interrupt.
  //
  interruptSource =
    IOInterruptEventSource::interruptEventSource(this, OSMemberFunctionCast(IOInterruptEventAction, this, &HyperVPCIBridge::handleInterrupt), provider, 0);
  getWorkLoop()->addEventSource(interruptSource);
  interruptSource->enable();
  
  //
  // Configure the channel.
  //
  if (!hvDevice->openChannel(kHyperVPCIBridgeRingBufferSize, kHyperVPCIBridgeRingBufferSize, 0xFFFFFFFF)) {
    return false;
  }
  
  pciLock = IOSimpleLockAlloc();
  
  //
  // Locate root PCI bus instance and register ourselves.
  //
  if (!HyperVPCIRoot::registerChildPCIBridge(this)) {
    HVSYSLOG("Failed to register with root PCI bus instance");
    hvDevice->release();
    return false;
  }
  
  // Negoiate protocol version and send request for functions.
  if (!negotiateProtocolVersion() || !allocatePCIConfigWindow() || !queryBusRelations() || !enterPCID0()) {
    hvDevice->closeChannel();
    return false;
  }
  
  if (!super::start(provider)) {
    return false;
  }
  
  return true;
}

UInt32 HyperVPCIBridge::configRead32(IOPCIAddressSpace space, UInt8 offset) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return 0xFFFFFFFF;
  }
  
  return readPCIConfig(offset, sizeof (UInt32));
}

void HyperVPCIBridge::configWrite32(IOPCIAddressSpace space, UInt8 offset, UInt32 data) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return;
  }
  
  writePCIConfig(offset, sizeof (UInt32), data);
}

UInt16 HyperVPCIBridge::configRead16(IOPCIAddressSpace space, UInt8 offset) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return 0xFFFF;
  }
  
  return (UInt16)readPCIConfig(offset, sizeof (UInt16));
}

void HyperVPCIBridge::configWrite16(IOPCIAddressSpace space, UInt8 offset, UInt16 data) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return;
  }
  
  writePCIConfig(offset, sizeof (UInt16), data);
}

UInt8 HyperVPCIBridge::configRead8(IOPCIAddressSpace space, UInt8 offset) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return 0xFF;
  }
  
  return (UInt8)readPCIConfig(offset, sizeof (UInt8));
}

void HyperVPCIBridge::configWrite8(IOPCIAddressSpace space, UInt8 offset, UInt8 data) {
  HVDBGLOG("Bus: %u, device: %u, function: %u, offset %X", space.es.busNum, space.es.deviceNum, space.es.functionNum, offset);
  
  if (space.es.deviceNum != 0 || space.es.functionNum != 0) {
    return;
  }
  
  writePCIConfig(offset, sizeof (UInt8), data);
}
