//
//  HyperVHeartbeat.cpp
//  Hyper-V heartbeat driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVHeartbeat.hpp"

#include <IOKit/IOPlatformExpert.h>

#define super HyperVICService

#define SYSLOG(str, ...) SYSLOG_PRINT("HyperVHeartbeat", str, ## __VA_ARGS__)
#define DBGLOG(str, ...) DBGLOG_PRINT("HyperVHeartbeat", str, ## __VA_ARGS__)

OSDefineMetaClassAndStructors(HyperVHeartbeat, super);

bool HyperVHeartbeat::start(IOService *provider) {
  DBGLOG("Initializing Hyper-V Heartbeat");
  return super::start(provider);
}

bool HyperVHeartbeat::processMessage() {
  VMBusICMessageHeartbeat heartbeatMsg;

  //
  // Ignore errors and the acknowledgement interrupt (no data to read).
  //
  UInt32 pktDataLength;
  if (!hvDevice->nextInbandPacketAvailable(&pktDataLength) || pktDataLength > sizeof (heartbeatMsg)) {
    return false;
  }

  //
  // Read and parse inbound inband packet.
  //
  if (hvDevice->readInbandPacket(&heartbeatMsg, sizeof (heartbeatMsg), NULL) != kIOReturnSuccess) {
    return false;
  }
  switch (heartbeatMsg.header.type) {
    case kVMBusICMessageTypeNegotiate:
      firstHeartbeatReceived = false;
      createNegotiationResponse(&heartbeatMsg.negotiate, 3, 3);
      break;

    case kVMBusICMessageTypeHeartbeat:
      //
      // Increment sequence.
      // Host will increment this further before sending a message back.
      //
      //DBGLOG("Got heartbeat, seq = %u", heartbeatMsg.heartbeat.sequence);
      heartbeatMsg.heartbeat.sequence++;

      if (!firstHeartbeatReceived) {
        firstHeartbeatReceived = true;
        SYSLOG("Initialized Hyper-V Heartbeat");
      }
      break;

    default:
      DBGLOG("Unknown heartbeat message type %u", heartbeatMsg.header.type);
      heartbeatMsg.header.status = kHyperVStatusFail;
      break;
  }

  //
  // Send response back to Hyper-V. The packet size will always be the same as the original inbound one.
  //
  heartbeatMsg.header.flags = kVMBusICFlagTransaction | kVMBusICFlagResponse;
  hvDevice->writeInbandPacket(&heartbeatMsg, pktDataLength, false);
  return true;
}
