//
//  HyperVFileCopyUserClientPrivate.cpp
//  Hyper-V file copy user client
//
//  Copyright © 2022-2025 flagers, Goldfish64. All rights reserved.
//

#include "HyperVFileCopyUserClientInternal.hpp"
#include <sys/utfconv.h>

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

IOReturn HyperVFileCopyUserClient::notifyFileCopyClientApplication(HyperVFileCopyUserClientNotificationMessage *notificationMsg) {
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

IOReturn HyperVFileCopyUserClient::getFilePath(void *output, UInt32 *outputSize) {
  HyperVFileCopyUserClientStartCopyData *startCopyData;

  HVDBGLOG("Filename and path requested by userspace daemon");
  if (output == nullptr || *outputSize != sizeof (*startCopyData)) {
    HVDBGLOG("Invalid struct, null: %u size: %u bytes", output == nullptr, *outputSize);
    return kIOReturnBadArgument;
  }
  startCopyData = (HyperVFileCopyUserClientStartCopyData *) output;

  //
  // Copy filename and file path to userspace.
  //
  memcpy(startCopyData->fileName, _currentFileName, sizeof (startCopyData->fileName));
  memcpy(startCopyData->filePath, _currentFilePath, sizeof (startCopyData->filePath));
  HVDBGLOG("Returning filename %s and path %s to userspace daemon", startCopyData->fileName, startCopyData->filePath);
  return kIOReturnSuccess;
}

IOReturn HyperVFileCopyUserClient::getNextDataFragment(IOMemoryDescriptor *memoryDesc, void *output, UInt32 *outputSize) {
  IOReturn    status;
  IOByteCount bytesWritten;

  //
  // Structures larger than 4KB are passed in with an IOMemoryDescriptor that must be mapped instead.
  // This only applies to 10.5 and newer.
  //
  HVDBGLOG("File data requested by userspace daemon");
  if (memoryDesc != nullptr) {
    status = memoryDesc->prepare();
    if (status != kIOReturnSuccess) {
      HVDBGLOG("Failed to prepare data descriptor with status 0x%X", status);
      return status;
    }

    bytesWritten = memoryDesc->writeBytes(0, _currentFileData, *outputSize); // TODO: is this right output size?
    memoryDesc->complete();

    if (bytesWritten != kHyperVFileCopyFragmentSize) {
      HVDBGLOG("Failed to write all bytes to descriptor, only wrote %u bytes", bytesWritten);
      return kIOReturnIOError;
    }
  } else {
    if (output == nullptr || *outputSize != kHyperVFileCopyMaxDataSize) {
      HVDBGLOG("Invalid struct, null: %u, size %u bytes", output == nullptr, *outputSize);
      return kIOReturnBadArgument;
    }

    //
    // Copy file data to userspace.
    //
    memcpy(output, _currentFileData, *outputSize);
  }

  return kIOReturnSuccess;
}

IOReturn HyperVFileCopyUserClient::completeIO(IOReturn status) {
  HVDBGLOG("Completing I/O with status 0x%X", status);
  wakeThread(status);
  return kIOReturnSuccess;
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
IOReturn HyperVFileCopyUserClient::sDispatchMethodGetFilePath(HyperVFileCopyUserClient *target, void *ref, IOExternalMethodArguments *args) {
  return target->getFilePath(args->structureOutput, &args->structureOutputSize);
}

IOReturn HyperVFileCopyUserClient::sDispatchMethodGetNextDataFragment(HyperVFileCopyUserClient *target, void *ref, IOExternalMethodArguments *args) {
  return target->getNextDataFragment(args->structureOutputDescriptor, args->structureOutput, &args->structureOutputSize);
}

IOReturn HyperVFileCopyUserClient::sDispatchMethodCompleteIO(HyperVFileCopyUserClient *target, void *ref, IOExternalMethodArguments *args) {
  return target->completeIO(static_cast<IOReturn>(args->scalarInput[0]));
}
#else
#if (defined(__i386__) && defined(__clang__))
IOReturn HyperVFileCopyUserClient::sMethodGetFilePath(HyperVFileCopyUserClient *that, void *output, UInt32 *outputSize) {
  return that->getFilePath(output, outputSize);
}

IOReturn HyperVFileCopyUserClient::sMethodGetNextDataFragment(HyperVFileCopyUserClient *that, void *output, UInt32 *outputSize) {
  return that->getNextDataFragment(nullptr, output, outputSize);
}

IOReturn HyperVFileCopyUserClient::sMethodCompleteIO(HyperVFileCopyUserClient *that, UInt32 status) {
  return that->completeIO(static_cast<IOReturn>(status));
}
#else
IOReturn HyperVFileCopyUserClient::methodGetFilePath(void *output, UInt32 *outputSize) {
  return getFilePath(output, outputSize);
}

IOReturn HyperVFileCopyUserClient::methodGetNextDataFragment(void *output, UInt32 *outputSize) {
  return getNextDataFragment(nullptr, output, outputSize);
}

IOReturn HyperVFileCopyUserClient::methodCompleteIO(UInt32 status) {
  return completeIO(static_cast<IOReturn>(status));
}
#endif
#endif


