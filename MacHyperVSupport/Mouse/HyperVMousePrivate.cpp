//
//  HyperVMousePrivate.cpp
//  Hyper-V mouse driver
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#include "HyperVMouse.hpp"

void HyperVMouse::handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  HyperVMousePipeIncomingMessage *mouseMsg = (HyperVMousePipeIncomingMessage*) pktData;
  
  switch (mouseMsg->header.type) {
    case kHyperVMouseMessageTypeProtocolResponse:
      //
      // Initial protocol response during startup.
      //
      handleProtocolResponse(&mouseMsg->response);
      break;

    case kHyperVMouseMessageTypeInitialDeviceInfo:
      //
      // Device info result.
      //
      handleDeviceInfo(&mouseMsg->deviceInfo);
      break;

    case kHyperVMouseMessageTypeInputReport:
      //
      // Normal input packets.
      //
      handleInputReport(&mouseMsg->inputReport);
      break;

    default:
      HVDBGLOG("Unknown message type %u, size %u", mouseMsg->header.type, mouseMsg->header.size);
      break;
  }
}

bool HyperVMouse::setupMouse() {
  //
  // Send mouse request.
  // Fixed transaction ID is used for tracking as the response is always zero.
  //
  HyperVMousePipeMessage             message;
  HyperVMouseMessageProtocolResponse protoResponse;

  message.type = kHyperVPipeMessageTypeData;
  message.size = sizeof (HyperVMouseMessageProtocolRequest);

  message.request.header.type      = kHyperVMouseMessageTypeProtocolRequest;
  message.request.header.size      = sizeof (UInt32);
  message.request.versionRequested = kHyperVMouseVersion;

  HVDBGLOG("Sending mouse protocol request");
  if (_hvDevice->writeInbandPacketWithTransactionId(&message, sizeof (message), kHyperVMouseProtocolRequestTransactionID, true, &protoResponse, sizeof (protoResponse)) != kIOReturnSuccess) {
    return false;
  }

  HVDBGLOG("Got mouse protocol response of %u, version 0x%X", protoResponse.status, protoResponse.versionRequested);
  if (protoResponse.status == 0) {
    return false;
  }

  //
  // Wait for device info packet to be received.
  // The VMBusDevice nub reserves a special transaction ID 0 for general purpose waiting.
  //
  // The device info packet is sent unsolicitated by Hyper-V and we cannot continue until that is received.
  //
  _hvDevice->sleepThreadZero();
  return true;
}

void HyperVMouse::handleProtocolResponse(HyperVMouseMessageProtocolResponse *response) {
  void   *responseBuffer;
  UInt32 responseLength;

  //
  // The protocol response packet always has a transaction ID of 0, wake using fixed transaction ID.
  // This assumes the response length is of the correct size, set before the initial request.
  //
  if (_hvDevice->getPendingTransaction(kHyperVMouseProtocolRequestTransactionID, &responseBuffer, &responseLength)) {
    memcpy(responseBuffer, response, responseLength);
    _hvDevice->wakeTransaction(kHyperVMouseProtocolRequestTransactionID);
  }
}

void HyperVMouse::handleDeviceInfo(HyperVMouseMessageInitialDeviceInfo *deviceInfo) {
  IOReturn status;
  
  if (deviceInfo->header.size < sizeof (*deviceInfo)
      || deviceInfo->info.size < sizeof (deviceInfo->info)) {
    return;
  }

  memcpy(&_mouseInfo, &deviceInfo->info, sizeof (_mouseInfo));
  HVDBGLOG("Hyper-V Mouse ID %04X:%04X, version 0x%X",
         _mouseInfo.vendor, _mouseInfo.product, _mouseInfo.version);

  //
  // Store HID descriptor.
  //
  _hidDescriptorLength = deviceInfo->hidDescriptor.hidDescriptorLength;
  HVDBGLOG("HID descriptor is %u bytes", _hidDescriptorLength);

  _hidDescriptor = IOMalloc(_hidDescriptorLength);
  if (_hidDescriptor == nullptr) {
    HVSYSLOG("Failed to allocate HID descriptor");
    return;
  }
  memcpy(_hidDescriptor, deviceInfo->hidDescriptorData, _hidDescriptorLength);

  //
  // Send device info ack message.
  //
  HyperVMousePipeMessage message;
  message.type = kHyperVPipeMessageTypeData;
  message.size = sizeof (message.deviceInfoAck);

  message.deviceInfoAck.header.type = kHyperVMouseMessageTypeInitialDeviceInfoAck;
  message.deviceInfoAck.header.size = sizeof (message.deviceInfoAck) - sizeof (message.deviceInfoAck.header);
  message.deviceInfoAck.reserved    = 0;

  HVDBGLOG("Sending device info ack");
  status = _hvDevice->writeInbandPacket(&message, sizeof (message), false);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to send device info ack with status 0x%X", status);
  }
  _hvDevice->wakeThreadZero();
}

void HyperVMouse::handleInputReport(HyperVMouseMessageInputReport *inputReport) {
#if DEBUG
  typedef struct __attribute__((packed)) {
    UInt8  buttons;
    UInt16 x;
    UInt16 y;
    SInt8  wheel;
    SInt8  pan;
  } HIDABSReport;

  HIDABSReport *report = (HIDABSReport*)inputReport->data;
  HVDBGLOG("Got mouse input buttons %u X: %u Y: %u wheel: %d pan: %d", report->buttons, report->x, report->y, report->wheel, report->pan);
#endif

  //
  // Send new report to HID system.
  //
  IOBufferMemoryDescriptor *memDesc = IOBufferMemoryDescriptor::withBytes(inputReport->data, inputReport->header.size, kIODirectionNone);
  if (memDesc == nullptr) {
    HVSYSLOG("Failed to allocate input report descriptor");
    return;
  }
  
  handleReport(memDesc);
  memDesc->release();
}
