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
  
  lock = IOLockAlloc();
  if (lock == NULL) {
    HVSYSLOG("Failed to allocate lock for userspace transactions");
    return false;
  }
  
#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_6
  _userClientNotify = addNotification(gIOPublishNotification,
                      serviceMatching("HyperVUserClient"),
                      &HyperVFileCopy::_userClientAvailable,
                      this, 0 );
#else
  _userClientNotify = addMatchingNotification(gIOPublishNotification,
                      serviceMatching("HyperVUserClient"),
                      &HyperVFileCopy::_userClientAvailable,
                      this, 0 );
#endif

  HVDBGLOG("Initializing Hyper-V File Copy");
  registerService();
  return true;
}

void HyperVFileCopy::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V File Copy");
  _hvDevice->getHvController()->deregisterUserClientDriver(this);
  IOLockFree(lock);
  super::stop(provider);
}

void HyperVFileCopy::handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  _fileCopyPkt = (VMBusICMessageFileCopy*) pktData;
  HyperVUserClientFileCopyMessage fileCopyMsg;

  switch (_fileCopyPkt->header.type) {
    case kVMBusICMessageTypeNegotiate:
      //
      // Determine supported protocol version and communicate back to Hyper-V.
      //
      if (!processNegotiationResponse(&_fileCopyPkt->negotiate, fcopyVersions, arrsize(fcopyVersions))) {
        HVSYSLOG("Failed to determine a supported Hyper-V File Copy version");
        _fileCopyPkt->header.status = kHyperVStatusFail;
      }
      break;

    case kVMBusICMessageTypeFileCopy:
      if (!_hvDevice->getHvController()->checkUserClient() || !isRegistered) {
        HVDBGLOG("Userspace or user client is not ready yet");
        _fileCopyPkt->header.status = kHyperVStatusFail;
        break;
      }
      HVDBGLOG("File copy operation type %u attempted", _fileCopyPkt->fcopyHeader.operation);
      switch (_fileCopyPkt->fcopyHeader.operation) {
        case kHyperVUserClientFileCopyOperationStartFileCopy:
        case kHyperVUserClientFileCopyOperationWriteToFile:
          fileCopyMsg.operation = _fileCopyPkt->fcopyHeader.operation;
          if (_fileCopyPkt->fcopyHeader.operation == kHyperVUserClientFileCopyOperationStartFileCopy) {
            fileCopyMsg.startCopy.fileSize = _fileCopyPkt->startCopy.fileSize;
            fileCopyMsg.startCopy.copyFlags = _fileCopyPkt->startCopy.copyFlags;
            // Ask userspace to start a copy transaction and wait for it to provide a buffer.
            _hvDevice->getHvController()->notifyUserClient(kHyperVUserClientNotificationTypeFileCopy, &fileCopyMsg, sizeof (fileCopyMsg));
            sleepForUserspace();
            _fileCopyPkt->header.status = this->status;
            
          } else if (_fileCopyPkt->fcopyHeader.operation == kHyperVUserClientFileCopyOperationWriteToFile) {
            fileCopyMsg.doCopy.offset = _fileCopyPkt->doCopy.offset;
            fileCopyMsg.doCopy.size = _fileCopyPkt->doCopy.size;
            _hvDevice->getHvController()->notifyUserClient(kHyperVUserClientNotificationTypeFileCopy, &fileCopyMsg, sizeof (fileCopyMsg));
            sleepForUserspace();
            _fileCopyPkt->header.status = this->status;
          }
          break;
        case kHyperVUserClientFileCopyOperationCompleteFileCopy:
        case kHyperVUserClientFileCopyOperationCancelFileCopy:
          fileCopyMsg.operation = _fileCopyPkt->fcopyHeader.operation;
          _hvDevice->getHvController()->notifyUserClient(kHyperVUserClientNotificationTypeFileCopy, &fileCopyMsg, sizeof (fileCopyMsg.operation));
          break;
        default:
          HVDBGLOG("Unknown file copy operation type %u", _fileCopyPkt->fcopyHeader.operation);
          _fileCopyPkt->header.status = kHyperVStatusFail;
          break;
      }
      break;

    default:
      HVDBGLOG("Unknown file copy message type %u", _fileCopyPkt->header.type);
      _fileCopyPkt->header.status = kHyperVStatusFail;
      break;
  }

  //
  // Send response back to Hyper-V. The packet size will always be the same as the original inbound one.
  //
  _fileCopyPkt->header.flags = kVMBusICFlagTransaction | kVMBusICFlagResponse;
  _hvDevice->writeInbandPacket(_fileCopyPkt, pktDataLength, false);
}

int HyperVFileCopy::sleepForUserspace(UInt32 seconds) {
  AbsoluteTime deadline = 0;
  int waitResult = THREAD_AWAKENED;
  bool computeDeadline = true;
  
  HVDBGLOG("Sleeping until response from userspace", status);
  isSleeping = true;
  IOLockLock(lock);
  while (isSleeping && waitResult != THREAD_TIMED_OUT) {
    if (seconds) {
      if (computeDeadline) {
        clock_interval_to_deadline(seconds, kSecondScale, &deadline);
        computeDeadline = false;
      }
      waitResult = IOLockSleepDeadline(lock, &isSleeping, deadline, THREAD_INTERRUPTIBLE);
    } else {
      waitResult = IOLockSleep(lock, &isSleeping, THREAD_INTERRUPTIBLE);
    }
  }
  IOLockUnlock(lock);
  return waitResult;
}

void HyperVFileCopy::wakeForUserspace() {
  HVDBGLOG("Waking after response from userspace", status);
  IOLockLock(lock);
  isSleeping = false;
  IOLockWakeup(lock, &isSleeping, false);
  IOLockUnlock(lock);
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_6
bool HyperVFileCopy::_userClientAvailable(void *target, void *ref, IOService *newService)
#else
bool HyperVFileCopy::_userClientAvailable(void *target, void *ref, IOService *newService, IONotifier *notifier)
#endif
{
  HyperVFileCopy *fcopy = (HyperVFileCopy *) target;
  HyperVUserClientFileCopyMessage fileCopyMsg;
  if (fcopy) {
    fcopy->HVDBGLOG("User client is now available; attempting registration");
    if (!fcopy->_hvDevice->getHvController()->registerUserClientDriver(fcopy)) {
      fcopy->HVSYSLOG("Failed to register driver in user client");
    } else {
      fcopy->isRegistered = true;
    }
  }
  
  return true;
}
  
IOReturn HyperVFileCopy::callPlatformFunction(const OSSymbol *functionName,
                                              bool waitForFunction, void *param1,
                                              void *param2, void *param3, void *param4) {
  HVDBGLOG("Calling function '%s'", functionName->getCStringNoCopy());
  if (functionName->isEqualTo("returnCodeFromUserspace")) {
    returnCodeFromUserspace((UInt64 *) param1);
    return kIOReturnSuccess;
  } else if (functionName->isEqualTo("getStartCopyData")) {
    getStartCopyData((HyperVUserClientFileCopyStartCopyData *) param1);
    return kIOReturnSuccess;
  }
  return super::callPlatformFunction(functionName, waitForFunction,
                                     param1, param2, param3, param4);
}

void HyperVFileCopy::returnCodeFromUserspace(UInt64 *status) {
  HVDBGLOG("Got response from userspace: 0x%x", *status);
  this->status = *status;
  wakeForUserspace();
}

void HyperVFileCopy::getStartCopyData(HyperVUserClientFileCopyStartCopyData *startCopyDataOut) {
  HVDBGLOG("Got request for file name and file path from userspace");
  convertNameAndPath(_fileCopyPkt, startCopyDataOut);
}

bool HyperVFileCopy::convertNameAndPath(VMBusICMessageFileCopy *input, HyperVUserClientFileCopyStartCopyData *output) {
  UInt8 unicodeNul[3] = { 0xE2, 0x90, 0x80 };
  void *unicodeNulLoc;
  size_t nlen;
  size_t plen;
  
  memset(output->fileName, 0, PATH_MAX);
  utf8_encodestr(input->startCopy.fileName, kHyperVFileCopyMaxPath * 2, (UInt8*)output->fileName, &nlen, PATH_MAX, '_', UTF_LITTLE_ENDIAN);
  unicodeNulLoc = lilu_os_memmem((UInt8*)output->fileName, PATH_MAX, &unicodeNul, sizeof (unicodeNul));
  if (!unicodeNulLoc) {
    input->header.status = kHyperVStatusFail;
    return false;
  }
  *(char *)unicodeNulLoc = 0x00;
  
  memset(output->filePath, 0, PATH_MAX);
  utf8_encodestr(input->startCopy.filePath, kHyperVFileCopyMaxPath * 2, (UInt8*)output->filePath, &plen, PATH_MAX, '/', UTF_LITTLE_ENDIAN);
  unicodeNulLoc = lilu_os_memmem((UInt8*)output->filePath, PATH_MAX, &unicodeNul, sizeof (unicodeNul));
  if (!unicodeNulLoc) {
    input->header.status = kHyperVStatusFail;
    return false;
  }
  *(char *)unicodeNulLoc = 0x00;

  return true;
}
