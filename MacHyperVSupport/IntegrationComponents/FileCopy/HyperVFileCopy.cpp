//
//  HyperVFileCopy.cpp
//  Hyper-V file copy driver
//
//  Copyright © 2022 flagers. All rights reserved.
//

#include "HyperVFileCopy.hpp"

OSDefineMetaClassAndStructors(HyperVFileCopy, super);

static const VMBusICVersion fileCopyVersions[] = {
  kHyperVFileCopyVersionV1_1
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
  registerService();
  return true;
}

void HyperVFileCopy::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V File Copy");
  super::stop(provider);
}

bool HyperVFileCopy::open(IOService *forClient, IOOptionBits options, void *arg) {
  HyperVFileCopyUserClient *hvFileCopyUserClient = OSDynamicCast(HyperVFileCopyUserClient, forClient);
  if (hvFileCopyUserClient == nullptr || _userClientInstance != nullptr) {
    return false;
  }

  if (!super::open(forClient, options, arg)) {
    return false;
  }

  _userClientInstance = hvFileCopyUserClient;
  _userClientInstance->retain();
  return true;
}

void HyperVFileCopy::close(IOService *forClient, IOOptionBits options) {
  OSSafeReleaseNULL(_userClientInstance);
  super::close(forClient, options);
}

void HyperVFileCopy::handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  HyperVFileCopyMessage *fileCopyMsg = (HyperVFileCopyMessage*) pktData;
  IOReturn              status;

  switch (fileCopyMsg->icHeader.type) {
    case kVMBusICMessageTypeNegotiate:
      //
      // Determine supported protocol version and communicate back to Hyper-V.
      //
      if (!processNegotiationResponse(&fileCopyMsg->negotiate, fileCopyVersions, arrsize(fileCopyVersions))) {
        HVSYSLOG("Failed to determine a supported Hyper-V File Copy version");
        fileCopyMsg->icHeader.status = kHyperVStatusFailure;
      }
      break;

    case kVMBusICMessageTypeFileCopy:
      HVDBGLOG("Attempting file copy operation of type 0x%X", fileCopyMsg->fileCopyHeader.type);
      if (_userClientInstance == nullptr) {
        HVSYSLOG("Unable to start file copy (file copy daemon is not running)");
        fileCopyMsg->icHeader.status = kHyperVStatusFailure;
        break;
      }

      switch (fileCopyMsg->fileCopyHeader.type) {
        case kHyperVFileCopyMessageTypeStartCopy:
          status = _userClientInstance->startFileCopy(fileCopyMsg->startCopy.fileName, fileCopyMsg->startCopy.filePath,
                                                      fileCopyMsg->startCopy.flags, fileCopyMsg->startCopy.fileSize);
          break;

        case kHyperVFileCopyMessageTypeWriteToFile:
          status = _userClientInstance->writeFileFragment(fileCopyMsg->dataFragment.offset,
                                                          fileCopyMsg->dataFragment.data, fileCopyMsg->dataFragment.size);
          break;

        case kHyperVFileCopyMessageTypeCompleteCopy:
          status = _userClientInstance->completeFileCopy();
          break;

        case kHyperVFileCopyMessageTypeCancelCopy:
          status = _userClientInstance->cancelFileCopy();
          break;

        default:
          status = kIOReturnUnsupported;
          break;
      }

      switch (status) {
        case kIOReturnSuccess:
          fileCopyMsg->icHeader.status = kHyperVStatusSuccess;
          break;

        case kIOReturnStillOpen:
          fileCopyMsg->icHeader.status = kHyperVStatusAlreadyExists;
          break;

        case kIOReturnNoSpace:
          fileCopyMsg->icHeader.status = kHyperVStatusDiskFull;
          break;

        default:
          fileCopyMsg->icHeader.status = kHyperVStatusFailure;
          break;
      }
      break;

    default:
      HVDBGLOG("Unknown file copy message type %u", fileCopyMsg->fileCopyHeader.type);
      fileCopyMsg->icHeader.status = kHyperVStatusFailure;
      break;
  }

  //
  // Send response back to Hyper-V. The packet size will always be the same as the original inbound one.
  //
  fileCopyMsg->icHeader.flags = kVMBusICFlagTransaction | kVMBusICFlagResponse;
  _hvDevice->writeInbandPacket(fileCopyMsg, pktDataLength, false);
}
