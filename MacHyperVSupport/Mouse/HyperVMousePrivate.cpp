//
//  HyperVMousePrivate.cpp
//  Hyper-V mouse driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVMouse.hpp"

void HyperVMouse::handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count) {
  UInt8 data128[128];

  do {
    //
    // Check for available inband packets.
    // Large packets will be allocated as needed.
    //
    HyperVMousePipeIncomingMessage *message;
    UInt32 pktDataLength;
    if (!hvDevice->nextInbandPacketAvailable(&pktDataLength)) {
      break;
    }

    if (pktDataLength <= sizeof (data128)) {
      message = (HyperVMousePipeIncomingMessage*)data128;
    } else {
      HVDBGLOG("Allocating large packet of %u bytes", pktDataLength);
      message = (HyperVMousePipeIncomingMessage*)IOMalloc(pktDataLength);
    }

    //
    // Read next packet.
    //
    UInt64 transactionId;
    if (hvDevice->readInbandCompletionPacket((void *)message, pktDataLength, &transactionId) == kIOReturnSuccess) {
      // TODO: Handle other failures
      switch (message->header.type) {
        case kHyperVMouseMessageTypeProtocolResponse:
          handleProtocolResponse(&message->response, transactionId);
          break;

        case kHyperVMouseMessageTypeInitialDeviceInfo:
          handleDeviceInfo(&message->deviceInfo);
          break;

        case kHyperVMouseMessageTypeInputReport:
          handleInputReport(&message->inputReport);
          break;

        default:
          HVDBGLOG("Unknown message type %u, size %u", message->header.type, message->header.size);
          break;
      }
    }

    //
    // Free allocated packet if needed.
    //
    if (pktDataLength > sizeof (data128)) {
      IOFree(message, pktDataLength);
    }
  } while (true);
}

bool HyperVMouse::setupMouse() {
  //
  // Device info is invalid.
  //
  hidDescriptorValid = false;

  //
  // Send mouse request.
  //
  HyperVMousePipeMessage              message;
  HyperVMouseMessageProtocolResponse  protoResponse;

  message.type = kHyperVPipeMessageTypeData;
  message.size = sizeof (HyperVMouseMessageProtocolRequest);

  message.request.header.type = kHyperVMouseMessageTypeProtocolRequest;
  message.request.header.size = sizeof (UInt32);
  message.request.versionRequested = kHyperVMouseVersion;

  HVDBGLOG("Sending mouse protocol request");
  if (hvDevice->writeInbandPacket(&message, sizeof (message), true, &protoResponse, sizeof (protoResponse)) != kIOReturnSuccess) {
    return false;
  }

  HVDBGLOG("Got mouse protocol response of %u, version 0x%X", protoResponse.status, protoResponse.versionRequested);
  return protoResponse.status != 0;
}

void HyperVMouse::handleProtocolResponse(HyperVMouseMessageProtocolResponse *response, UInt64 transactionId) {
  void    *responseBuffer;
  UInt32  responseLength;

  if (hvDevice->getPendingTransaction(transactionId, &responseBuffer, &responseLength)) {
    if (sizeof (*response) > responseLength) {
      return;
    }
    memcpy(responseBuffer, response, responseLength);
    hvDevice->wakeTransaction(transactionId);
  }
}

void HyperVMouse::handleDeviceInfo(HyperVMouseMessageInitialDeviceInfo *deviceInfo) {
  if (deviceInfo->header.size < sizeof (HyperVMouseMessageInitialDeviceInfo) ||
      deviceInfo->info.size < sizeof(deviceInfo->info)) {
    return;
  }

  memcpy(&mouseInfo, &deviceInfo->info, sizeof (mouseInfo));
  HVDBGLOG("Hyper-V Mouse ID %04X:%04X, version 0x%X",
         mouseInfo.vendor, mouseInfo.product, mouseInfo.version);

  //
  // Store HID descriptor.
  //
  hidDescriptorLength = deviceInfo->hidDescriptor.hidDescriptorLength;
  HVDBGLOG("HID descriptor is %u bytes", hidDescriptorLength);

  hidDescriptor = IOMalloc(hidDescriptorLength);
  if (hidDescriptor == NULL) {
    return;
  }
  memcpy(hidDescriptor, deviceInfo->hidDescriptorData, hidDescriptorLength);
  hidDescriptorValid = true;

  //
  // Send device info ack message.
  //
  HyperVMousePipeMessage message;
  message.type = kHyperVPipeMessageTypeData;
  message.size = sizeof (HyperVMouseMessageInitialDeviceInfoAck);

  message.deviceInfoAck.header.type = kHyperVMouseMessageTypeInitialDeviceInfoAck;
  message.deviceInfoAck.header.size = sizeof (HyperVMouseMessageInitialDeviceInfoAck) - sizeof (HyperVMouseMessageHeader);
  message.deviceInfoAck.reserved = 0;

  HVDBGLOG("Sending device info ack");
  hvDevice->writeInbandPacket(&message, sizeof (message), false);
}

void HyperVMouse::handleInputReport(HyperVMouseMessageInputReport *inputReport) {
  //
  // Send new report to HID system.
  //
  IOBufferMemoryDescriptor *memDesc = IOBufferMemoryDescriptor::withBytes(inputReport->data, inputReport->header.size, kIODirectionNone);
  if (memDesc != NULL) {
    handleReport(memDesc);
    memDesc->release();
  }
}
