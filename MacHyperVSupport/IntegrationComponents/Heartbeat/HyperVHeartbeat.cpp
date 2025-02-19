//
//  HyperVHeartbeat.cpp
//  Hyper-V heartbeat driver
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#include "HyperVHeartbeat.hpp"

OSDefineMetaClassAndStructors(HyperVHeartbeat, super);

static const VMBusVersion heartbeatVersions[] = {
  { kHyperVHeartbeatVersionV3 },
  { kHyperVHeartbeatVersionV1 }
};

bool HyperVHeartbeat::start(IOService *provider) {
  if (HVCheckOffArg()) {
    HVSYSLOG("Disabling Hyper-V Heartbeat due to boot arg");
    return false;
  }

  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    return false;
  }

  HVCheckDebugArgs();
  setICDebug(debugEnabled);

  HVDBGLOG("Initializing Hyper-V Heartbeat");
  return true;
}

void HyperVHeartbeat::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V Heartbeat");
  super::stop(provider);
}

void HyperVHeartbeat::handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  VMBusICMessageHeartbeat *heartbeatMsg = (VMBusICMessageHeartbeat*) pktData;
  UInt32                  packetSize;

  switch (heartbeatMsg->header.type) {
    case kVMBusICMessageTypeNegotiate:
      //
      // Determine supported protocol version and communicate back to Hyper-V.
      //
      firstHeartbeatReceived = false;
      if (!processNegotiationResponse(&heartbeatMsg->negotiate, heartbeatVersions, arrsize(heartbeatVersions))) {
        HVSYSLOG("Failed to determine a supported Hyper-V Heartbeat version");
        heartbeatMsg->header.status = kHyperVStatusFailure;
      }
      break;

    case kVMBusICMessageTypeHeartbeat:
      //
      // Normal heartbeat packet.
      // The sequence number is incremented twice per cycle, once by the guest and once by Hyper-V.
      //
      packetSize = heartbeatMsg->header.dataSize + sizeof (heartbeatMsg->header);
      if (packetSize < __offsetof (VMBusICMessageHeartbeatSequence, sequence)) {
        HVSYSLOG("Heartbeat packet is invalid size (%u bytes)", packetSize);
        heartbeatMsg->header.status = kHyperVStatusFailure;
        break;
      }

      HVDBGLOG("Got heartbeat, seq = %u", heartbeatMsg->heartbeat.sequence);
      heartbeatMsg->heartbeat.sequence++;

      if (!firstHeartbeatReceived) {
        firstHeartbeatReceived = true;
        HVDBGLOG("Initialized Hyper-V Heartbeat");
      }
      break;

    default:
      HVDBGLOG("Unknown heartbeat message type %u", heartbeatMsg->header.type);
      heartbeatMsg->header.status = kHyperVStatusFailure;
      break;
  }

  //
  // Send response back to Hyper-V. The packet size will always be the same as the original inbound one.
  //
  heartbeatMsg->header.flags = kVMBusICFlagTransaction | kVMBusICFlagResponse;
  _hvDevice->writeInbandPacket(heartbeatMsg, pktDataLength, false);
}
