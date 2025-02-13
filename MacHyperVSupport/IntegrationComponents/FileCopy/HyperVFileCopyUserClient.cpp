//
//  HyperVFileCopyUserClient.cpp
//  Hyper-V file copy user client
//
//  Copyright © 2022-2025 flagers, Goldfish64. All rights reserved.
//

#include "HyperVFileCopyUserClientInternal.hpp"

OSDefineMetaClassAndStructors(HyperVFileCopyUserClient, super);

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
  static const IOExternalMethodDispatch methods[kHyperVFileCopyUserClientMethodNumberOfMethods] = {
    { // kHyperVFileCopyUserClientMethodGetFilePath
      reinterpret_cast<IOExternalMethodAction>(&HyperVFileCopyUserClient::sDispatchMethodGetFilePath),          // Method pointer
      0,                                              // Num of scalar input values
      0,                                              // Size of struct input
      0,                                              // Num of scalar output values
      sizeof (HyperVFileCopyUserClientStartCopyData)  // Size of struct output
    },
    { // kHyperVFileCopyUserClientMethodGetNextDataFragment
      reinterpret_cast<IOExternalMethodAction>(&HyperVFileCopyUserClient::sDispatchMethodGetNextDataFragment),  // Method pointer
      0,                                              // Num of scalar input values
      0,                                              // Size of struct input
      0,                                              // Num of scalar output values
      kHyperVFileCopyMaxDataSize                      // Size of struct output
    },
    { // kHyperVFileCopyUserClientMethodCompleteIO
      reinterpret_cast<IOExternalMethodAction>(&HyperVFileCopyUserClient::sDispatchMethodCompleteIO),           // Method pointer
      1,                                              // Num of scalar input values
      0,                                              // Size of struct input
      0,                                              // Num of scalar output values
      0                                               // Size of struct output
    }
  };

  if (selector >= kHyperVFileCopyUserClientMethodNumberOfMethods) {
    return kIOReturnUnsupported;
  }
  dispatch = const_cast<IOExternalMethodDispatch*>(&methods[selector]);

  target = this;
  reference = nullptr;

  return super::externalMethod(selector, arguments, dispatch, target, reference);
}
#else
IOExternalMethod* HyperVFileCopyUserClient::getTargetAndMethodForIndex(IOService **target, UInt32 index) {
  static const IOExternalMethod methods[kHyperVFileCopyUserClientMethodNumberOfMethods] = {
    { // kHyperVFileCopyUserClientMethodGetFilePath
      NULL,                                             // Target pointer
#if (defined(__i386__) && defined(__clang__))
      // Required to match GCC behavior on 32-bit when building with clang
      kIOExternalMethodACID32Padding,
      reinterpret_cast<IOMethodACID32>(&HyperVFileCopyUserClient::sMethodGetFilePath),          // Static method pointer
#else
      reinterpret_cast<IOMethod>(&HyperVFileCopyUserClient::methodGetFilePath),                 // Instance method pointer
#endif
      kIOUCScalarIStructO,                              // Method type
      0,                                                // Num of scalar input values or size of struct input
      sizeof (HyperVFileCopyUserClientStartCopyData)    // Num of scalar output values or size of struct output
    },
    { // kHyperVFileCopyUserClientMethodGetNextDataFragment
      NULL,                                             // Target pointer
#if (defined(__i386__) && defined(__clang__))
      // Required to match GCC behavior on 32-bit when building with clang
      kIOExternalMethodACID32Padding,
      reinterpret_cast<IOMethodACID32>(&HyperVFileCopyUserClient::sMethodGetNextDataFragment),  // Static method pointer
#else
      reinterpret_cast<IOMethod>(&HyperVFileCopyUserClient::methodGetNextDataFragment),         // Instance method pointer
#endif
      kIOUCScalarIStructO,                              // Method type
      0,                                                // Num of scalar input values or size of struct input
      kHyperVFileCopyMaxDataSize                        // Num of scalar output values or size of struct output
    },
    { // kHyperVFileCopyUserClientMethodCompleteIO
      NULL,                                             // Target pointer
#if (defined(__i386__) && defined(__clang__))
      // Required to match GCC behavior on 32-bit when building with clang
      kIOExternalMethodACID32Padding,
      reinterpret_cast<IOMethodACID32>(&HyperVFileCopyUserClient::sMethodCompleteIO),           // Static method pointer
#else
      reinterpret_cast<IOMethod>(&HyperVFileCopyUserClient::methodCompleteIO),                  // Instance method pointer
#endif
      kIOUCScalarIScalarO,                              // Method type
      1,                                                // Num of scalar input values or size of struct input
      0                                                 // Num of scalar output values or size of struct output
    }
  };

  if (index >= kHyperVFileCopyUserClientMethodNumberOfMethods) {
    return nullptr;
  }

  *target = this;
  return const_cast<IOExternalMethod*>(&methods[index]);
}
#endif

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
  notificationMsg.type                      = kHyperVFileCopyMessageTypeStartCopy;
  notificationMsg.startCopy.flags           = flags;
  notificationMsg.startCopy.fileSize        = fileSize;
  notificationMsg.startCopy.maxFragmentSize = kHyperVFileCopyMaxDataSize;

  _isSleeping = true;
  status = notifyFileCopyClientApplication(&notificationMsg);
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
  notificationMsg.dataFragment.size   = (kHyperVFileCopyMaxDataSize > dataSize) ? kHyperVFileCopyMaxDataSize : dataSize;
  _currentFileData                    = data;

  _isSleeping = true;
  status = notifyFileCopyClientApplication(&notificationMsg);
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

  //
  // To maintain compatibility with macOS 10.4, the legacy APIs are used for userspace communication.
  // This limits the maximum transfer size to 4KB.
  //
  if (dataSize > kHyperVFileCopyMaxDataSize) {
    //
    // Notify userspace daemon that the next data fragment is available.
    //
    notificationMsg.type                = kHyperVFileCopyMessageTypeWriteToFile;
    notificationMsg.dataFragment.offset = offset + kHyperVFileCopyMaxDataSize;
    notificationMsg.dataFragment.size   = dataSize - kHyperVFileCopyMaxDataSize;
    _currentFileData                   += kHyperVFileCopyMaxDataSize;

    _isSleeping = true;
    status = notifyFileCopyClientApplication(&notificationMsg);
    if (status != kIOReturnSuccess) {
      HVDBGLOG("Failed to send next data fragment (second portion) message to daemon with status 0x%X", status);
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
    HVDBGLOG("Userspace daemon acknowledged next data fragment (second portion)");
  }
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
  status = notifyFileCopyClientApplication(&notificationMsg);
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
  status = notifyFileCopyClientApplication(&notificationMsg);
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
