//
//  HyperVPCIBridge.cpp
//  Hyper-V PCI passthrough device support
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVPCIBridge.hpp"
#include <Headers/kern_api.hpp>

#include "HyperVModuleDevice.hpp"

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
  if (!negotiateProtocolVersion() || !queryBusRelations()) {
    hvDevice->closeChannel();
    super::stop(provider);
    return false;
  }
  
  OSDictionary *pciMatching = IOService::serviceMatching("HyperVModuleDevice");
  if (pciMatching == NULL) {
    HVSYSLOG("Failed to create HyperVModuleDevice matching dictionary");
    return false;
  }
  
  HVDBGLOG("Waiting for HyperVModuleDevice");
  IOService *pciService = waitForMatchingService(pciMatching);
  pciMatching->release();
  if (pciService == NULL) {
    HVSYSLOG("Failed to locate HyperVModuleDevice");
    super::stop(provider);
    return false;
  }
  HVDBGLOG("got for HyperVModuleDevice");

  
  HyperVModuleDevice *hvModuleDevice = OSDynamicCast(HyperVModuleDevice, pciService);
  
  IORangeScalar pciConfigSpace;
  hvModuleDevice->rangeAllocator->allocate(0x2000, &pciConfigSpace, 0x1000);
  
  HyperVPCIBridgeMessagePCIBusD0Entry pktD0;
  pktD0.header.type =kHyperVPCIBridgeMessageTypeBusD0Entry;
  pktD0.reserved = 0;
  pktD0.mmioBase = pciConfigSpace;
  
  UInt32 pciStatus = 0x1234567;
  hvDevice->writeInbandPacket(&pktD0, sizeof (pktD0), true, &pciStatus, sizeof (pciStatus));
  
  HVDBGLOG("PCI status 0x%X", pciStatus);
  HVDBGLOG("using config space @ 0x%llX", pciConfigSpace);
  
  IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(pciConfigSpace, 0x2000, kIOMemoryDirectionInOut);
  IOMemoryMap *map = desc->map();
  
  UInt16 *bytes = (UInt16*)(map->getAddress() + 0x1000);
  UInt32 *bytes32 = (UInt32*)(map->getAddress() + 0x1000);
  
  HVDBGLOG("PCI device ID %X:%X, class code %X", bytes[0], bytes[1], bytes32[2]);
  
  
  pciService->release();
  
  return true;
}
