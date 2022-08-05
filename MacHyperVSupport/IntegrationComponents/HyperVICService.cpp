//
//  HyperVICService.cpp
//  Hyper-V IC base class
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVICService.hpp"

OSDefineMetaClassAndAbstractStructors(HyperVICService, super);

bool HyperVICService::start(IOService *provider) {
  bool      result = false;
  IOReturn  status;
  
  if (!super::start(provider)) {
    return false;
  }
  
  do {
    //
    // Get parent VMBus device object.
    //
    hvDevice = OSDynamicCast(HyperVVMBusDevice, provider);
    if (hvDevice == nullptr) {
      HVSYSLOG("Unable to get parent VMBus device nub");
      break;
    }
    hvDevice->retain();
    HVCheckDebugArgs();
    
    //
    // Configure interrupt.
    //
    interruptSource = IOInterruptEventSource::interruptEventSource(this, OSMemberFunctionCast(IOInterruptEventAction, this, &HyperVICService::handleInterrupt), provider, 0);
    if (interruptSource == nullptr) {
      HVSYSLOG("Unable to initialize interrupt");
      break;
    }
    
    status = getWorkLoop()->addEventSource(interruptSource);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Unable to add interrupt event source: 0x%X", status);
      break;
    }
    interruptSource->enable();
    
    //
    // Configure and open the VMBus channel.
    //
    if (!hvDevice->openChannel(kHyperVICBufferSize, kHyperVICBufferSize)) {
      HVSYSLOG("Unable to configure VMBus channel");
      break;
    }
    
    result = true;
  } while (false);
  
  if (!result) {
    if (interruptSource != nullptr) {
      interruptSource->disable();
      getWorkLoop()->removeEventSource(interruptSource);
      interruptSource->release();
    }
    OSSafeReleaseNULL(hvDevice);
  }
  
  return result;
}

void HyperVICService::stop(IOService *provider) {
  //
  // Release interrupt.
  //
  if (interruptSource != nullptr) {
    interruptSource->disable();
    getWorkLoop()->removeEventSource(interruptSource);
    interruptSource->release();
  }
  
  //
  // Close channel and release parent VMBus device object.
  //
  if (hvDevice != nullptr) {
    hvDevice->closeChannel();
    hvDevice->release();
  }
  
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

void HyperVICService::handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count) {
  while (processMessage());
}
