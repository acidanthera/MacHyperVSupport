//
//  hvsnapshotd.c
//  Hyper-V VSS snapshot userspace daemon
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "HyperVSnapshotUserClient.h"
#include "hvdebug.h"
#include "hviokit.h"

#include <paths.h>
#include <sys/mount.h>
#include <sys/utsname.h>

#include <DiskArbitration/DiskArbitration.h>

#define HYPERV_SNAPSHOT_KERNEL_SERVICE  "HyperVSnapshot"
#define DARWIN_VERSION_HIGHSIERRA       17

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

static CFStringRef apfsGetDiskContainer(const char *diskPath) __attribute__((cf_returns_retained)) {
  DASessionRef    session           = NULL;
  DADiskRef       disk              = NULL;
  io_service_t    media             = IO_OBJECT_NULL;
  io_service_t    partitionScheme   = IO_OBJECT_NULL;
  io_service_t    mediaContainer    = IO_OBJECT_NULL;
  CFTypeRef       containerBSDName  = NULL;

  kern_return_t   status            = KERN_SUCCESS;
  CFStringRef     resultStr         = NULL;

  do {
    //
    // Get disk object for path.
    //
    session = DASessionCreate(kCFAllocatorDefault);
    if (session == NULL) {
      HVSYSLOG(stderr, "Failed to create DiskArbitration session");
      break;
    }
    disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, diskPath);
    if (disk == NULL) {
      HVSYSLOG(stderr, "Failed to get DiskArbitration disk for '%s'", diskPath);
      break;
    }

    //
    // Get IOMedia object.
    //
    media = DADiskCopyIOMedia(disk);
    if (media == IO_OBJECT_NULL) {
      HVSYSLOG(stderr, "Failed to get IOMedia service for '%s'", diskPath);
      break;
    }

    //
    // Get parent APFSPartitionScheme and AppleAPFSMedia.
    //
    status = IORegistryEntryGetParentEntry(media, kIOServicePlane, &partitionScheme);
    if ((status != KERN_SUCCESS) || IOObjectConformsTo(partitionScheme, "APFSPartitionScheme")) {
      HVSYSLOG(stderr, "Failed to get parent APFSPartitionScheme service for '%s'", diskPath);
      break;
    }
    status = IORegistryEntryGetParentEntry(partitionScheme, kIOServicePlane, &mediaContainer);
    if ((status != KERN_SUCCESS) || IOObjectConformsTo(mediaContainer, "APFSMedia")) {
      HVSYSLOG(stderr, "Failed to get parent APFSMedia service for '%s'", diskPath);
      break;
    }

    //
    // Get BSD name for AppleAPFSMedia service.
    //
    containerBSDName = IORegistryEntryCreateCFProperty(mediaContainer, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
    if ((containerBSDName == NULL) || (CFGetTypeID(containerBSDName) != CFStringGetTypeID())) {
      HVSYSLOG(stderr, "Failed to get name for parent APFSMedia service for '%s'", diskPath);
      break;
    }

    resultStr = CFStringCreateCopy(kCFAllocatorDefault, containerBSDName);
  } while (false);

  if (containerBSDName != NULL) {
    CFRelease(containerBSDName);
  }
  if (mediaContainer != IO_OBJECT_NULL) {
    IOObjectRelease(mediaContainer);
  }
  if (partitionScheme != IO_OBJECT_NULL) {
    IOObjectRelease(partitionScheme);
  }
  if (media != IO_OBJECT_NULL) {
    IOObjectRelease(media);
  }
  if (disk != NULL) {
    CFRelease(disk);
  }
  if (session != NULL) {
    CFRelease(session);
  }

  return resultStr;
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
    if (strncmp(mountList[i].f_mntfromname, _PATH_DEV, strlen(_PATH_DEV)) != 0) {
      HVDBGLOG(stdout, "Skipping non-disk filesystem");
      continue;
    } else if (isReadOnly) {
      HVDBGLOG(stdout, "Skipping read only filesystem");
      continue;
    } else if (!checkFilesystemIsSupported(mountList[i].f_fstypename)) {
      HVDBGLOG(stdout, "Skipping unsupported filesystem");
      continue;
    }

    if (isApfs && !isApfsSupported()) {
      // TODO: Fail if APFS is found on Sierra.
      HVDBGLOG(stdout, "Skipping unsupported APFS filesystem");
      continue;
    }

    //
    // Populate APFS mappings if we are in the check phase.
    //
    if ((type == kHyperVSnapshotUserClientNotificationTypeCheck) && isApfs) {
      strncpy(apfsContainerMappings[i].apfsDevPath, mountList[i].f_mntfromname, sizeof (apfsContainerMappings[i].apfsDevPath) - 1);
      apfsContainerMappings[i].apfsDevPath[sizeof (apfsContainerMappings[i].apfsDevPath) - 1] = '\0';

      apfsContainerStr = apfsGetDiskContainer(mountList[i].f_mntfromname);
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
          if (strncmp(mountList[f].f_mntfromname, _PATH_DEV, strlen(_PATH_DEV)) != 0) {
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
