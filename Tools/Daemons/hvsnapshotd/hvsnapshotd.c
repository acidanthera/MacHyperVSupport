//
//  hvsnapshotd.c
//  Hyper-V VSS snapshot userspace daemon
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "HyperVSnapshotUserClient.h"
#include "hvdebug.h"
#include "hviokit.h"

#include <sys/mount.h>
#include <sys/utsname.h>

#define HYPERV_SNAPSHOT_KERNEL_SERVICE  "HyperVSnapshot"

#define DARWIN_VERSION_HIGHSIERRA       17
#define DISKUTIL_INFO                   "/usr/sbin/diskutil info -plist "
#define DISKUTIL_APFS_LIST              "/usr/sbin/diskutil apfs list -plist"

static const char *devPath = "/dev/";

typedef struct {
  char  apfsDevPath[128 + 1];
  char  apfsContainer[128 + 1];
  bool  containerFrozen;
} apfs_fs_container_mapping_t;

static int apfsContainerMappingCount                      = 0;
static apfs_fs_container_mapping_t *apfsContainerMappings = NULL;

//
// Filesystems that do not support freeze/thaw.
//
static const char *excludeFileSystems[] = {
  "exfat",  // exFAT
  "msdos",  // FAT
  NULL
};

HVDeclareLogFunctionsUser("hvsnapshotd");

static bool isApfsSupported() {
  static short int version[3] = {0};
  struct utsname uts;

  uname(&uts);
  sscanf(uts.release, "%hd.%hd.%hd", &version[0], &version[1], &version[2]);

  return version[0] >= DARWIN_VERSION_HIGHSIERRA;
}

static IOReturn getPlistDiskInfo(const char *command, CFDataRef *outPlistData, CFDictionaryRef *outPlistDict) {
  char              lineBuffer[256];
  size_t            plistBufferAllocatedSize  = 0;
  size_t            plistBufferCurrentLength  = 0;
  char              *plistBuffer              = NULL;
  char              *newPlistBuffer           = NULL;
  CFDataRef         plistData                 = NULL;
  CFDictionaryRef   plistDict                 = NULL;

  //
  // Invoke diskutil to get APFS info.
  //
  FILE *stream = popen(command, "r");
  if (stream == NULL) {
    HVSYSLOG(stderr, "Failed to call '%'", command);
    return kIOReturnIOError;
  }

  plistBufferAllocatedSize = (PAGE_SIZE * 4);
  plistBufferCurrentLength = 0;
  plistBuffer = malloc(plistBufferAllocatedSize);
  if (plistBuffer == NULL) {
    HVSYSLOG(stderr, "Failed to allocate plist buffer");
    return kIOReturnNoResources;
  }

  //
  // Read all lines and append to plist buffer.
  // Resize plist buffer if needed.
  //
  while (!feof(stream)) {
    if (fgets(lineBuffer, sizeof (lineBuffer), stream) != NULL) {
      size_t lineLength = strlen(lineBuffer);
      if ((plistBufferCurrentLength + lineLength + 1) > plistBufferAllocatedSize) {
        HVDBGLOG(stdout, "Plist buffer too small, reallocating");
        plistBufferAllocatedSize += PAGE_SIZE;
        newPlistBuffer = realloc(plistBuffer, plistBufferAllocatedSize);
        if (newPlistBuffer == NULL) {
          free(plistBuffer);
          HVSYSLOG(stderr, "Failed to reallocate plist buffer");
          return kIOReturnNoResources;
        }
        plistBuffer = newPlistBuffer;
      }

      strncat(plistBuffer, lineBuffer, plistBufferAllocatedSize);
      plistBufferCurrentLength += lineLength;
    }
  }
  pclose(stream);

  //
  // Create CFData object.
  //
  plistData = CFDataCreate(kCFAllocatorDefault, (uint8_t*) plistBuffer, plistBufferAllocatedSize);
  free(plistBuffer);
  if (plistData == NULL) {
    HVSYSLOG(stderr, "Failed to create CFDataRef plist object");
    return kIOReturnNoResources;
  }

  if (__builtin_available(macOS 10.6, *)) {
    plistDict = CFPropertyListCreateWithData(kCFAllocatorDefault, plistData, 0, NULL, NULL);
  } else {
    plistDict = NULL;
  }
  if (plistDict == NULL) {
    HVSYSLOG(stderr, "Failed to create CFDataRef plist object");
    CFRelease(plistData);
    return kIOReturnNoResources;
  }

  *outPlistData = plistData;
  *outPlistDict = plistDict;
  return kIOReturnSuccess;
}

static IOReturn getApfsList(CFDataRef *outPlistData, CFDictionaryRef *outPlistDict) {
  return getPlistDiskInfo(DISKUTIL_APFS_LIST, outPlistData, outPlistDict);
}

static IOReturn getDiskInfo(const char *disk, CFDataRef *outPlistData, CFDictionaryRef *outPlistDict) {
  char diskUtilCommand[128] = { DISKUTIL_INFO };
  strncat(diskUtilCommand, disk, sizeof (diskUtilCommand) - 1);

  return getPlistDiskInfo(diskUtilCommand, outPlistData, outPlistDict);
}

static CFStringRef getApfsContainer(const char *volume) {
  CFDataRef       plistData         = NULL;
  CFDictionaryRef plistDict         = NULL;
  CFDataRef       diskPlistData     = NULL;
  CFDictionaryRef diskPlistDict     = NULL;
  CFStringRef     diskDeviceIdStr   = NULL;

  CFArrayRef      containersArray   = NULL;
  CFDictionaryRef containerDict     = NULL;
  CFArrayRef      volumesArray      = NULL;
  CFDictionaryRef volumeDict        = NULL;
  CFStringRef     deviceIdStr       = NULL;
  CFStringRef     containerRefStr   = NULL;
  CFStringRef     resultStr         = NULL;

  IOReturn        status;

  //
  // Get device info for volume path.
  //
  status = getDiskInfo(volume, &diskPlistData, &diskPlistDict);
  if (status != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failed to get disk information for '%s'", volume);
    return NULL;
  }

  //
  // Check if container reference is directly present on disk info.
  //
  containerRefStr = CFDictionaryGetValue(diskPlistDict, CFSTR("APFSContainerReference"));
  if (containerRefStr != NULL) {
    resultStr = CFStringCreateCopy(kCFAllocatorDefault, containerRefStr);
    HVDBGLOG(stdout, "Got APFS container for device '%s' using direct property", volume);

    CFRelease(diskPlistDict);
    CFRelease(diskPlistData);
    return resultStr;
  }

  //
  // Fallback to pulling/comparing from all disks.
  //
  diskDeviceIdStr = CFDictionaryGetValue(diskPlistDict, CFSTR("DeviceIdentifier"));
  if (diskDeviceIdStr == NULL) {
    HVSYSLOG(stderr, "Failed to get disk identifier for '%s'", volume);
    CFRelease(diskPlistDict);
    CFRelease(diskPlistData);
    return NULL;
  }

  status = getApfsList(&plistData, &plistDict);
  if (status != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failed to get APFS disk info");
    CFRelease(diskPlistDict);
    CFRelease(diskPlistData);
    return NULL;
  }

  containersArray = CFDictionaryGetValue(plistDict, CFSTR("Containers"));
  if (containersArray == NULL) {
    HVSYSLOG(stderr, "Failed to get containers array");
    CFRelease(diskPlistDict);
    CFRelease(diskPlistData);
    CFRelease(plistDict);
    CFRelease(plistData);
    return NULL;
  }

  for (int i = 0; i < CFArrayGetCount(containersArray); i++) {
    containerDict = CFArrayGetValueAtIndex(containersArray, i);
    if (containerDict == NULL) {
      HVSYSLOG(stderr, "Failed to get container dict");
      continue;
    }

    //
    // Check if container has volume.
    //
    volumesArray = CFDictionaryGetValue(containerDict, CFSTR("Volumes"));
    if (volumesArray == NULL) {
      HVSYSLOG(stderr, "Failed to get volumes array");
      continue;
    }

    for (int v = 0; v < CFArrayGetCount(volumesArray); v++) {
      volumeDict = CFArrayGetValueAtIndex(volumesArray, v);
      if (volumeDict == NULL) {
        HVSYSLOG(stderr, "Failed to get volume dict");
        continue;
      }

      deviceIdStr = CFDictionaryGetValue(volumeDict, CFSTR("DeviceIdentifier"));
      if (deviceIdStr == NULL) {
        HVSYSLOG(stderr, "Failed to get device identifier");
        continue;
      }
      if (CFStringCompare(diskDeviceIdStr, deviceIdStr, 0) == kCFCompareEqualTo) {
        //
        // Get container reference.
        //
        containerRefStr = CFDictionaryGetValue(containerDict, CFSTR("ContainerReference"));
        if (containerRefStr != NULL) {
          resultStr = CFStringCreateCopy(kCFAllocatorDefault, containerRefStr);
          HVDBGLOG(stdout, "Got APFS container for device '%s'", volume);

          CFRelease(diskPlistDict);
          CFRelease(diskPlistData);
          CFRelease(plistDict);
          CFRelease(plistData);
          return resultStr;
        }
      }
    }
  }

  CFRelease(diskPlistDict);
  CFRelease(diskPlistData);
  CFRelease(plistDict);
  CFRelease(plistData);
  return NULL;
}

static bool checkFilesystemIsSupported(const char *fsName) {
  for (int i = 0; excludeFileSystems[i] != NULL; i++) {
    if (strncmp(fsName, excludeFileSystems[i], strlen(excludeFileSystems[i])) == 0) {
      return false;
    }
  }
  return true;
}

static IOReturn freezeThawFilesystem(const char *mountDir, int command) {
  //
  // Open filesystem as read only.
  //
  int fd = open(mountDir, O_RDONLY);
  if (fd < 0) {
    return kIOReturnIOError;
  }

  //
  // Freeze/thaw filesystem
  //
  int ret = fcntl(fd, command, 0);
  close(fd);

  return (ret == 0) ? kIOReturnSuccess : kIOReturnIOError;
}

static IOReturn freezeThawMountedFilesystems(UInt32 type) {
  struct statfs*    mountList         = NULL;
  int               mountCount        = 0;
  CFStringRef       apfsContainerStr  = NULL;

  IOReturn status;

  //
  // Get mounted filesystems.
  //
  mountCount = getmntinfo(&mountList, MNT_WAIT);
  if (mountCount < 0) {
    HVSYSLOG(stderr, "Failed to get mounted filesystems");
    return kIOReturnIOError;
  }

  //
  // Create APFS container to volume mapping on High Sierra and newer.
  //
  if ((type == kHyperVSnapshotUserClientNotificationTypeCheck) && isApfsSupported()) {
    if (apfsContainerMappings != NULL) {
      free(apfsContainerMappings);
      apfsContainerMappings = NULL;
    }

    apfsContainerMappingCount = mountCount;
    apfsContainerMappings = malloc(sizeof (*apfsContainerMappings) * apfsContainerMappingCount);
    if (apfsContainerMappings == NULL) {
      HVSYSLOG(stderr, "Failed to create APFS container mappings");
      return kIOReturnNoResources;
    }
  } else if (isApfsSupported()) {
    if ((apfsContainerMappings == NULL) || (apfsContainerMappingCount != mountCount)) {
      HVSYSLOG(stdout, "APFS container mapping array not created or mount count mismatch (%d %d)",
               apfsContainerMappingCount, mountCount);
      return kIOReturnUnsupported;
    }
  }

  for (int i = 0; i < mountCount; i++) {
    bool isReadOnly = (mountList[i].f_flags & MNT_RDONLY) == MNT_RDONLY;
    bool isApfs = strncmp(mountList[i].f_fstypename, "apfs", strlen(mountList[i].f_fstypename)) == 0;
      HVDBGLOG(stdout, "Filesystem %d type '%s' rw %u mounted at '%s' from '%s'", i,
               mountList[i].f_fstypename, !isReadOnly, mountList[i].f_mntonname, mountList[i].f_mntfromname);

    //
    // Only handle disk-based filesystems that are mounted read/write.
    //
    if (strncmp(mountList[i].f_mntfromname, devPath, strlen(devPath)) != 0) {
      continue;
    } else if (isReadOnly) {
      continue;
    } else if (!checkFilesystemIsSupported(mountList[i].f_fstypename)) {
      continue;
    }
    
    if (isApfs && !isApfsSupported()) {
      // TODO: Fail if APFS is found on Sierra.
      continue;
    }

    //
    // Populate APFS mappings if we are in the check phase.
    //
    if ((type == kHyperVSnapshotUserClientNotificationTypeCheck) && isApfs) {
      strncpy(apfsContainerMappings[i].apfsDevPath, mountList[i].f_mntfromname, sizeof (apfsContainerMappings[i].apfsDevPath) - 1);
      apfsContainerMappings[i].apfsDevPath[sizeof (apfsContainerMappings[i].apfsDevPath) - 1] = '\0';

      apfsContainerStr = getApfsContainer(mountList[i].f_mntfromname);
      if (apfsContainerStr == NULL) {
        HVSYSLOG(stderr, "Failed to get container for APFS volume '%s'", mountList[i].f_mntfromname);
        free(apfsContainerMappings);
        apfsContainerMappings = NULL;
        return kIOReturnUnsupported;
      }

      CFStringGetCString(apfsContainerStr, apfsContainerMappings[i].apfsContainer,
                         sizeof (apfsContainerMappings[i].apfsContainer), kCFStringEncodingUTF8);
      CFRelease(apfsContainerStr);

      HVDBGLOG(stdout, "Got APFS container '%s' for volume '%s'",
               apfsContainerMappings[i].apfsContainer, apfsContainerMappings[i].apfsDevPath);
    }

    //
    // Freeze filesystem.
    //
    if (type == kHyperVSnapshotUserClientNotificationTypeFreeze) {
      if (isApfs) {
        if (apfsContainerMappings[i].containerFrozen) {
          HVDBGLOG(stdout, "Skipping due to APFS container already frozen");
          continue;
        }

        //
        // Mark APFS container as frozen for all children.
        //
        for (int f = 0; f < mountCount; f++) {
          if (strlen(apfsContainerMappings[f].apfsContainer) == 0) {
            continue;
          }
          if (strncmp(apfsContainerMappings[i].apfsContainer, apfsContainerMappings[f].apfsContainer, strlen(apfsContainerMappings[f].apfsContainer)) == 0) {
            HVDBGLOG(stdout, "Marking APFS container '%s' as frozen for volume '%s'",
                     apfsContainerMappings[f].apfsContainer, apfsContainerMappings[f].apfsDevPath);
            apfsContainerMappings[f].containerFrozen = true;
          }
        }
      }

      status = freezeThawFilesystem(mountList[i].f_mntonname, F_FREEZE_FS);
      HVDBGLOG(stdout, "Freeze status: 0x%X", status);

      if (status != kIOReturnSuccess) {
        //
        // Thaw any frozen filesystems and return error.
        //
        for (int f = 0; f < i; f++) {
          if (strncmp(mountList[f].f_mntfromname, devPath, strlen(devPath)) != 0) {
            continue;
          } else if ((mountList[f].f_flags & MNT_RDONLY) == MNT_RDONLY) {
            continue;
          } else if (!checkFilesystemIsSupported(mountList[f].f_fstypename)) {
            continue;
          }

          freezeThawFilesystem(mountList[f].f_mntonname, F_THAW_FS);
        }

        HVSYSLOG(stderr, "Failed to freeze filesystem %d type '%s' mounted at '%s' from '%s'", i,
                 mountList[i].f_fstypename, mountList[i].f_mntonname, mountList[i].f_mntfromname);
        return status;
      }

    //
    // Thaw filesystem.
    //
    } else if (type == kHyperVSnapshotUserClientNotificationTypeThaw) {
      status = freezeThawFilesystem(mountList[i].f_mntonname, F_THAW_FS);
      HVDBGLOG(stdout, "Thaw status: 0x%X", status);
    }
  }

  //
  // Release APFS mappings on thaw.
  //
  if ((type == kHyperVSnapshotUserClientNotificationTypeThaw) && isApfsSupported()) {
    if (apfsContainerMappings != NULL) {
      free(apfsContainerMappings);
      apfsContainerMappings = NULL;
    }
  }

  return kIOReturnSuccess;
}

static void callUserClientMethod(io_connect_t connection, UInt32 selector, IOReturn status) {
  IOReturn callStatus;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_5
  //
  // Call into user client with standard API.
  //
  UInt64 input64[2];
  input64[0] = kHyperVSnapshotMagic;
  input64[1] = status;
  callStatus = IOConnectCallScalarMethod(connection, selector, input64, 2, NULL, NULL);
#else
  //
  // Call into user client with legacy API.
  //
  callStatus = IOConnectMethodScalarIScalarO(connection, selector, 2, 0, kHyperVSnapshotMagic, status);
#endif

  if (callStatus != kIOReturnSuccess) {
    HVSYSLOG(stderr, "Failed to call user client method %u with status 0x%X", selector, status);
  }
}

void hvIOKitNotificationHandler(io_connect_t connection, CFMachPortRef port, void *msg, CFIndex size, void *info) {
  HyperVSnapshotUserClientNotificationMessage *snapshotMsg = (HyperVSnapshotUserClientNotificationMessage *) msg;
  IOReturn status;

  if (size < __offsetof(HyperVSnapshotUserClientNotificationMessage, type)) {
    HVSYSLOG(stderr, "Invalid message size %u received, should be at least %u",
             size, __offsetof(HyperVSnapshotUserClientNotificationMessage, type));
    return;
  }

  HVDBGLOG(stdout, "Received notification of type 0x%X", snapshotMsg->type);
  switch (snapshotMsg->type) {
    //
    // Always returns magic value, means daemon is alive and can handle a snapshot request.
    //
    case kHyperVSnapshotUserClientNotificationTypeCheck:
    case kHyperVSnapshotUserClientNotificationTypeFreeze:
    case kHyperVSnapshotUserClientNotificationTypeThaw:
      status = freezeThawMountedFilesystems(snapshotMsg->type);
      callUserClientMethod(connection, snapshotMsg->type, status);
      break;

    default:
      HVDBGLOG(stdout, "Unknown notification type 0x%X", snapshotMsg->type);
      break;
  }
}

int main(int argc, const char * argv[]) {
  //
  // Setup I/O Kit notifications.
  //
  if (hvIOKitSetupIOKitNotifications(HYPERV_SNAPSHOT_KERNEL_SERVICE) != kIOReturnSuccess) {
    return -1;
  }

  //
  // Run main loop, this should not return.
  //
  CFRunLoopRun();
  return 0;
}
