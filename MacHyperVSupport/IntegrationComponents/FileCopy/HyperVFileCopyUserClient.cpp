//
//  HyperVFileCopyUserClient.cpp
//  Hyper-V file copy user client
//
//  Copyright © 2022 flagers, Goldfish64. All rights reserved.
//

#include "HyperVFileCopyUserClientInternal.hpp"

#include <sys/syslimits.h>
#include <sys/utfconv.h>

OSDefineMetaClassAndStructors(HyperVFileCopyUserClient, super);

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
const IOExternalMethodDispatch HyperVFileCopyUserClient::sFileCopyMethods[kHyperVFileCopyUserClientMethodNumberOfMethods] = {
  { // kHyperVFileCopyUserClientMethodGetFilePath
    reinterpret_cast<IOExternalMethodAction>(&HyperVFileCopyUserClient::methodGetFilePath),         // Method pointer
    0,                                                    // Num of scalar input values
    0,                                                    // Size of struct input
    0,                                                    // Num of scalar output values
    sizeof (HyperVFileCopyUserClientStartCopyData)        // Size of struct output
  },
  { // kHyperVFileCopyUserClientMethodGetNextDataFragment
    reinterpret_cast<IOExternalMethodAction>(&HyperVFileCopyUserClient::methodGetNextDataFragment), // Method pointer
    0,                                                    // Num of scalar input values
    0,                                                    // Size of struct input
    0,                                                    // Num of scalar output values
    kHyperVFileCopyFragmentSize                           // Size of struct output
  },
  { // kHyperVFileCopyUserClientMethodCompleteIO
    reinterpret_cast<IOExternalMethodAction>(&HyperVFileCopyUserClient::methodCompleteIO),          // Method pointer
    1,                                                    // Num of scalar input values
    0,                                                    // Size of struct input
    0,                                                    // Num of scalar output values
    0                                                     // Size of struct output
  }
};
#else
#endif

bool HyperVFileCopyUserClient::start(IOService *provider) {
  if (!super::start(provider)) {
    HVSYSLOG("super::start() returned false");
    return false;
  }

  HVCheckDebugArgs();
  setICDebug(debugEnabled);

  HVDBGLOG("Initialized Hyper-V File Copy user client");
  return true;
}

void HyperVFileCopyUserClient::stop(IOService *provider) {
  HVDBGLOG("Stopping Hyper-V File Copy user client");
  super::stop(provider);
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
IOReturn HyperVFileCopyUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *arguments, IOExternalMethodDispatch *dispatch,
                                                  OSObject *target, void *reference) {
  if (selector >= kHyperVFileCopyUserClientMethodNumberOfMethods) {
    return kIOReturnUnsupported;
  }
  dispatch = const_cast<IOExternalMethodDispatch*>(&sFileCopyMethods[selector]);

  target = this;
  reference = nullptr;

  return super::externalMethod(selector, arguments, dispatch, target, reference);
}
#else
#endif

bool HyperVFileCopyUserClient::convertUnicodePath(UInt16 *inputStr, UInt8 *outputStr, size_t outputStrSize) {
  int result;
  UInt8 unicodeNul[3] = { 0xE2, 0x90, 0x80 };
  void *unicodeNulLoc;
  size_t nlen;

  bzero(outputStr, outputStrSize);
  result = utf8_encodestr(inputStr, kHyperVFileCopyMaxPath * sizeof (UInt16), outputStr, &nlen, outputStrSize, '/', UTF_LITTLE_ENDIAN);
  if (result != 0) {
    HVDBGLOG("Failed to encode UTF8 string with result %d", result);
    return false;
  }
  
  unicodeNulLoc = lilu_os_memmem(outputStr, outputStrSize, &unicodeNul, sizeof (unicodeNul));
  if (!unicodeNulLoc) {
    return false;
  }
  *(char *)unicodeNulLoc = 0x00;

  return true;
}

IOReturn HyperVFileCopyUserClient::notifyClientApplication(HyperVFileCopyUserClientNotificationMessage *notificationMsg) {
  if (_notificationPort == MACH_PORT_NULL) {
    HVDBGLOG("Notification port is not open");
    return kIOReturnNotFound;
  }

  HVDBGLOG("Sending file copy notification type %u", notificationMsg->type);

  notificationMsg->header.msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
  notificationMsg->header.msgh_size        = sizeof (*notificationMsg);
  notificationMsg->header.msgh_remote_port = _notificationPort;
  notificationMsg->header.msgh_local_port  = MACH_PORT_NULL;
  notificationMsg->header.msgh_reserved    = 0;
  notificationMsg->header.msgh_id          = 0;

  return mach_msg_send_from_kernel(&notificationMsg->header, notificationMsg->header.msgh_size);
}

IOReturn HyperVFileCopyUserClient::methodGetFilePath(HyperVFileCopyUserClient *target, void *ref, IOExternalMethodArguments *args) {
  HyperVFileCopyUserClientStartCopyData *startCopyData;

  target->HVDBGLOG("File name and path requested by userspace daemon");
  if (args->structureOutput == nullptr || args->structureOutputSize != sizeof (*startCopyData)) {
    target->HVDBGLOG("Invalid struct %u size %u bytes", args->structureOutput == nullptr, args->structureOutputSize);
    return kIOReturnBadArgument;
  }
  startCopyData = (HyperVFileCopyUserClientStartCopyData *) args->structureOutput;

  //
  // Copy filename and file path to userspace.
  //
  memcpy(startCopyData->fileName, target->_currentFileName, sizeof (startCopyData->fileName));
  memcpy(startCopyData->filePath, target->_currentFilePath, sizeof (startCopyData->filePath));
  return kIOReturnSuccess;
}

IOReturn HyperVFileCopyUserClient::methodGetNextDataFragment(HyperVFileCopyUserClient *target, void *ref, IOExternalMethodArguments *args) {
  IOReturn    status;
  IOByteCount bytesWritten;

  //
  // Structures larger than 4KB are passed in with an IOMemoryDescriptor that must be mapped instead.
  //
  target->HVDBGLOG("File data requested by userspace daemon");
  if (args->structureOutputDescriptor != nullptr) {
    status = args->structureOutputDescriptor->prepare();
    if (status != kIOReturnSuccess) {
      target->HVDBGLOG("Failed to prepare data descriptor with status 0x%X", status);
      return status;
    }

    bytesWritten = args->structureOutputDescriptor->writeBytes(0, target->_currentFileData, kHyperVFileCopyFragmentSize);
    args->structureOutputDescriptor->complete();

    if (bytesWritten != kHyperVFileCopyFragmentSize) {
      target->HVDBGLOG("Failed to write all bytes to descriptor, only wrote %u bytes", bytesWritten);
      return kIOReturnIOError;
    }
  } else {
    if (args->structureOutput == nullptr || args->structureOutputSize != kHyperVFileCopyFragmentSize) {
      target->HVDBGLOG("Invalid struct %u size %u bytes", args->structureOutput == nullptr, args->structureOutputSize);
      return kIOReturnBadArgument;
    }

    //
    // Copy file data to userspace.
    //
    memcpy(args->structureOutput, target->_currentFileData, kHyperVFileCopyFragmentSize);
  }

  return kIOReturnSuccess;
}

IOReturn HyperVFileCopyUserClient::methodCompleteIO(HyperVFileCopyUserClient *target, void *ref, IOExternalMethodArguments *args) {
  target->HVDBGLOG("Completing I/O with status 0x%X", static_cast<IOReturn>(args->scalarInput[0]));
  target->wakeThread(static_cast<IOReturn>(args->scalarInput[0]));
  return kIOReturnSuccess;
}

IOReturn HyperVFileCopyUserClient::startFileCopy(UInt16 *fileName, UInt16 *filePath, HyperVFileCopyMessageFlags flags, UInt64 fileSize) {
  IOReturn                                    status;
  HyperVFileCopyUserClientNotificationMessage notificationMsg = { };

  //
  // Convert and store filenames for later use by userspace daemon.
  //
  if (!convertUnicodePath(fileName, _currentFileName, sizeof (_currentFileName))) {
    return kIOReturnBadArgument;
  }
  if (!convertUnicodePath(filePath, _currentFilePath, sizeof (_currentFilePath))) {
    return kIOReturnBadArgument;
  }
  HVDBGLOG("Source file name: %s", _currentFileName);
  HVDBGLOG("Destination file path: %s", _currentFilePath);

  //
  // Notify userspace daemon that a file copy operation is starting.
  //
  notificationMsg.type               = kHyperVFileCopyMessageTypeStartCopy;
  notificationMsg.startCopy.flags    = flags;
  notificationMsg.startCopy.fileSize = fileSize;

  _isSleeping = true;
  status = notifyClientApplication(&notificationMsg);
  if (status != kIOReturnSuccess) {
    HVDBGLOG("Failed to send start copy message to daemon with status 0x%X", status);
    return status;
  }

  //
  // Wait for userspace daemon to complete.
  //
  // Userspace daemon needs to call the external method GetFilePath() followed by
  // CompleteIO() to continue.
  //
  HVDBGLOG("Waiting for userspace daemon to complete");
  status = sleepThread();
  if (status != kIOReturnSuccess) {
    HVDBGLOG("Received failure response or timed out with status 0x%X", status);
    return status;
  }
  HVDBGLOG("Userspace daemon acknowledged start of file copy");
  return kIOReturnSuccess;
}

IOReturn HyperVFileCopyUserClient::writeFileFragment(UInt64 offset, UInt8 *data, UInt32 dataSize) {
  IOReturn                                    status;
  HyperVFileCopyUserClientNotificationMessage notificationMsg = { };

  HVDBGLOG("Received %u bytes to write to file offset 0x%llX", dataSize, offset);

  //
  // Notify userspace daemon that the next data fragment is available.
  //
  notificationMsg.type                = kHyperVFileCopyMessageTypeWriteToFile;
  notificationMsg.dataFragment.offset = offset;
  notificationMsg.dataFragment.size   = dataSize;
  _currentFileData                    = data;

  _isSleeping = true;
  status = notifyClientApplication(&notificationMsg);
  if (status != kIOReturnSuccess) {
    HVDBGLOG("Failed to send next data fragment message to daemon with status 0x%X", status);
    return status;
  }

  //
  // Wait for userspace daemon to complete.
  //
  // Userspace daemon needs to call the external method GetNextDataFragment() followed by
  // CompleteIO() to continue.
  //
  HVDBGLOG("Waiting for userspace daemon to complete");
  status = sleepThread();
  if (status != kIOReturnSuccess) {
    HVDBGLOG("Received failure response or timed out with status 0x%X", status);
    return status;
  }
  HVDBGLOG("Userspace daemon acknowledged next data fragment");
  return kIOReturnSuccess;
}

IOReturn HyperVFileCopyUserClient::completeFileCopy() {
  IOReturn                                    status;
  HyperVFileCopyUserClientNotificationMessage notificationMsg = { };

  HVDBGLOG("Completing file copy");

  //
  // Notify userspace daemon that the file copy is complete.
  //
  notificationMsg.type = kHyperVFileCopyMessageTypeCompleteCopy;
  _isSleeping = true;
  status = notifyClientApplication(&notificationMsg);
  if (status != kIOReturnSuccess) {
    HVDBGLOG("Failed to send copy complete to daemon with status 0x%X", status);
    return status;
  }

  //
  // Wait for userspace daemon to complete.
  //
  // Userspace daemon needs to call the external method CompleteIO() to continue.
  //
  HVDBGLOG("Waiting for userspace daemon to complete");
  status = sleepThread();
  if (status != kIOReturnSuccess) {
    HVDBGLOG("Received failure response or timed out with status 0x%X", status);
    return status;
  }
  HVDBGLOG("Userspace daemon acknowledged file copy completion");
  return kIOReturnSuccess;
}

IOReturn HyperVFileCopyUserClient::cancelFileCopy() {
  IOReturn                                    status;
  HyperVFileCopyUserClientNotificationMessage notificationMsg = { };

  HVDBGLOG("Cancelling file copy");

  //
  // Notify userspace daemon that the file copy is complete.
  //
  notificationMsg.type = kHyperVFileCopyMessageTypeCancelCopy;
  _isSleeping = true;
  status = notifyClientApplication(&notificationMsg);
  if (status != kIOReturnSuccess) {
    HVDBGLOG("Failed to send copy cancellation message to daemon with status 0x%X", status);
    return status;
  }

  //
  // Wait for userspace daemon to complete.
  //
  // Userspace daemon needs to call the external method CompleteIO() to continue.
  //
  HVDBGLOG("Waiting for userspace daemon to complete");
  status = sleepThread();
  if (status != kIOReturnSuccess) {
    HVDBGLOG("Received failure response or timed out with status 0x%X", status);
    return status;
  }
  HVDBGLOG("Userspace daemon acknowledged file copy cancellation");
  return kIOReturnSuccess;
}
