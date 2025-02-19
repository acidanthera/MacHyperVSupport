//
//  HyperVShutdown.cpp
//  Hyper-V guest shutdown driver
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#include "HyperVShutdown.hpp"

OSDefineMetaClassAndStructors(HyperVShutdown, super);

static const VMBusVersion shutdownVersions[] = {
  { kHyperVShutdownVersionV3_2 },
  { kHyperVShutdownVersionV3_1 },
  { kHyperVShutdownVersionV3_0 },
  { kHyperVShutdownVersionV1_0 }
};

bool HyperVShutdown::start(IOService *provider) {
  if (HVCheckOffArg()) {
    HVSYSLOG("Disabling Hyper-V Guest Shutdown due to boot arg");
    return false;
  }

  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    return false;
  }

  HVCheckDebugArgs();
  setICDebug(debugEnabled);

  registerService();
  HVDBGLOG("Initialized Hyper-V Guest Shutdown");
  return true;
}

void HyperVShutdown::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V Guest Shutdown");
  super::stop(provider);
}

bool HyperVShutdown::open(IOService *forClient, IOOptionBits options, void *arg) {
  HyperVShutdownUserClient *hvShutdownUserClient = OSDynamicCast(HyperVShutdownUserClient, forClient);
  if (hvShutdownUserClient == nullptr || _userClientInstance != nullptr) {
    return false;
  }

  if (!super::open(forClient, options, arg)) {
    return false;
  }

  _userClientInstance = hvShutdownUserClient;
  return true;
}

void HyperVShutdown::close(IOService *forClient, IOOptionBits options) {
  _userClientInstance = nullptr;
  super::close(forClient, options);
}

void HyperVShutdown::handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  VMBusICMessageShutdown *shutdownMsg = (VMBusICMessageShutdown*) pktData;

  bool doShutdown = false;
  switch (shutdownMsg->header.type) {
    case kVMBusICMessageTypeNegotiate:
      //
      // Determine supported protocol version and communicate back to Hyper-V.
      //
      if (!processNegotiationResponse(&shutdownMsg->negotiate, shutdownVersions, arrsize(shutdownVersions))) {
        HVSYSLOG("Failed to determine a supported Hyper-V Guest Shutdown version");
        shutdownMsg->header.status = kHyperVStatusFailure;
      }
      break;

    case kVMBusICMessageTypeShutdown:
      //
      // Shutdown/restart request.
      //
      doShutdown = handleShutdown(&shutdownMsg->shutdown);
      break;

    default:
      HVDBGLOG("Unknown shutdown message type %u", shutdownMsg->header.type);
      shutdownMsg->header.status = kHyperVStatusFailure;
      break;
  }

  //
  // Send response back to Hyper-V. The packet size will always be the same as the original inbound one.
  //
  shutdownMsg->header.flags = kVMBusICFlagTransaction | kVMBusICFlagResponse;
  _hvDevice->writeInbandPacket(shutdownMsg, pktDataLength, false);

  //
  // Shutdown/restart machine if requested. This should not return.
  //
  if (doShutdown) {
    HVDBGLOG("Shutdown request received, notifying userspace");
    performShutdown(&shutdownMsg->shutdown);
  }
}

bool HyperVShutdown::handleShutdown(VMBusICMessageShutdownData *shutdownData) {
  bool result       = false;
  UInt32 packetSize = shutdownData->header.dataSize + sizeof (shutdownData->header);

  if (packetSize < __offsetof (VMBusICMessageShutdownData, reason)) {
    HVSYSLOG("Shutdown packet is invalid size (%u bytes)", packetSize);
    return false;
  }
  HVDBGLOG("Shutdown request received: flags 0x%X, reason 0x%X", shutdownData->flags, shutdownData->reason);

  result = checkShutdown(shutdownData);
  shutdownData->header.status = result ? kHyperVStatusSuccess : kHyperVStatusFailure;
  return result;
}

bool HyperVShutdown::checkShutdown(VMBusICMessageShutdownData *shutdownData) {
  switch (shutdownData->flags) {
    case kVMBusICShutdownFlagsShutdown:
    case kVMBusICShutdownFlagsShutdownForced:
    case kVMBusICShutdownFlagsRestart:
    case kVMBusICShutdownFlagsRestartForced:
      if (_userClientInstance != nullptr) {
        return _userClientInstance->canShutdown();
      } else {
        HVSYSLOG("Unable to request shutdown (shutdown daemon is not running)");
      }
      break;

    default:
      HVSYSLOG("Invalid shutdown flags %u");
      break;
  }

  return false;
}

void HyperVShutdown::performShutdown(VMBusICMessageShutdownData *shutdownData) {
  switch (shutdownData->flags) {
    case kVMBusICShutdownFlagsShutdown:
    case kVMBusICShutdownFlagsShutdownForced:
      HVDBGLOG("Performing shutdown");
      _userClientInstance->doShutdown(false);
      break;

    case kVMBusICShutdownFlagsRestart:
    case kVMBusICShutdownFlagsRestartForced:
      HVDBGLOG("Performing restart");
      _userClientInstance->doShutdown(true);
      break;

    default:
      HVSYSLOG("Invalid shutdown flags %u");
      break;
  }
}
