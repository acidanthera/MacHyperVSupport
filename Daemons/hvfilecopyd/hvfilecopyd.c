//
//  hvfilecopyd.c
//  Hyper-V guest shutdown userspace daemon
//
//  Copyright © 2022 flagers, Goldfish64. All rights reserved.
//

#include <sys/stat.h>

#include "HyperVFileCopyUserClient.h"
#include "hvdebug.h"
#include "hviokit.h"

#define HYPERV_FILE_COPY_KERNEL_SERVICE "HyperVFileCopy"

HVDeclareLogFunctionsUser("hvfilecopyd");

static char _currentFilePath[PATH_MAX];
static int _currentFileDesc;
static UInt8 _fragmentData[kHyperVFileCopyFragmentSize];
static UInt32 _maxFragmentDataSize;

static IOReturn hvFileCopyStartCopy(io_connect_t connection, HyperVFileCopyMessageFlags flags, UInt64 fileSize, UInt32 maxFragmentSize) {
  IOReturn                              status;
  HyperVFileCopyUserClientStartCopyData startCopyData     = { };
  size_t                                startCopyDataSize = sizeof (startCopyData);

  struct stat pathStat;
  char *pathPtr;
  char *charPtr;

  //
  // Get file name and file path from userclient.
  //
  HVDBGLOG(stdout, "Starting file copy of %llu bytes (flags 0x%X, max fragment size %u bytes)", fileSize, flags, maxFragmentSize);
  _maxFragmentDataSize = maxFragmentSize;
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
  //
  // Call into user client with standard API.
  //
  status = IOConnectCallStructMethod(connection, kHyperVFileCopyUserClientMethodGetFilePath, NULL, 0, &startCopyData, &startCopyDataSize);
#else
  //
  // Call into user client with legacy API.
  //
  status = IOConnectMethodScalarIStructureO(connection, kHyperVFileCopyUserClientMethodGetFilePath, 0, &startCopyDataSize, &startCopyData);
#endif
  if (status != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failed to call GetFilePath() with status 0x%X", status);
    return status;
  }
  HVDBGLOG(stdout, "Source file name: %s", startCopyData.fileName);
  HVDBGLOG(stdout, "Destination file path: %s", startCopyData.filePath);

  //
  // If the destination path is an existing directory, append the source filename to it.
  // If not, use it as the full file path.
  //
  // Additionally, force all paths to be relative to root of disk.
  //
  snprintf(_currentFilePath, sizeof (_currentFilePath), "/%s", (char *) startCopyData.filePath);
  if (!access(_currentFilePath, F_OK) && !stat(_currentFilePath, &pathStat) && S_ISDIR(pathStat.st_mode)) {
    HVDBGLOG(stdout, "Directory %s exists, will append source filename", _currentFilePath);
    snprintf(_currentFilePath, sizeof (_currentFilePath), "/%s/%s", (char *) startCopyData.filePath, (char *) startCopyData.fileName);
  } else {
    snprintf(_currentFilePath, sizeof (_currentFilePath), "/%s", (char *) startCopyData.filePath);
  }
  HVDBGLOG(stdout, "Full destination file path: %s", _currentFilePath);

  //
  // Check if file exists already.
  //
  if (!access(_currentFilePath, F_OK) && !(flags & kHyperVFileCopyMessageFlagsOverwrite)) {
    HVSYSLOG(stderr, "File %s already exists", _currentFilePath);
    return kIOReturnStillOpen;
  }

  //
  // Walk directory tree and ensure all directories exist.
  // If one does not and Hyper-V has instructed the guest to create them, do that.
  //
  pathPtr = _currentFilePath;
  while ((charPtr = strchr(pathPtr, '/')) != NULL) {
    if (charPtr == pathPtr) {
      pathPtr++;
      continue;
    }

    *charPtr = '\0';
    HVDBGLOG(stdout, "Got %s", _currentFilePath);
    if (access(_currentFilePath, F_OK)) {
      if (flags & kHyperVFileCopyMessageFlagsCreatePath) {
        HVDBGLOG(stdout, "Directory %s does not exist, creating it", _currentFilePath);
        if (mkdir(_currentFilePath, 0755)) {
          HVSYSLOG(stderr, "Failed to create directory %s", _currentFilePath);
          return kIOReturnIOError;
        }
      } else {
        HVSYSLOG(stderr, "Directory %s does not exist", _currentFilePath);
        return kIOReturnNotFound;
      }
    }

    pathPtr = charPtr + 1;
    *charPtr = '/';
  }

  //
  // Create/open file for writing.
  //
  _currentFileDesc = open(_currentFilePath, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0744);
  if (_currentFileDesc == -1) {
    HVSYSLOG(stderr, "Failed to create file %s with error %s", _currentFilePath, strerror(errno));
    return kIOReturnIOError;
  }
  return kIOReturnSuccess;
}

static IOReturn hvFileCopyWriteDataFragment(io_connect_t connection, UInt64 offset, UInt32 size) {
  IOReturn status;
  size_t   fragmentDataSize = _maxFragmentDataSize;
  size_t   fileBytesWritten;

  //
  // Get file data.
  //
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
  //
  // Call into user client with standard API.
  //
  status = IOConnectCallStructMethod(connection, kHyperVFileCopyUserClientMethodGetNextDataFragment, NULL, 0, _fragmentData, &fragmentDataSize);
#else
  //
  // Call into user client with legacy API.
  //
  status = IOConnectMethodScalarIStructureO(connection, kHyperVFileCopyUserClientMethodGetNextDataFragment, 0, &fragmentDataSize, _fragmentData);
#endif
  if (status != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failed to call GetNextDataFragment() with status 0x%X", status);
    return status;
  }

  //
  // Verify size is within bounds, but should not occur.
  //
  if (size > sizeof (_fragmentData)) {
    HVSYSLOG(stderr, "Received data size is too large for buffer");
    return kIOReturnBadArgument;
  }

  status = kIOReturnSuccess;
  HVDBGLOG(stdout, "Writing %u bytes to offset 0x%llX in file %s", size, offset, _currentFilePath);
  fileBytesWritten = pwrite(_currentFileDesc, _fragmentData, size, offset);
  if (fileBytesWritten != size) {
    switch (errno) {
      case ENOSPC:
        status = kIOReturnNoSpace;
        break;

      default:
        status = kIOReturnIOError;
        break;
    }
    HVSYSLOG(stderr, "Only wrote %llu bytes to file, failed with status %s", fileBytesWritten, strerror(errno));
  }

  return status;
}

static IOReturn hvFileCopyComplete() {
  //
  // Close file.
  //
  close(_currentFileDesc);
  _currentFileDesc = -1;

  HVDBGLOG(stdout, "Completed file copy to %s", _currentFilePath);
  return kIOReturnSuccess;
}

static IOReturn hvFileCopyCancel() {
  //
  // Close and remove file.
  //
  close(_currentFileDesc);
  _currentFileDesc = -1;
  unlink(_currentFilePath);

  HVDBGLOG(stdout, "Cancelled file copy to %s", _currentFilePath);
  return kIOReturnSuccess;
}

static void hvFileCopyCompleteIO(io_connect_t connection, IOReturn status) {
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
  //
  // Call into user client with standard API.
  //
  UInt64 status64 = (UInt64) status;
  IOReturn completeStatus = IOConnectCallScalarMethod(connection, kHyperVFileCopyUserClientMethodCompleteIO, &status64, 1, NULL, NULL);
#else
  //
  // Call into user client with legacy API.
  //
  IOReturn completeStatus = IOConnectMethodScalarIScalarO(connection, kHyperVFileCopyUserClientMethodCompleteIO, 1, 0, status);
#endif
  if (completeStatus != kIOReturnSuccess) {
    HVSYSLOG(stdout, "Failed to call CompleteIO() with status 0x%X", completeStatus);
  }
}

void hvIOKitNotificationHandler(io_connect_t connection, CFMachPortRef port, void *msg, CFIndex size, void *info) {
  HyperVFileCopyUserClientNotificationMessage *fileCopyMsg = (HyperVFileCopyUserClientNotificationMessage *) msg;
  IOReturn                                    status;

  if (size < __offsetof(HyperVFileCopyUserClientNotificationMessage, type)) {
    HVSYSLOG(stderr, "Invalid message size %u received, should be at least %u",
             size, __offsetof(HyperVFileCopyUserClientNotificationMessage, type));
    return;
  }

  HVDBGLOG(stdout, "Received notification of type 0x%X", fileCopyMsg->type);
  switch (fileCopyMsg->type) {
    case kHyperVFileCopyMessageTypeStartCopy:
      status = hvFileCopyStartCopy(connection, fileCopyMsg->startCopy.flags,
                                   fileCopyMsg->startCopy.fileSize, fileCopyMsg->startCopy.maxFragmentSize);
      break;

    case kHyperVFileCopyMessageTypeWriteToFile:
      status = hvFileCopyWriteDataFragment(connection, fileCopyMsg->dataFragment.offset, fileCopyMsg->dataFragment.size);
      break;

    case kHyperVFileCopyMessageTypeCompleteCopy:
      status = hvFileCopyComplete();
      break;

    case kHyperVFileCopyMessageTypeCancelCopy:
      status = hvFileCopyCancel();
      break;

    default:
      HVDBGLOG(stdout, "Unknown notification type 0x%X", fileCopyMsg->type);
      return;
  }

  //
  // Userclient sleeps calling thread until this function is called with return status, or a timeout occurs.
  //
  HVDBGLOG(stdout, "Operation returned status 0x%X", status);
  hvFileCopyCompleteIO(connection, status);
}

int main(int argc, const char * argv[]) {
  //
  // Setup I/O Kit notifications.
  //
  if (hvIOKitSetupIOKitNotifications(HYPERV_FILE_COPY_KERNEL_SERVICE) != kIOReturnSuccess) {
    return -1;
  }

  //
  // Run main loop, this should not return.
  //
  CFRunLoopRun();
  return 0;
}
