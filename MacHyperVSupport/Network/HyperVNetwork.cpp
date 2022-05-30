//
//  HyperVNetwork.cpp
//  Hyper-V network driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVNetwork.hpp"

OSDefineMetaClassAndStructors(HyperVNetwork, super);

bool HyperVNetwork::start(IOService *provider) {
  if (!super::start(provider)) {
    return false;
  }
  
  HVDBGLOG("Initializing Hyper-V Synthetic Networking");
  
  //
  // Get parent VMBus device object.
  //
  hvDevice = OSDynamicCast(HyperVVMBusDevice, provider);
  if (hvDevice == NULL) {
    super::stop(provider);
    return false;
  }
  hvDevice->retain();
  
  //
  // Configure interrupt.
  //
  interruptSource =
    IOInterruptEventSource::interruptEventSource(this, OSMemberFunctionCast(IOInterruptEventAction, this, &HyperVNetwork::handleInterrupt), provider, 0);
  getWorkLoop()->addEventSource(interruptSource);
  interruptSource->enable();
  
  //
  // Configure the channel.
  //
  if (!hvDevice->openChannel(kHyperVNetworkRingBufferSize, kHyperVNetworkRingBufferSize, kHyperVNetworkMaximumTransId)) {
    super::stop(provider);
    return false;
  }
  
  rndisLock = IOLockAlloc();
  
  connectNetwork();
  createMediumDictionary();
  
  //
  // Attach network interface.
  //
  if (!attachInterface((IONetworkInterface **)&ethInterface, false)) {
    return false;
  }
  ethInterface->registerService();
  
  HVSYSLOG("Initialized Hyper-V Synthetic Networking");
  return true;
}

IOReturn HyperVNetwork::getHardwareAddress(IOEthernetAddress *addrP) {
  *addrP = ethAddress;
  return kIOReturnSuccess;
}

UInt32 HyperVNetwork::outputPacket(mbuf_t m, void *param) {
  return sendRNDISDataPacket(m) ? kIOReturnSuccess : kIOReturnIOError;
}

IOReturn HyperVNetwork::enable(IONetworkInterface *interface) {
  isEnabled = true;
  return kIOReturnSuccess;
}

IOReturn HyperVNetwork::disable(IONetworkInterface *interface) {
  isEnabled = false;
  return kIOReturnSuccess;
}
