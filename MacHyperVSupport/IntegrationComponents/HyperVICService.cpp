//
//  HyperVICService.cpp
//  Hyper-V IC base class
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVICService.hpp"

#define super IOService

#define SYSLOG(str, ...) SYSLOG_PRINT("HyperVICService", str, ## __VA_ARGS__)
#define DBGLOG(str, ...) DBGLOG_PRINT("HyperVICService", str, ## __VA_ARGS__)

OSDefineMetaClassAndAbstractStructors(HyperVICService, super);

bool HyperVICService::start(IOService *provider) {
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
  
  //
  // Configure interrupt.
  //
  interruptSource = IOInterruptEventSource::interruptEventSource(this, OSMemberFunctionCast(IOInterruptEventAction, this, &HyperVICService::handleInterrupt), provider, 0);
  getWorkLoop()->addEventSource(interruptSource);
  interruptSource->enable();
  
  //
  // Configure the channel.
  //
  if (!hvDevice->openChannel(kHyperVICBufferSize, kHyperVICBufferSize)) {
    super::stop(provider);
    return false;
  }
  
  return true;
}

void HyperVICService::stop(IOService *provider) {
  if (hvDevice != NULL) {
    hvDevice->closeChannel();
  }
  
  super::stop(provider);
}

bool HyperVICService::createNegotiationResponse(VMBusICMessageNegotiate *negMsg, UInt32 fwVersion, UInt32 msgVersion) {
  if (negMsg->frameworkVersionCount == 0 || negMsg->messageVersionCount == 0) {
    DBGLOG("Invalid framework or message version count");
    return false;
  }
  UInt32 versionCount = negMsg->frameworkVersionCount + negMsg->messageVersionCount;
  
  UInt32 packetSize = negMsg->header.dataSize + sizeof(negMsg->header);
  if (packetSize < __offsetof(VMBusICMessageNegotiate, versions[versionCount])) {
    DBGLOG("Packet has invalid size and does not contain all versions");
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
    DBGLOG("Found supported fw version %u and msg version %u", fwVersion, msgVersion);
    negMsg->frameworkVersionCount = 1;
    negMsg->messageVersionCount   = 1;
  } else {
    DBGLOG("Unsupported fw version %u and msg version %u", fwVersion, msgVersion);
    negMsg->frameworkVersionCount = 0;
    negMsg->messageVersionCount   = 0;
  }
  
  return foundFwMatch && foundMsgMatch;
}

void HyperVICService::handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count) {
  while (processMessage());
}
