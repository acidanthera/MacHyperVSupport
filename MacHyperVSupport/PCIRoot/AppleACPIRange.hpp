//
//  AppleACPIRange.hpp
//  Apple ACPI range structure exposed in acpi-address-spaces properties
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#ifndef AppleACPIRange_hpp
#define AppleACPIRange_hpp

typedef struct __attribute__((packed)) {
  UInt64 type;
  UInt64 reserved1;
  UInt64 reserved2;
  UInt64 min;
  UInt64 max;
  UInt64 reserved3;
  UInt64 length;
  UInt64 reserved4;
  UInt64 reserved5;
  UInt64 reserved6;
} AppleACPIRange;

#endif
