//
//  HyperVShutdown.cpp
//  Hyper-V guest shutdown driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVShutdown.hpp"

#define super HyperVICService

#define SYSLOG(str, ...) SYSLOG_PRINT("HyperVShutdown", str, ## __VA_ARGS__)
#define DBGLOG(str, ...) DBGLOG_PRINT("HyperVShutdown", str, ## __VA_ARGS__)

extern "C" int reboot_kernel(int, char *);

OSDefineMetaClassAndStructors(HyperVShutdown, super);

bool HyperVShutdown::start(IOService *provider) {
  if (!super::start(provider)) {
    return false;
  }
  
  SYSLOG("Initialized Hyper-V Guest Shutdown");
  return true;
}

void HyperVShutdown::processMessage() {
  VMBusICMessageShutdown shutdownMsg;
  
  HyperVVMBusDeviceRequest request = { 0 };
  request.responseData = &shutdownMsg;
  request.responseDataLength = sizeof (shutdownMsg);
  
  //
  // Ignore errors and the acknowledgement interrupt (no data to read).
  //
  if (hvDevice->doRequest(&request) != kIOReturnSuccess || request.responseDataLength == 0) {
    return;
  }
  
  switch (shutdownMsg.header.type) {
    case kVMBusICMessageTypeNegotiate:
      createNegotiationResponse(&shutdownMsg.negotiate, 3, 3);
      break;
      
    case kVMBusICMessageTypeShutdown:
      handleShutdown(&shutdownMsg.shutdown);
      break;
      
    default:
      DBGLOG("Unknown shutdown message type %u", shutdownMsg.header.type);
      shutdownMsg.header.status = kHyperVStatusFail;
      break;
  }
  
  shutdownMsg.header.flags = kVMBusICFlagTransaction | kVMBusICFlagResponse;
  
  UInt32 sendLength = request.responseDataLength;
  
  request = { 0 };
  request.sendData = &shutdownMsg;
  request.sendDataLength = sendLength;
  request.sendPacketType = kVMBusPacketTypeDataInband;
  
  hvDevice->doRequest(&request);
}

void HyperVShutdown::handleShutdown(VMBusICMessageShutdownData *shutdownData) {
  SYSLOG("Shutdown request received, flags 0x%X, reason 0x%X", shutdownData->flags, shutdownData->reason);
  
  //
  // Shutdown currently not supported.
  //
  shutdownData->header.status = kHyperVStatusFail;
}
