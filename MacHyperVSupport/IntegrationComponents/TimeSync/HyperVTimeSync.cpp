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

  registerService();
  HVDBGLOG("Initialized Hyper-V Time Synchronization");
  return true;
}

void HyperVTimeSync::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V Time Synchronization");
  super::stop(provider);
}

bool HyperVTimeSync::open(IOService *forClient, IOOptionBits options, void *arg) {
  HyperVTimeSyncUserClient *hvTimeSyncUserClient = OSDynamicCast(HyperVTimeSyncUserClient, forClient);
  if (hvTimeSyncUserClient == nullptr || _userClientInstance != nullptr) {
    return false;
  }

  if (!super::open(forClient, options, arg)) {
    return false;
  }

  _userClientInstance = hvTimeSyncUserClient;
  return true;
}

void HyperVTimeSync::close(IOService *forClient, IOOptionBits options) {
  _userClientInstance = nullptr;
  super::close(forClient, options);
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
        timeSyncMsg->header.status = kHyperVStatusFailure;
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
          timeSyncMsg->header.status = kHyperVStatusFailure;
          break;
        }
        handleTimeAdjust(timeSyncMsg->timeSyncRef.parentTime, timeSyncMsg->timeSyncRef.referenceTime, timeSyncMsg->timeSyncRef.flags);
      } else {
        if (packetSize < __offsetof (VMBusICMessageTimeSyncData, parentTime)) {
          HVSYSLOG("Time sync packet is invalid size (%u bytes)", packetSize);
          timeSyncMsg->header.status = kHyperVStatusFailure;
          break;
        }
        handleTimeAdjust(timeSyncMsg->timeSync.parentTime, 0, timeSyncMsg->timeSync.flags);
      }
      break;

    default:
      HVDBGLOG("Unknown shutdown message type %u", timeSyncMsg->header.type);
      timeSyncMsg->header.status = kHyperVStatusFailure;
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
  UInt64 vmTimeNs;
  UInt64 diffTimeNs;
  UInt64 hvRefTimeAdjust = 0;

  clock_sec_t  clockSeconds;
  clock_nsec_t clockNanoseconds;
  UInt64       userSeconds;
  UInt32       userMicroseconds;

  //
  // Only handle regular time samples and forced syncs.
  //
  if (!(flags & (kVMBusICTimeSyncFlagsSample | kVMBusICTimeSyncFlagsSync))) {
    return;
  }
  HVDBGLOG("Time sync request received (flags: 0x%X, host time: %llu, ref time: %llu)", flags, hostTime, referenceTime);

  //
  // Adjust time based on provided and current reference counter.
  //
  if (referenceTime != 0 && _hvDevice->getHvController()->isTimeRefCounterSupported()) {
    hvRefTimeAdjust = _hvDevice->getHvController()->readTimeRefCounter() - referenceTime;
    HVDBGLOG("Time sync will be adjusted by %llu", hvRefTimeAdjust);
  }

  //
  // Calculate epoch and break out into seconds and microseconds remainder.
  //
  hvTimeNs         = (hostTime - kHyperVTimeSyncTimeBase + hvRefTimeAdjust) * kHyperVTimerNanosecondFactor;
  userSeconds      = (clock_sec_t)(hvTimeNs / NSEC_PER_SEC);
  userMicroseconds = (hvTimeNs % NSEC_PER_SEC) / NSEC_PER_USEC;
  HVDBGLOG("New system time: (%llu seconds, %u microseconds, %llu total nanoseconds)", userSeconds, userMicroseconds, hvTimeNs);

  //
  // Only handle sample if current time has drifted too far.
  //
  if (flags & kVMBusICTimeSyncFlagsSample) {
    clock_get_calendar_nanotime(&clockSeconds, &clockNanoseconds);
    vmTimeNs = (clockSeconds * NSEC_PER_SEC) + clockNanoseconds;
    HVDBGLOG("Current system time: (%llu seconds, %u nanoseconds, %llu total nanoseconds)", clockSeconds, clockNanoseconds, vmTimeNs);

    if (hvTimeNs > vmTimeNs) {
      diffTimeNs = hvTimeNs - vmTimeNs;
    } else {
      diffTimeNs = vmTimeNs - hvTimeNs;
    }
    HVDBGLOG("Time difference: %llu nanoseconds", diffTimeNs);

    if (diffTimeNs <= kHyperVTimeSyncDiffThreshold) {
      return;
    }
    HVDBGLOG("Time difference is above %llu nanosecond threshold", kHyperVTimeSyncDiffThreshold);
  }

  //
  // Update time via userspace daemon.
  //
  if (_userClientInstance != nullptr) {
    _userClientInstance->doTimeSync(userSeconds, userMicroseconds);
  }
}
