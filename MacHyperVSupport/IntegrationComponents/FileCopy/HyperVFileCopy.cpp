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
  
  _hvDevice->getHvController()->registerUserClientDriverCallback(kMethodReturnFileCopy, (mach_vm_address_t)responseFromUserspace);
  lock = IOLockAlloc();
  if (lock == NULL) {
    HVSYSLOG("Failed to allocate lock for userspace transactions");
    return false;
  }

  HVDBGLOG("Initializing Hyper-V File Copy");
  registerService();
  return true;
}

void HyperVFileCopy::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V File Copy");
  _hvDevice->getHvController()->registerUserClientDriverCallback(kMethodReturnFileCopy, (mach_vm_address_t)nullptr);
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
      if (!_hvDevice->getHvController()->checkUserClient()) {
        HVDBGLOG("Userspace is not ready yet");
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
          _hvDevice->getHvController()->notifyUserClient(kHyperVUserClientNotificationTypeFileCopy, &userClientMsg, sizeof (userClientMsg));
          sleepForUserspace();
          fileCopyMsg->header.status = status;
          
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

void HyperVFileCopy::sleepForUserspace() {
  HVDBGLOG("Sleeping until response from userspace", status);
  isSleeping = true;
  IOLockLock(lock);
  while (isSleeping) {
    IOLockSleep(lock, &isSleeping, THREAD_INTERRUPTIBLE);
  }
  IOLockUnlock(lock);
}

void HyperVFileCopy::wakeForUserspace() {
  HVDBGLOG("Waking after response from userspace", status);
  IOLockLock(lock);
  isSleeping = false;
  IOLockWakeup(lock, &isSleeping, false);
  IOLockUnlock(lock);
}

void HyperVFileCopy::responseFromUserspace(int *status) {
  OSDictionary *fcopyMatching = IOService::serviceMatching("HyperVFileCopy");
  if (fcopyMatching == NULL) {
    return;
  }
  
  OSIterator *fcopyIterator = IOService::getMatchingServices(fcopyMatching);
  if (fcopyIterator == NULL) {
    return;
  }
  
  fcopyIterator->reset();
  HyperVFileCopy *fcopyInstance = OSDynamicCast(HyperVFileCopy, fcopyIterator->getNextObject());
  fcopyIterator->release();
  
  if (fcopyInstance == NULL) {
    return;
  }
  fcopyInstance->HVDBGLOG("Got response from userspace: 0x%x", status);
  fcopyInstance->status = *status;
  fcopyInstance->wakeForUserspace();
}
