//
//  HyperVPCIBridge.cpp
//  Hyper-V PCI passthrough device support
//
//  Copyright © 2022 Goldfish64. All rights reserved.
//

#include "HyperVPCIBridge.hpp"

OSDefineMetaClassAndStructors(HyperVPCIBridge, super);

bool HyperVPCIBridge::start(IOService *provider) {
  HVDBGLOG("START");
  return super::start(provider);
}
