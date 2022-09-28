//
//  HyperVTimeSync.cpp
//  Hyper-V time synchronization driver
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVTimeSync.hpp"

OSDefineMetaClassAndStructors(HyperVTimeSync, super);

static const VMBusICVersion timeSyncVersions[] = {
  kHyperVTimeSyncVersionV4_0,
  kHyperVTimeSyncVersionV3_0,
  kHyperVTimeSyncVersionV1_0
};

bool HyperVTimeSync::start(IOService *provider) {
  if (HVCheckOffArg()) {
    HVSYSLOG("Disabling Hyper-V Time Synchronization due to boot arg");
    return false;
  }

  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    return false;
  }

  HVCheckDebugArgs();
  setICDebug(debugEnabled);

  HVDBGLOG("Initialized Hyper-V Time Synchronization");
  return true;
}

void HyperVTimeSync::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V Time Synchronization");
  super::stop(provider);
}

void HyperVTimeSync::handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  VMBusICMessageTimeSync *timeSyncMsg       = (VMBusICMessageTimeSync*) pktData;
  VMBusICVersion         timeSyncRefVersion = kHyperVTimeSyncVersionV4_0;
  UInt32                 packetSize;

  switch (timeSyncMsg->header.type) {
    case kVMBusICMessageTypeNegotiate:
      //
      // Determine supported protocol version and communicate back to Hyper-V.
      //
      if (!processNegotiationResponse(&timeSyncMsg->negotiate, timeSyncVersions, arrsize(timeSyncVersions), &_timeSyncCurrentVersion)) {
        HVSYSLOG("Failed to determine a supported Hyper-V Time Synchronization version");
        timeSyncMsg->header.status = kHyperVStatusFail;
      }
      break;

    case kVMBusICMessageTypeTimeSync:
      //
      // Time sync request.
      //
      packetSize = timeSyncMsg->header.dataSize + sizeof (timeSyncMsg->header);
      if (_timeSyncCurrentVersion.major >= timeSyncRefVersion.major && _timeSyncCurrentVersion.minor >= timeSyncRefVersion.minor) {
        if (packetSize < __offsetof (VMBusICMessageTimeSyncRefData, parentTime)) {
          HVSYSLOG("Time sync ref packet is invalid size (%u bytes)", packetSize);
          timeSyncMsg->header.status = kHyperVStatusFail;
          break;
        }
        handleTimeAdjust(timeSyncMsg->timeSyncRef.parentTime, timeSyncMsg->timeSyncRef.referenceTime, timeSyncMsg->timeSyncRef.flags);
      } else {
        if (packetSize < __offsetof (VMBusICMessageTimeSyncData, parentTime)) {
          HVSYSLOG("Time sync packet is invalid size (%u bytes)", packetSize);
          timeSyncMsg->header.status = kHyperVStatusFail;
          break;
        }
        handleTimeAdjust(timeSyncMsg->timeSync.parentTime, 0, timeSyncMsg->timeSync.flags);
      }
      break;

    default:
      HVDBGLOG("Unknown shutdown message type %u", timeSyncMsg->header.type);
      timeSyncMsg->header.status = kHyperVStatusFail;
      break;
  }

  //
  // Send response back to Hyper-V. The packet size will always be the same as the original inbound one.
  //
  timeSyncMsg->header.flags = kVMBusICFlagTransaction | kVMBusICFlagResponse;
  _hvDevice->writeInbandPacket(timeSyncMsg, pktDataLength, false);
}

void HyperVTimeSync::handleTimeAdjust(UInt64 hostTime, UInt64 referenceTime, VMBusICTimeSyncFlags flags) {
  UInt64 hvTimeNs;
  HyperVUserClientTimeData timeData;

  HVDBGLOG("Time sync request %u received (host time: %llu, ref time: %llu)", flags, hostTime, referenceTime);

  if (flags != kVMBusICTimeSyncFlagsSync) {
    return;
  }

  //
  // Calculate epoch.
  //
  hvTimeNs = (hostTime - kHyperVTimeSyncTimeBase) * kHyperVTimerNanosecondFactor;
  timeData.seconds      = (clock_sec_t)(hvTimeNs / NSEC_PER_SEC);
  timeData.microseconds = 0;//hvTimeNs / NSEC_PER_USEC;

  _hvDevice->notifyUserClient(kHyperVUserClientNotificationTypeTimeSync, &timeData, sizeof (timeData));
}
