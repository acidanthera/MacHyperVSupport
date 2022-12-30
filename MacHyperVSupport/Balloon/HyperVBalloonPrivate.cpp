//
//  HyperVBalloonPrivate.cpp
//  Hyper-V balloon driver internal implementation
//
//  Copyright © 2022-2023 xdqi. All rights reserved.
//

#include "HyperVBalloon.hpp"
#include "HyperVBalloonRegs.hpp"

bool HyperVBalloon::setupBalloon() {
  
  return true;
}

void HyperVBalloon::handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  HyperVDynamicMemoryMessage *balloonMessage = (HyperVDynamicMemoryMessage*) pktData;
  
  switch (balloonMessage->type) {
    case kDynamicMemoryMessageTypeVersionResponse:
      // handle it
      break;

    default:
      HVDBGLOG("Unknown message type %u, size %u", balloonMessage->type, balloonMessage->size);
      break;
  }
}

IOReturn sendStatusReport(void*, void*, void*) {
  
  return kIOReturnSuccess;
}
