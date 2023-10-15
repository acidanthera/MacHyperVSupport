//
//  DevicePathPropertyDatabase.hpp
//  Device Path Property Database definitions
//
//  Copyright © 2023 Goldfish64. All rights reserved.
//

#ifndef DevicePathPropertyDatabase_h
#define DevicePathPropertyDatabase_h

#define EFI_DEVICE_PATH_PROPERTY_DATABASE_VERSION  1

/**
  This protocol can be used on any device handle to obtain generic path/location
  information concerning the physical device or logical device. If the handle does
  not logically map to a physical device, the handle may not necessarily support
  the device path protocol. The device path describes the location of the device
  the handle is for. The size of the Device Path can be determined from the structures
  that make up the Device Path.
**/
#define HARDWARE_DEVICE_PATH  0x01
#define HW_VENDOR_DP  0x04

#define END_DEVICE_PATH 0x7F
#define END_DEVICE_PATH_SUBTYPE 0xFF

//
// Generic EFI device path node.
//
typedef struct {
  UInt8   type;
  UInt8   subType;
  UInt8   length[2];
} EfiDevicePathProtocol;

//
// Hyper-V vendor-specific EFI device path node.
//
typedef struct {
  EfiDevicePathProtocol header;

  uuid_t  vendorGuid;
  uuid_t  deviceTypeGuid;
  uuid_t  instanceGuid;
} HyperVDevPropEfiDevicePathProtocol;

//
// Device path property data set.
// There should be a pair of these for each property present,
// one for the property name (in unicode) and one for the data.
//
typedef struct {
  UInt32  Size;
  UInt8   Data[];
} EfiDevicePathPropertyData;

//
// Device path property node header.
//
typedef struct {
  UInt32  Size;
  UInt32  NumberOfProperties;
} EfiDevicePathPropertyBufferNodeHeader;

//
// Device path property node.
//
typedef struct {
  EfiDevicePathPropertyBufferNodeHeader Hdr;
  HyperVDevPropEfiDevicePathProtocol    DevicePath; // Variable length
  //EfiDevicePathPropertyData           Data;       // Variable length
} EfiDevicePathPropertyBufferNode;

//
// Device path property buffer.
//
typedef struct {
  UInt32  Size;
  UInt32  Version;
  UInt32  NumberOfNodes;

  EfiDevicePathPropertyBufferNode Nodes[]; // Each node is variable in length.
} EfiDevicePathPropertyBuffer;

#endif
