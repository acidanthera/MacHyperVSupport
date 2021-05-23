//
//  HyperVHeartbeat.cpp
//  Hyper-V heartbeat driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVHeartbeat.hpp"

#include <IOKit/IOPlatformExpert.h>

#define super HyperVICService

#define SYSLOG(str, ...) SYSLOG_PRINT("HyperVHeartbeat", str, ## __VA_ARGS__)
#define DBGLOG(str, ...) DBGLOG_PRINT("HyperVHeartbeat", str, ## __VA_ARGS__)

OSDefineMetaClassAndStructors(HyperVHeartbeat, super);

bool HyperVHeartbeat::start(IOService *provider) {
  DBGLOG("Initializing Hyper-V Heartbeat");
  return super::start(provider);
}

void HyperVHeartbeat::processMessage() {
  VMBusICMessageHeartbeat heartbeatMsg;
  
  HyperVVMBusDeviceRequest request = { 0 };
  request.responseData = &heartbeatMsg;
  request.responseDataLength = sizeof (heartbeatMsg);
  
  //
  // Ignore errors and the acknowledgement interrupt (no data to read).
  //
  if (hvDevice->doRequest(&request) != kIOReturnSuccess || request.responseDataLength == 0) {
    return;
  }
  
  switch (heartbeatMsg.header.type) {
    case kVMBusICMessageTypeNegotiate:
      firstHeartbeatReceived = false;
      createNegotiationResponse(&heartbeatMsg.negotiate, 3, 3);
      break;
      
    case kVMBusICMessageTypeHeartbeat:
      //
      // Increment sequence.
      // Host will increment this further before sending a message back.
      //
      //DBGLOG("Got heartbeat, seq = %u", heartbeatMsg.heartbeat.sequence);
      heartbeatMsg.heartbeat.sequence++;
      
      if (!firstHeartbeatReceived) {
        firstHeartbeatReceived = true;
        SYSLOG("Initialized Hyper-V Heartbeat");
      }
      break;
      
    default:
      DBGLOG("Unknown IC message type %u", heartbeatMsg.header.type);
      heartbeatMsg.header.status = kHyperVStatusFail;
      break;
  }
  
  heartbeatMsg.header.flags = kVMBusICFlagTransaction | kVMBusICFlagResponse;
  
  UInt32 sendLength = request.responseDataLength;
  
  request = { 0 };
  request.sendData = &heartbeatMsg;
  request.sendDataLength = sendLength;
  request.sendPacketType = kVMBusPacketTypeDataInband;
  
  hvDevice->doRequest(&request);
}
