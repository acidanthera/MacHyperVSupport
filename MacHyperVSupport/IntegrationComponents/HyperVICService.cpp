//
//  HyperVICService.cpp
//  Hyper-V IC base class
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVICService.hpp"

OSDefineMetaClassAndAbstractStructors(HyperVICService, super);

bool HyperVICService::start(IOService *provider) {
  bool      result  = false;
  bool      started = false;
  IOReturn  status;
  
  //
  // Get parent VMBus device object.
  //
  hvDevice = OSDynamicCast(HyperVVMBusDevice, provider);
  if (hvDevice == nullptr) {
    HVSYSLOG("Provider is not HyperVVMBusDevice");
    return false;
  }
  hvDevice->retain();
  
  HVCheckDebugArgs();
  HVDBGLOG("Initializing Hyper-V Integration Component");
  
  do {
    started = super::start(provider);
    if (!started) {
      HVSYSLOG("super::start() returned false");
      break;
    }
    
    //
    // Install packet handler.
    //
    status = hvDevice->installPacketActions(this, OSMemberFunctionCast(HyperVVMBusDevice::PacketReadyAction, this, &HyperVICService::handlePacket),
                                            nullptr, kHyperVICBufferSize, true, false);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to install packet handlers with status 0x%X", status);
      break;
    }
    
    //
    // Open VMBus channel
    //
    status = hvDevice->openVMBusChannel(kHyperVICBufferSize, kHyperVICBufferSize);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to open VMBus channel with status 0x%X", status);
      break;
    }
    
    result = true;
  } while (false);
  
  if (!result) {
    freeStructures();
    if (started) {
      super::stop(provider);
    }
  }
  
  return result;
}

void HyperVICService::stop(IOService *provider) {
  freeStructures();
  super::stop(provider);
}

bool HyperVICService::createNegotiationResponse(VMBusICMessageNegotiate *negMsg, UInt32 fwVersion, UInt32 msgVersion) {
  if (negMsg->frameworkVersionCount == 0 || negMsg->messageVersionCount == 0) {
    HVDBGLOG("Invalid framework or message version count");
    return false;
  }
  UInt32 versionCount = negMsg->frameworkVersionCount + negMsg->messageVersionCount;
  
  UInt32 packetSize = negMsg->header.dataSize + sizeof(negMsg->header);
  if (packetSize < __offsetof(VMBusICMessageNegotiate, versions[versionCount])) {
    HVDBGLOG("Packet has invalid size and does not contain all versions");
    return false;
  }
  
  bool foundFwMatch = false;
  bool foundMsgMatch = false;
  
  //
  // Find supported framework version, and then message version.
  //
  for (int i = 0; i < negMsg->frameworkVersionCount; i++) {
    if (negMsg->versions[i] == fwVersion) {
      negMsg->versions[i] = fwVersion;
      foundFwMatch = true;
      break;
    }
  }
  
  for (int i = negMsg->frameworkVersionCount; i < versionCount; i++) {
    if (negMsg->versions[i] == msgVersion) {
      negMsg->versions[i] = msgVersion;
      foundMsgMatch = true;
      break;
    }
  }
  
  if (foundFwMatch && foundMsgMatch) {
    HVDBGLOG("Found supported fw version %u and msg version %u", fwVersion, msgVersion);
    negMsg->frameworkVersionCount = 1;
    negMsg->messageVersionCount   = 1;
  } else {
    HVDBGLOG("Unsupported fw version %u and msg version %u", fwVersion, msgVersion);
    negMsg->frameworkVersionCount = 0;
    negMsg->messageVersionCount   = 0;
  }
  
  return foundFwMatch && foundMsgMatch;
}

void HyperVICService::freeStructures() {
  //
  // Close channel and release parent VMBus device object.
  //
  if (hvDevice != nullptr) {
    hvDevice->closeVMBusChannel();
    hvDevice->release();
  }
}
