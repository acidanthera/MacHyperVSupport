//
//  HyperVShutdown.cpp
//  Hyper-V guest shutdown driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVShutdown.hpp"
#include "HyperVPlatformProvider.hpp"

#include <Headers/kern_api.hpp>

OSDefineMetaClassAndStructors(HyperVShutdown, super);

bool HyperVShutdown::start(IOService *provider) {
  if (!super::start(provider)) {
    return false;
  }
  
  debugEnabled = checkKernelArgument("-hvshutdbg");
  setICDebug(debugEnabled);
  hvDevice->setDebugMessagePrinting(checkKernelArgument("-hvshutmsgdbg"));
  registerService();
  
  HVDBGLOG("Initialized Hyper-V Guest Shutdown");
  return true;
}

void HyperVShutdown::stop(IOService *provider) {
  super::stop(provider);
  HVDBGLOG("Stopped Hyper-V Guest Shutdown");
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
    HVSYSLOG("Shutdown request received, notifying userspace");
    messageClients(kHyperVShutdownMessageTypePerformShutdown);
  }
  return true;
}

bool HyperVShutdown::handleShutdown(VMBusICMessageShutdownData *shutdownData) {
  HVDBGLOG("Shutdown request received: flags 0x%X, reason 0x%X", shutdownData->flags, shutdownData->reason);

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
