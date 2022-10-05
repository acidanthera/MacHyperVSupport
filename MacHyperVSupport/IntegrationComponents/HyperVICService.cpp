//
//  HyperVICService.cpp
//  Hyper-V IC base class
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVICService.hpp"

OSDefineMetaClassAndAbstractStructors(HyperVICService, super);

static const VMBusICVersion frameworkVersions[] = {
  kHyperVICVersionV3,
  kHyperVICVersion2008
};

bool HyperVICService::start(IOService *provider) {
  bool     result = false;
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
  HVDBGLOG("Initializing Hyper-V Integration Component");

  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    OSSafeReleaseNULL(_hvDevice);
    return false;
  }

  do {
    //
    // Install packet handler.
    //
    status = _hvDevice->installPacketActions(this, OSMemberFunctionCast(HyperVVMBusDevice::PacketReadyAction, this, &HyperVICService::handlePacket),
                                             nullptr, kHyperVICBufferSize, true, false);
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to install packet handler with status 0x%X", status);
      break;
    }

    //
    // Open VMBus channel
    //
    status = _hvDevice->openVMBusChannel(txBufferSize(), rxBufferSize());
    if (status != kIOReturnSuccess) {
      HVSYSLOG("Failed to open VMBus channel with status 0x%X", status);
      break;
    }

    result = true;
  } while (false);
  
  if (!result) {
    stop(provider);
  }
  return result;
}

void HyperVICService::stop(IOService *provider) {
  //
  // Close channel and remove handler.
  //
  if (_hvDevice != nullptr) {
    _hvDevice->closeVMBusChannel();
    _hvDevice->uninstallPacketActions();
    OSSafeReleaseNULL(_hvDevice);
  }

  super::stop(provider);
}

bool HyperVICService::processNegotiationResponse(VMBusICMessageNegotiate *negMsg, const VMBusICVersion *msgVersions,
                                                 UInt32 msgVersionsCount, VMBusICVersion *msgVersionUsed) {
  UInt32 versionCount;
  UInt32 packetSize;

  bool foundFwMatch  = false;
  bool foundMsgMatch = false;

  VMBusICVersion frameworkVersion = { };
  VMBusICVersion msgVersion       = { };

  if (negMsg->frameworkVersionCount == 0 || negMsg->messageVersionCount == 0) {
    HVDBGLOG("Invalid framework or message version count");
    return false;
  }

  versionCount = negMsg->frameworkVersionCount + negMsg->messageVersionCount;
  packetSize   = negMsg->header.dataSize + sizeof (negMsg->header);
  if (packetSize < __offsetof (VMBusICMessageNegotiate, versions[versionCount])) {
    HVSYSLOG("Negotiate packet is invalid size (%u bytes)", packetSize);
    return false;
  }

  //
  // Determine highest supported framework version.
  //
  for (UInt32 i = 0; i < arrsize(frameworkVersions); i++) {
    for (UInt32 j = 0; j < negMsg->frameworkVersionCount; j++) {
      HVDBGLOG("Checking framework version %u.%u against version %u.%u",
               frameworkVersions[i].major, frameworkVersions[i].minor, negMsg->versions[j].major, negMsg->versions[j].minor);
      if ((frameworkVersions[i].major == negMsg->versions[j].major)
          && (frameworkVersions[i].minor == negMsg->versions[j].minor)) {
        frameworkVersion = negMsg->versions[j];
        foundFwMatch = true;
        break;
      }
    }

    if (foundFwMatch) {
      break;
    }
  }

  //
  // Determine highest supported message version.
  //
  for (UInt32 i = 0; i < msgVersionsCount; i++) {
    for (UInt32 j = negMsg->frameworkVersionCount; j < versionCount; j++) {
      HVDBGLOG("Checking message version %u.%u against version %u.%u",
               msgVersions[i].major, msgVersions[i].minor, negMsg->versions[j].major, negMsg->versions[j].minor);
      if ((msgVersions[i].major == negMsg->versions[j].major)
          && (msgVersions[i].minor == negMsg->versions[j].minor)) {
        msgVersion = negMsg->versions[j];
        foundMsgMatch = true;
        break;
      }
    }

    if (foundMsgMatch) {
      break;
    }
  }

  if (foundFwMatch && foundMsgMatch) {
    HVDBGLOG("Found supported fw version %u.%u and msg version %u.%u",
             frameworkVersion.major, frameworkVersion.minor, msgVersion.major, msgVersion.minor);
    negMsg->frameworkVersionCount = 1;
    negMsg->messageVersionCount   = 1;

    negMsg->versions[0] = frameworkVersion;
    negMsg->versions[1] = msgVersion;

    if (msgVersionUsed != nullptr) {
      *msgVersionUsed = msgVersion;
    }
  } else {
    HVDBGLOG("Unsupported fw/msg version");
    negMsg->frameworkVersionCount = 0;
    negMsg->messageVersionCount   = 0;
  }

  return foundFwMatch && foundMsgMatch;
}
