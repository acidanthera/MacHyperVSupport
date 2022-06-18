//
//  HyperVPCIBridge.cpp
//  Hyper-V PCI passthrough device support
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVPCIBridge.hpp"
#include <Headers/kern_api.hpp>

OSDefineMetaClassAndStructors(HyperVPCIBridge, super);

bool HyperVPCIBridge::start(IOService *provider) {
  if (!super::start(provider)) {
    return false;
  }
  
  //
  // Get parent VMBus device object.
  //
  hvDevice = OSDynamicCast(HyperVVMBusDevice, provider);
  if (hvDevice == NULL) {
    super::stop(provider);
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
    super::stop(provider);
    return false;
  }
  
  // Negoiate protocol version and send request for functions.
  if (!negotiateProtocolVersion() || !allocatePCIConfigWindow() || !queryBusRelations() || !enterPCID0()) {
    hvDevice->closeChannel();
    super::stop(provider);
    return false;
  }
  
  UInt16 *bytes = (UInt16*)(pciConfigMemoryMap->getAddress() + 0x1000);
  UInt32 *bytes32 = (UInt32*)(pciConfigMemoryMap->getAddress() + 0x1000);
  
  HVDBGLOG("PCI device ID %X:%X, class code %X", bytes[0], bytes[1], bytes32[2]);
  
  return true;
}
