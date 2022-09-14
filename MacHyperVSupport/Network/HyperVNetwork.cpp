//
//  HyperVNetwork.cpp
//  Hyper-V network driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVNetwork.hpp"

OSDefineMetaClassAndStructors(HyperVNetwork, super);

bool HyperVNetwork::start(IOService *provider) {
  bool     result  = false;
  bool     started = false;
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
  HVDBGLOG("Initializing Hyper-V Synthetic Networking");
  
  do {
    if (HVCheckOffArg()) {
      HVSYSLOG("Disabling Hyper-V Synthetic Networking due to boot arg");
      break;
    }
    
    started = super::start(provider);
    if (!started) {
      HVSYSLOG("super::start() returned false");
      break;
    }
    
    //
    // Install packet handlers.
    //
    status = _hvDevice->installPacketActions(this, OSMemberFunctionCast(HyperVVMBusDevice::PacketReadyAction, this, &HyperVNetwork::handlePacket), OSMemberFunctionCast(HyperVVMBusDevice::WakePacketAction, this, &HyperVNetwork::wakePacketHandler), kHyperVNetworkReceivePacketSize);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to install packet handlers with status 0x%X", status);
      break;
    }
    
#if DEBUG
    _hvDevice->installTimerDebugPrintAction(this, OSMemberFunctionCast(HyperVVMBusDevice::TimerDebugAction, this, &HyperVNetwork::handleTimer));
#endif
    
    //
    // Open VMBus channel.
    //
    status = _hvDevice->openVMBusChannel(kHyperVNetworkRingBufferSize, kHyperVNetworkRingBufferSize, kHyperVNetworkMaximumTransId);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to open VMBus channel with status 0x%X", status);
      break;
    }
    
    // TODO
    rndisLock = IOLockAlloc();
    connectNetwork();
    createMediumDictionary();
    
    //
    // Attach network interface.
    //
    if (!attachInterface((IONetworkInterface **)&ethInterface, false)) {
      HVSYSLOG("Failed to attach network interface");
      break;
    }
    
    result = true;
  } while (false);
  
  if (!result) {
    if (started) {
      super::stop(provider);
    }
  }

  ethInterface->registerService();
  
  HVSYSLOG("Initialized Hyper-V Synthetic Networking");
  return result;
}

IOReturn HyperVNetwork::getHardwareAddress(IOEthernetAddress *addrP) {
  *addrP = ethAddress;
  return kIOReturnSuccess;
}

UInt32 HyperVNetwork::outputPacket(mbuf_t m, void *param) {
  return sendRNDISDataPacket(m) ? kIOReturnOutputSuccess : kIOReturnOutputStall;
}

IOReturn HyperVNetwork::enable(IONetworkInterface *interface) {
  isEnabled = true;
  return kIOReturnSuccess;
}

IOReturn HyperVNetwork::disable(IONetworkInterface *interface) {
  isEnabled = false;
  return kIOReturnSuccess;
}
