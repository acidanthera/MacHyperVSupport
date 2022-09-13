//
//  HyperVShutdown.cpp
//  Hyper-V guest shutdown driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVShutdown.hpp"

OSDefineMetaClassAndStructors(HyperVShutdown, super);

static const VMBusICVersion shutdownVersions[] = {
  kHyperVShutdownVersionV3_2,
  kHyperVShutdownVersionV3_1,
  kHyperVShutdownVersionV3_0,
  kHyperVShutdownVersionV1_0
};

bool HyperVShutdown::start(IOService *provider) {
  if (!super::start(provider)) {
    HVSYSLOG("Superclass start function failed");
    return false;
  }
  
  HVCheckDebugArgs();
  setICDebug(debugEnabled);
  
  if (HVCheckOffArg()) {
    HVSYSLOG("Disabling Hyper-V Guest Shutdown due to boot arg");
    super::stop(provider);
    return false;
  }
  
  HVDBGLOG("Initialized Hyper-V Guest Shutdown");
  registerService();
  
  return true;
}

void HyperVShutdown::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V Guest Shutdown");
  super::stop(provider);
}

bool HyperVShutdown::open(IOService *forClient, IOOptionBits options, void *arg) {
  if (userClientInstance != nullptr) {
    return false;
  }
  
  if (!super::open(forClient, options, arg)) {
    return false;
  }
  
  userClientInstance = forClient;
  return true;
}

void HyperVShutdown::close(IOService *forClient, IOOptionBits options) {
  userClientInstance = nullptr;
  super::close(forClient, options);
}

void HyperVShutdown::handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  VMBusICMessageShutdown *shutdownMsg = (VMBusICMessageShutdown*) pktData;

  bool doShutdown = false;
  switch (shutdownMsg->header.type) {
    case kVMBusICMessageTypeNegotiate:
      if (!processNegotiationResponse(&shutdownMsg->negotiate, shutdownVersions, arrsize(shutdownVersions))) {
        HVSYSLOG("Failed to determine a supported Hyper-V Shutdown version");
        return;
      }

    case kVMBusICMessageTypeShutdown:
      doShutdown = handleShutdown(&shutdownMsg->shutdown);
      break;

    default:
      HVDBGLOG("Unknown shutdown message type %u", shutdownMsg->header.type);
      shutdownMsg->header.status = kHyperVStatusFail;
      break;
  }

  //
  // Send response back to Hyper-V. The packet size will always be the same as the original inbound one.
  //
  shutdownMsg->header.flags = kVMBusICFlagTransaction | kVMBusICFlagResponse;
  hvDevice->writeInbandPacket(&shutdownMsg, pktDataLength, false);

  //
  // Shutdown machine if requested. This should not return.
  //
  if (doShutdown) {
    HVDBGLOG("Shutdown request received, notifying userspace");
    messageClients(kHyperVShutdownMessageTypePerformShutdown);
  }
}

bool HyperVShutdown::handleShutdown(VMBusICMessageShutdownData *shutdownData) {
  HVDBGLOG("Shutdown request received: flags 0x%X, reason 0x%X", shutdownData->flags, shutdownData->reason); // TODO: Flags may indicate restart or shutdown, need to handle.

  //
  // Send message to userclients to see if we can shutdown.
  //
  bool result       = false;
  bool clientResult = false;
  if (userClientInstance != nullptr) {
    IOReturn status = messageClient(kHyperVShutdownMessageTypeShutdownRequested, userClientInstance, &clientResult, sizeof (clientResult));
    result = (status == kIOReturnSuccess) && clientResult;
    HVDBGLOG("Response from client: status 0x%X result %u", status, result);
  }

  shutdownData->header.status = result ? kHyperVStatusSuccess : kHyperVStatusFail;
  if (!result) {
    HVSYSLOG("Unable to request shutdown (shutdown daemon is not running)");
  }
  return result;
}
