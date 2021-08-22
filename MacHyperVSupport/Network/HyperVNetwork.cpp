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
  
  DBGLOG("Initializing Hyper-V Synthetic Networking");
  
  //
  // Get parent VMBus device object.
  //
  hvDevice = OSDynamicCast(HyperVVMBusDevice, provider);
  if (hvDevice == NULL) {
    super::stop(provider);
    return false;
  }
  hvDevice->retain();
  hvDevice->setDebugMessagePrinting(true);
  
  //
  // Configure the channel.
  //
  if (!hvDevice->openChannel(kHyperVNetworkRingBufferSize, kHyperVNetworkRingBufferSize, this, OSMemberFunctionCast(IOInterruptEventAction, this, &HyperVNetwork::handleInterrupt))) {
    super::stop(provider);
    return false;
  }
  
  rndisLock = IOLockAlloc();
  
  connectNetwork();
  
  //
  // Attach network interface.
  //
  if (!attachInterface((IONetworkInterface **)&ethInterface, false)) {
    return false;
  }
  ethInterface->registerService();
  
  SYSLOG("Initialized Hyper-V Synthetic Networking");
  return true;
}

IOReturn HyperVNetwork::getHardwareAddress(IOEthernetAddress *addrP) {
  *addrP = ethAddress;
  return kIOReturnSuccess;
}
