//
//  HyperVPCIProvider.cpp
//  Hyper-V PCI root bridge provider
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVPCIProvider.hpp"
#include <IOKit/IODeviceTreeSupport.h>

OSDefineMetaClassAndStructors(HyperVPCIProvider, super);

IOService* HyperVPCIProvider::probe(IOService *provider, SInt32 *score) {
  IORegistryEntry *pciEntry = IORegistryEntry::fromPath("/PCI0@0", gIODTPlane);
  if (pciEntry != NULL) {
    DBGLOG("Existing PCI bus found (Gen1 VM), will not start");
    
    pciEntry->release();
    return NULL;
  }
  
  return this;
}

bool HyperVPCIProvider::start(IOService *provider) {
  //
  // Required by AppleACPIPlatform.
  //
  if (!super::init(provider, NULL, getPropertyTable())) {
    SYSLOG("Failed to initialize parent provider");
    return false;
  }
  
  if (!super::start(provider)) {
    SYSLOG("Failed to start parent provider");
    return false;
  }
  
  registerService();
  DBGLOG("Hyper-V PCI provider is now registered");
  return true;
}
