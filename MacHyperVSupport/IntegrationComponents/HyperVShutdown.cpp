//
//  HyperVShutdown.cpp
//  Hyper-V guest shutdown driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVShutdown.hpp"

#define super HyperVICService

#define SYSLOG(str, ...) LOG_PRINT("HyperVShutdown", str, ## __VA_ARGS__)

#ifdef DEBUG
#define DBGLOG(str, ...) LOG_PRINT("HyperVShutdown", str, ## __VA_ARGS__)
#else
#define DBGLOG(str, ...) {}
#endif

extern "C" int reboot_kernel(int, char *);

OSDefineMetaClassAndStructors(HyperVShutdown, super);

bool HyperVShutdown::start(IOService *provider) {
  DBGLOG("Starting guest shutdown service");
  return super::start(provider);
}

inline void processShutdown(VMBusICMessageShutdown* shutdownMsg) {
  DBGLOG("Got shutdown request %X", shutdownMsg->flags);
  reboot_kernel(0x08, "test");
}

void HyperVShutdown::processMessage() {
  DBGLOG("Interrupt!");
  
  UInt8 data[2100];
  UInt32 dataSize = 2100;
  
  VMBusICMessageHeader *icHeader = (VMBusICMessageHeader*)data;
  
  /*bool result = hvDevice->readPacket(data, &dataSize);
  if (!result || dataSize == 0) {
    return;
  }*/
  
  switch (icHeader->type) {
    case kVMBusICMessageTypeNegotiate:
      createNegotiationResponse((VMBusICMessageNegotiate*) icHeader, 3, 3);
      break;
      
    case kVMBusICMessageTypeShutdown:
      processShutdown((VMBusICMessageShutdown*)icHeader);
      break;
      
    default:
      DBGLOG("Unknown IC message type %u", icHeader->type);
      icHeader->status = kHyperVStatusFail;
      break;
  }
  
  icHeader->flags = kVMBusICFlagTransaction | kVMBusICFlagResponse;
 // hvDevice->writePacket(data, dataSize);
}
