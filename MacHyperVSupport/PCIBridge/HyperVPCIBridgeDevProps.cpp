//
//  HyperVPCIBridgePrivate.cpp
//  Hyper-V PCI passthrough device properties support
//
//  Copyright © 2023 Goldfish64. All rights reserved.
//

#include "HyperVPCIBridge.hpp"
#include "HyperV.hpp"

#include "DevicePathPropertyDatabase.hpp"
#include <IOKit/IODeviceTreeSupport.h>

#include <pexpert/i386/efi.h>
#include <sys/utfconv.h>

const EFI_GUID VMBusDPGuid = VMBUS_EFI_DEVICE_PATH_GUID;
const EFI_GUID HyperVPCIDPGuid = HYPERV_PCI_EFI_DEVICE_PATH_GUID;

inline UInt16 getDevicePathNodeLength(const EfiDevicePathProtocol *node) {
  return OSReadLittleInt16(node->length, 0);
}

inline EfiDevicePathProtocol* getNextDevicePathNode(const EfiDevicePathProtocol *node) {
  return (EfiDevicePathProtocol*)((UInt8*)(node) + getDevicePathNodeLength(node));
}

inline bool isDevicePathNodeEnd(const EfiDevicePathProtocol *node) {
  return (node->type == END_DEVICE_PATH) && (node->subType == END_DEVICE_PATH_SUBTYPE);
}

static UInt32 getDevicePathSize(const EfiDevicePathProtocol *devicePath) {
  UInt32 devicePathSize = 0;
  const EfiDevicePathProtocol *node = devicePath;

  //
  // Get device path size.
  //
  while (true) {
    devicePathSize += getDevicePathNodeLength(node);
    if (isDevicePathNodeEnd(node)) {
      break;
    }
    node = getNextDevicePathNode(node);
  }
  
  return devicePathSize;
}

IOReturn HyperVPCIBridge::mergePropertiesFromDT(UInt32 slot, OSDictionary *dict) {
  IOReturn status;

  //
  // Get device-properties property from IODT.
  //
  IORegistryEntry *entry = IORegistryEntry::fromPath("/efi", gIODTPlane);
  if (entry == nullptr) {
    HVSYSLOG("Failed to locate IODT:/efi registry entry");
    return kIOReturnNotFound;
  }

  OSData *deviceProperties = OSDynamicCast(OSData, entry->getProperty("device-properties"));
  if (deviceProperties == nullptr) {
    HVSYSLOG("Failed to locate device-properties");
    entry->release();
    return kIOReturnNotFound;
  }

  //
  // Parse device-properties structure.
  //
  const EfiDevicePathPropertyBuffer *devicePropertiesBuffer =
    static_cast<const EfiDevicePathPropertyBuffer*>(deviceProperties->getBytesNoCopy());
  if (deviceProperties->getLength() < sizeof (*devicePropertiesBuffer) ||
      deviceProperties->getLength() != devicePropertiesBuffer->Size) {
    HVSYSLOG("Size of device-properties does not match expected size");
    entry->release();
    return kIOReturnIOError;
  }
  if (devicePropertiesBuffer->Version != EFI_DEVICE_PATH_PROPERTY_DATABASE_VERSION) {
    HVSYSLOG("Version of device-properties is unexpected: 0x%X", devicePropertiesBuffer->Version);
    entry->release();
    return kIOReturnUnsupported;
  }
  HVDBGLOG("Got device-properties (%u bytes) with %u nodes", devicePropertiesBuffer->Size, devicePropertiesBuffer->NumberOfNodes);

  //
  // Look for our device path.
  // VenHw(9B17E5A2-0891-42DD-B653-80B5C22809BA,1DF6C444444400449D52802E27EDE19F...instance ID GUID...)
  //
  status = kIOReturnSuccess;
  const EfiDevicePathPropertyBufferNode *node = devicePropertiesBuffer->Nodes;
  for (int i = 0; i < devicePropertiesBuffer->NumberOfNodes; i++, node = (const EfiDevicePathPropertyBufferNode *)((UInt8*)node + node->Hdr.Size)) {
    if (devicePropertiesBuffer->Size < node->Hdr.Size) {
      HVSYSLOG("Invalid node size %u bytes", node->Hdr.Size);
      status = kIOReturnIOError;
      break;
    }

    const HyperVDevPropEfiDevicePathProtocol *devicePath = &node->DevicePath;
    const EfiDevicePathProtocol *devicePathHdr = &devicePath->header;

    //
    // Skip non Hyper-V PCI paths and paths belonging to other devices.
    //
    if (devicePathHdr->type != HARDWARE_DEVICE_PATH || devicePathHdr->subType != HW_VENDOR_DP) {
      continue;
    }
    if (memcmp(devicePath->vendorGuid, &VMBusDPGuid, sizeof (devicePath->vendorGuid)) != 0 ||
        getDevicePathNodeLength(devicePathHdr) < sizeof(*devicePath)) {
      continue;
    }
    if (memcmp(devicePath->deviceTypeGuid, &HyperVPCIDPGuid, sizeof (devicePath->deviceTypeGuid)) != 0 ||
        memcmp(devicePath->instanceGuid, _hvDevice->getInstanceId(), sizeof (devicePath->instanceGuid)) != 0) {
      continue;
    }

    //
    // Validate sizes.
    //
    UInt32 devicePathSize = getDevicePathSize(devicePathHdr);
    if (node->Hdr.Size < devicePathSize ||
        (node->Hdr.Size - devicePathSize) < (sizeof (EfiDevicePathPropertyData) * node->Hdr.NumberOfProperties)) {
      HVSYSLOG("Invalid node size %u bytes", node->Hdr.Size);
      status = kIOReturnIOError;
      break;
    }

    if (debugEnabled) {
      uuid_string_t instanceGuidStr;
      guid_unparse(*_hvDevice->getInstanceId(), instanceGuidStr);
      HVDBGLOG("Matched device properties for PCI device instance %s", instanceGuidStr);
    }

    const EfiDevicePathPropertyData *nodeProperties = (const EfiDevicePathPropertyData*)((UInt8*)&node->DevicePath + devicePathSize);
    HVDBGLOG("%u properties located at 0x%X (%u bytes)", node->Hdr.NumberOfProperties,
             (UInt8*)nodeProperties - (UInt8*)node, node->Hdr.Size - devicePathSize);

    //
    // Iterate through property pairs and populate dictionary.
    //
    UInt32 nodeCurrentSize = 0;
    const EfiDevicePathPropertyData *nodePropertiesPtr = nodeProperties;
    for (int p = 0; p < node->Hdr.NumberOfProperties; p++) {
      //
      // Get property name data.
      //
      nodeCurrentSize += nodePropertiesPtr->Size;
      if (nodeCurrentSize > node->Hdr.Size || nodePropertiesPtr->Size < sizeof (*nodePropertiesPtr)) {
        HVSYSLOG("Property size is invalid");
        status = kIOReturnIOError;
        break;
      }

      //
      // Convert from UTF16 to UTF8.
      //
      size_t utf16Length = nodePropertiesPtr->Size - __offsetof (EfiDevicePathPropertyData, Data);
      size_t utf8Length = 0;
      size_t utf8MaxLength = utf16Length / sizeof (UInt16);
      UInt8 *utf8Buffer = (UInt8*) IOMalloc(utf8MaxLength);
      if (utf8Buffer == nullptr) {
        HVSYSLOG("Failed to allocate property name buffer");
        status = kIOReturnNoResources;
        break;
      }
      utf8_encodestr(reinterpret_cast<const UInt16*>(nodePropertiesPtr->Data), utf16Length,
                     utf8Buffer, &utf8Length, utf8MaxLength, '/', UTF_LITTLE_ENDIAN);

      //
      // Get property value.
      //
      nodePropertiesPtr = (const EfiDevicePathPropertyData *)((UInt8*)nodePropertiesPtr + nodePropertiesPtr->Size);
      nodeCurrentSize += nodePropertiesPtr->Size;
      if (nodeCurrentSize > node->Hdr.Size || nodePropertiesPtr->Size < sizeof (*nodePropertiesPtr)) {
        IOFree(utf8Buffer, utf8MaxLength);
        HVSYSLOG("Property size is invalid");
        status = kIOReturnIOError;
        break;
      }

      UInt32 dataLength = nodePropertiesPtr->Size - __offsetof (EfiDevicePathPropertyData, Data);
      HVDBGLOG("Property %s -> %u bytes", utf8Buffer, dataLength);
      
      //
      // Set property name/value pair in dictionary.
      //
      OSString *propString = OSString::withCString((const char*) utf8Buffer);
      IOFree(utf8Buffer, utf8MaxLength);
      OSData *propData = OSData::withBytes(nodePropertiesPtr->Data, dataLength);
      if (propString == nullptr || propData == nullptr) {
        OSSafeReleaseNULL(propString);
        OSSafeReleaseNULL(propData);
        HVSYSLOG("Failed to allocate property data");
        status = kIOReturnNoResources;
        break;
      }

      bool result = dict->setObject(propString, propData);
      propString->release();
      propData->release();
      if (!result) {
        HVSYSLOG("Failed to add property data");
        status = kIOReturnNoResources;
        break;
      }

      nodePropertiesPtr = (const EfiDevicePathPropertyData *)((UInt8*)nodePropertiesPtr + nodePropertiesPtr->Size);
    }

    if (status != kIOReturnSuccess) {
      break;
    }
  }
  entry->release();

  return status;
}
