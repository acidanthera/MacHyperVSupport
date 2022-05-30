//
//  HyperVShutdown.cpp
//  Hyper-V guest shutdown driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVShutdown.hpp"
#include "HyperVPlatformProvider.hpp"

#define super HyperVICService

#define HVSYSLOG(str, ...) HVSYSLOG_PRINT("HyperVShutdown", str, ## __VA_ARGS__)
#define HVDBGLOG(str, ...) HVDBGLOG_PRINT("HyperVShutdown", str, ## __VA_ARGS__)

OSDefineMetaClassAndStructors(HyperVShutdown, super);

bool HyperVShutdown::start(IOService *provider) {
  if (!super::start(provider)) {
    return false;
  }

  HVSYSLOG("Initialized Hyper-V Guest Shutdown");
  return true;
}

bool HyperVShutdown::processMessage() {
  VMBusICMessageShutdown shutdownMsg;

  //
  // Ignore errors and the acknowledgement interrupt (no data to read).
  //
  UInt32 pktDataLength;
  if (!hvDevice->nextInbandPacketAvailable(&pktDataLength) || pktDataLength > sizeof (shutdownMsg)) {
    return false;
  }

  //
  // Read and parse inbound inband packet.
  //
  if (hvDevice->readInbandCompletionPacket(&shutdownMsg, sizeof (shutdownMsg), NULL) != kIOReturnSuccess) {
    return false;
  }

  bool doShutdown = false;
  switch (shutdownMsg.header.type) {
    case kVMBusICMessageTypeNegotiate:
      createNegotiationResponse(&shutdownMsg.negotiate, 3, 3);
      break;

    case kVMBusICMessageTypeShutdown:
      doShutdown = handleShutdown(&shutdownMsg.shutdown);
      break;

    default:
      HVDBGLOG("Unknown shutdown message type %u", shutdownMsg.header.type);
      shutdownMsg.header.status = kHyperVStatusFail;
      break;
  }

  //
  // Send response back to Hyper-V. The packet size will always be the same as the original inbound one.
  //
  shutdownMsg.header.flags = kVMBusICFlagTransaction | kVMBusICFlagResponse;
  hvDevice->writeInbandPacket(&shutdownMsg, pktDataLength, false);

  //
  // Shutdown machine if requested. This should not return.
  //
  if (doShutdown) {
    HVSYSLOG("Shutting down system");
    HyperVPlatformProvider::getInstance()->shutdownSystem();
  }
  return true;
}

bool HyperVShutdown::handleShutdown(VMBusICMessageShutdownData *shutdownData) {
  HVDBGLOG("Shutdown request received: flags 0x%X, reason 0x%X", shutdownData->flags, shutdownData->reason);

  //
  // Report back to Hyper-V if we can shutdown system.
  //
  bool result = false;
  HyperVPlatformProvider *provider = HyperVPlatformProvider::getInstance();
  if (provider != NULL) {
    result = provider->canShutdownSystem();
  }

  shutdownData->header.status = result ? kHyperVStatusSuccess : kHyperVStatusFail;
  if (!result) {
    HVSYSLOG("Platform does not support shutdown");
  }
  return result;
}
