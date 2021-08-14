//
//  HyperVNetwork.cpp
//  Hyper-V network driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVNetwork.hpp"
#include "HyperVNetworkRegs.hpp"

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
  
  //
  // Configure the channel.
  //
  if (!hvDevice->openChannel(kHyperVNetworkRingBufferSize, kHyperVNetworkRingBufferSize, this, OSMemberFunctionCast(IOInterruptEventAction, this, &HyperVNetwork::handleInterrupt))) {
    super::stop(provider);
    return false;
  }
  
  SYSLOG("Initialized Hyper-V Synthetic Networking");
  return true;
}

void HyperVNetwork::handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count) {
  DBGLOG("Interrupt");
}
