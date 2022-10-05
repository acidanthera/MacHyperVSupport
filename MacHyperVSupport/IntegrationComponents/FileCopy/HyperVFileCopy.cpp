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
  VMBusICMessageFileCopy *fileCopyMsg = (VMBusICMessageFileCopy*) pktData;
  HyperVUserClientFileCopy userClientMsg;
  UInt8 unicodeNul[3] = { 0xE2, 0x90, 0x80 };
  void *unicodeNulLoc;
  size_t nlen;
  size_t plen;

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
      if (!_hvDevice->getHvController()->checkUserClient() || !isRegistered) {
        HVDBGLOG("Userspace or user client is not ready yet");
        fileCopyMsg->header.status = kHyperVStatusFail;
        break;
      }
      switch (fileCopyMsg->fcopyHeader.operation) {
        case kVMBusICFileCopyOperationStartFileCopy:
        case kVMBusICFileCopyOperationWriteToFile:
        case kVMBusICFileCopyOperationCompleteFileCopy:
        case kVMBusICFileCopyOperationCancelFileCopy:
          memset(&userClientMsg, 0, sizeof (userClientMsg));
          if (fileCopyMsg->fcopyHeader.operation == kVMBusICFileCopyOperationStartFileCopy) {
            memset(&userClientMsg.operationData.startCopy.fileName, 0, PATH_MAX);
            utf8_encodestr(fileCopyMsg->startCopy.fileName, kHyperVFileCopyMaxPath * 2, (UInt8*)&userClientMsg.operationData.startCopy.fileName, &nlen, PATH_MAX, '_', UTF_LITTLE_ENDIAN);
            unicodeNulLoc = lilu_os_memmem((UInt8*)&userClientMsg.operationData.startCopy.fileName, PATH_MAX, &unicodeNul, sizeof (unicodeNul));
            if (!unicodeNulLoc) {
              fileCopyMsg->header.status = kHyperVStatusFail;
              break;
            }
            *(char *)unicodeNulLoc = 0x00;
            
            memset(&userClientMsg.operationData.startCopy.filePath, 0, PATH_MAX);
            utf8_encodestr(fileCopyMsg->startCopy.filePath, kHyperVFileCopyMaxPath * 2, (UInt8*)&userClientMsg.operationData.startCopy.filePath, &plen, PATH_MAX, '/', UTF_LITTLE_ENDIAN);
            unicodeNulLoc = lilu_os_memmem((UInt8*)&userClientMsg.operationData.startCopy.filePath, PATH_MAX, &unicodeNul, sizeof (unicodeNul));
            if (!unicodeNulLoc) {
              fileCopyMsg->header.status = kHyperVStatusFail;
              break;
            }
            *(char *)unicodeNulLoc = 0x00;
            
            userClientMsg.operationData.startCopy.fileSize = fileCopyMsg->startCopy.fileSize;
            userClientMsg.operationData.startCopy.copyFlags = (HyperVUserClientFileCopyFlags)fileCopyMsg->startCopy.copyFlags;
            
            HVSYSLOG("File copy attempted for file %s at path %s", &userClientMsg.operationData.startCopy.fileName, &userClientMsg.operationData.startCopy.filePath);
          } else {
            memcpy(&userClientMsg.operationData, fileCopyMsg + sizeof (VMBusICMessageFileCopyHeader), pktDataLength - sizeof (VMBusICMessageFileCopyHeader));
          }
          userClientMsg.operation = (HyperVUserClientFileCopyOperation)fileCopyMsg->fcopyHeader.operation;
          HVDBGLOG("Sending file copy operation type %u", fileCopyMsg->fcopyHeader.operation);
          _hvDevice->getHvController()->notifyUserClient(kHyperVUserClientNotificationTypeFileCopy, &userClientMsg, sizeof (userClientMsg));
          if (sleepForUserspace(15) == THREAD_TIMED_OUT) {
            fileCopyMsg->header.status = kHyperVUserClientStatusTimedOut;
            break;
          }
          fileCopyMsg->header.status = this->status;
          
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
bool HyperVFileCopy::_userClientAvailable(void *target, void *ref, IOService *newService) {
#else
bool HyperVFileCopy::_userClientAvailable(void *target, void *ref, IOService *newService, IONotifier *notifier) {
#endif
  HyperVFileCopy *fcopy = (HyperVFileCopy *) target;
  if (fcopy) {
    fcopy->HVDBGLOG("User client is now available; attempting registration");
    if (!fcopy->_hvDevice->getHvController()->registerUserClientDriver(fcopy)) {
      fcopy->HVSYSLOG("Failed to register driver in user client");
    } else { fcopy->isRegistered = true; }
  }
  return true;
}
  
IOReturn HyperVFileCopy::callPlatformFunction(const OSSymbol *functionName,
                              bool waitForFunction, void *param1,
                              void *param2, void *param3, void *param4) {
  UInt64 *status;
  if (functionName->isEqualTo("responseFromUserspace")) {
    status = (UInt64 *) param1;
    HVDBGLOG("Got response from userspace: 0x%x", *status);
    this->status = *status;
    wakeForUserspace();
    return kIOReturnSuccess;
  }
  return super::callPlatformFunction(functionName, waitForFunction,
                                     param1, param2, param3, param4);
}
