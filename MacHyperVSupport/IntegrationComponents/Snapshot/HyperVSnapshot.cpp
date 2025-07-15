//
//  HyperVSnapshot.cpp
//  Hyper-V snapshot (VSS) driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "HyperVSnapshot.hpp"

OSDefineMetaClassAndStructors(HyperVSnapshot, super);

static const VMBusVersion snapshotVersions[] = {
  { kHyperVSnapshotVersionV5_0 }
};

bool HyperVSnapshot::start(IOService *provider) {
  if (HVCheckOffArg()) {
    HVSYSLOG("Disabling Hyper-V Snapshot due to boot arg");
    return false;
  }

  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    return false;
  }

  HVCheckDebugArgs();
  setICDebug(debugEnabled);

  registerService();
  HVDBGLOG("Initialized Hyper-V Snapshot");
  return true;
}

void HyperVSnapshot::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V Snapshot");
  super::stop(provider);
}

bool HyperVSnapshot::open(IOService *forClient, IOOptionBits options, void *arg) {
  HyperVSnapshotUserClient *hvSnapshotUserClient = OSDynamicCast(HyperVSnapshotUserClient, forClient);
  if ((hvSnapshotUserClient == nullptr) || (_userClientInstance != nullptr)) {
    return false;
  }

  if (!super::open(forClient, options, arg)) {
    return false;
  }

  _userClientInstance = hvSnapshotUserClient;
  return true;
}

void HyperVSnapshot::close(IOService *forClient, IOOptionBits options) {
  _userClientInstance = nullptr;
  super::close(forClient, options);
}

void HyperVSnapshot::handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  HyperVSnapshotMessage *snapshotMsg = reinterpret_cast<HyperVSnapshotMessage*>(pktData);
  IOReturn              status;

  switch (snapshotMsg->icHeader.type) {
    case kVMBusICMessageTypeNegotiate:
      //
      // Determine supported protocol version and communicate back to Hyper-V.
      //
      if (!processNegotiationResponse(&snapshotMsg->negotiate, snapshotVersions, arrsize(snapshotVersions))) {
        HVSYSLOG("Failed to determine a supported Hyper-V Snapshot version");
        snapshotMsg->icHeader.status = kHyperVStatusFailure;
      }
      break;

    case kVMBusICMessageTypeSnapshot:
      HVDBGLOG("Attempting snapshot operation type %u", snapshotMsg->snapshotHeader.type);
      if (_userClientInstance == nullptr) {
        HVSYSLOG("Unable to perform snapshot (snapshot daemon is not running)");
        snapshotMsg->icHeader.status = kHyperVStatusFailure;
        break;
      }

      switch (snapshotMsg->snapshotHeader.type) {
        case kHyperVSnapshotMessageTypeHotBackup:
          HVDBGLOG("Hot backup is starting");
          if (pktDataLength < sizeof (snapshotMsg->checkFeature)) {
            HVSYSLOG("Hot backup request packet is invalid size (%u bytes)", pktDataLength);
            status = kIOReturnUnsupported;
            break;
          }

          status = _userClientInstance->checkSnapshotAbility();
          if (status == kIOReturnSuccess) {
            snapshotMsg->checkFeature.flags = kHyperVSnapshotMessageCheckFeatureFlagNoAutoRecovery;
          }
          break;

        case kHyperVSnapshotMessageTypeFreeze:
          HVDBGLOG("Filesystem freeze requested");
          status = _userClientInstance->doFreeze();
          break;

        case kHyperVSnapshotMessageTypeThaw:
          HVDBGLOG("Filesystem thaw requested");
          status = _userClientInstance->doThaw();
          break;

        default:
          HVDBGLOG("Unknown snapshot operation type %u", snapshotMsg->snapshotHeader.type);
          status = kIOReturnUnsupported;
          break;
      }

      //
      // Report status to Hyper-V.
      //
      switch (status) {
        case kIOReturnSuccess:
          snapshotMsg->icHeader.status = kHyperVStatusSuccess;
          break;

        default:
          snapshotMsg->icHeader.status = kHyperVStatusFailure;
          break;
      }
      break;

    default:
      HVDBGLOG("Unknown IC message type %u", snapshotMsg->icHeader.type);
      snapshotMsg->icHeader.status = kHyperVStatusFailure;
      break;
  };

  //
  // Send response back to Hyper-V. The packet size will always be the same as the original inbound one.
  //
  snapshotMsg->icHeader.flags = kVMBusICFlagTransaction | kVMBusICFlagResponse;
  _hvDevice->writeInbandPacket(snapshotMsg, pktDataLength, false);
}
