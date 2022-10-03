//
//  HyperVFileCopy.cpp
//  Hyper-V file copy driver
//
//  Copyright © 2022 flagers. All rights reserved.
//

#include "HyperVFileCopy.hpp"

OSDefineMetaClassAndStructors(HyperVFileCopy, super);

static const VMBusICVersion fcopyVersions[] = {
  kHyperVFileCopyVersionV1
};

bool HyperVFileCopy::start(IOService *provider) {
  if (HVCheckOffArg()) {
    HVSYSLOG("Disabling Hyper-V File Copy due to boot arg");
    return false;
  }

  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    return false;
  }

  HVCheckDebugArgs();
  setICDebug(debugEnabled);

  HVDBGLOG("Initializing Hyper-V File Copy");
  return true;
}

void HyperVFileCopy::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V File Copy");
  super::stop(provider);
}

void HyperVFileCopy::handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  VMBusICMessageFileCopy *fileCopyMsg = (VMBusICMessageFileCopy*) pktData;

  switch (fileCopyMsg->header.type) {
    case kVMBusICMessageTypeNegotiate:
      //
      // Determine supported protocol version and communicate back to Hyper-V.
      //
      if (!processNegotiationResponse(&fileCopyMsg->negotiate, fcopyVersions, arrsize(fcopyVersions))) {
        HVSYSLOG("Failed to determine a supported Hyper-V File Copy version");
        fileCopyMsg->header.status = kHyperVStatusFail;
      }
      break;

    case kVMBusICMessageTypeFileCopy:
      if (!_hvDevice->getHvController()->checkUserClient()) {
        HVDBGLOG("Userspace is not ready yet");
        fileCopyMsg->header.status = kHyperVStatusFail;
      }
      switch (fileCopyMsg->fcopyHeader.operation) {
        case kVMBusICFileCopyOperationStartFileCopy:
          UInt8 pbuf[PATH_MAX];
          size_t plen;
          utf8_encodestr(fileCopyMsg->startCopy.fileName, kHyperVFileCopyMaxPath, (UInt8*)&pbuf, &plen, PATH_MAX, 0, 0);
          HVSYSLOG("File copy attempted for file: %s", &pbuf);
          break;
        default:
          HVDBGLOG("Unknown file copy operation type %u", fileCopyMsg->fcopyHeader.operation);
          fileCopyMsg->header.status = kHyperVStatusFail;
          break;
      }
      break;

    default:
      HVDBGLOG("Unknown file copy message type %u", fileCopyMsg->header.type);
      fileCopyMsg->header.status = kHyperVStatusFail;
      break;
  }

  //
  // Send response back to Hyper-V. The packet size will always be the same as the original inbound one.
  //
  fileCopyMsg->header.flags = kVMBusICFlagTransaction | kVMBusICFlagResponse;
  _hvDevice->writeInbandPacket(fileCopyMsg, pktDataLength, false);
}
