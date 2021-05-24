//
//  HyperVMousePrivate.cpp
//  Hyper-V mouse driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVMouse.hpp"

void HyperVMouse::handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count) {
  HyperVVMBusDeviceRequest request = { 0 };
  UInt8 data[256];
  
  //
  // Very large packets should never occur,
  // but we do not want to hold up the packet chain.
  //
  HyperVMousePipeIncomingMessage *message = (HyperVMousePipeIncomingMessage*)data;
  request.responseData = data;
  request.ignoreLargePackets = true;
  
  do {
    //
    // Get next packet.
    //
    request.responseDataLength = sizeof (data);
    IOReturn status = hvDevice->doRequest(&request);
    if (status != kIOReturnSuccess || request.responseDataLength == 0) {
      // TODO: Handle other failures
      return;
    }
    
    switch (message->header.type) {
      case kHyperVMouseMessageTypeInitialDeviceInfo:
        handleDeviceInfo(&message->deviceInfo);
        break;
        
      case kHyperVMouseMessageTypeInputReport:
        handleInputReport(&message->inputReport);
        break;
        
      default:
        DBGLOG("Unknown message type %u, size %u", message->header.type, message->header.size);
        break;
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
  HyperVVMBusDeviceRequest        request = { 0 };
  HyperVMousePipeMessage          message;
  HyperVMousePipeIncomingMessage  respMessage;
  
  message.type = kHyperVPipeMessageTypeData;
  message.size = sizeof (HyperVMouseMessageProtocolRequest);
  
  message.request.header.type = kHyperVMouseMessageTypeProtocolRequest;
  message.request.header.size = sizeof (UInt32);
  message.request.versionRequested = kHyperVMouseVersion;
  
  request.sendData = &message;
  request.sendDataLength = sizeof (message);
  request.sendPacketType = kVMBusPacketTypeDataInband;
  request.responseRequired = true;
  request.responseData = &respMessage;
  request.responseDataLength = sizeof (respMessage);
  
  DBGLOG("Sending mouse protocol request");
  if (hvDevice->doRequest(&request) != kIOReturnSuccess) {
    return false;
  }
  
  DBGLOG("Got mouse protocol response of %u", respMessage.response.status);
  return respMessage.response.status != 0;
}

void HyperVMouse::handleDeviceInfo(HyperVMouseMessageInitialDeviceInfo *deviceInfo) {
  if (deviceInfo->header.size < sizeof (HyperVMouseMessageInitialDeviceInfo) ||
      deviceInfo->info.size < sizeof(deviceInfo->info)) {
    return;
  }
  
  memcpy(&mouseInfo, &deviceInfo->info, sizeof (mouseInfo));
  DBGLOG("Hyper-V Mouse ID %04X:%04X, version 0x%X",
         mouseInfo.vendor, mouseInfo.product, mouseInfo.version);
  
  //
  // Store HID descriptor.
  //
  hidDescriptorLength = deviceInfo->hidDescriptor.hidDescriptorLength;
  DBGLOG("HID descriptor is %u bytes", hidDescriptorLength);
  
  hidDescriptor = IOMalloc(hidDescriptorLength);
  if (hidDescriptor == NULL) {
    return;
  }
  memcpy(hidDescriptor, deviceInfo->hidDescriptorData, hidDescriptorLength);
  hidDescriptorValid = true;
  
  //
  // Send device info ack message.
  //
  HyperVVMBusDeviceRequest  request = { 0 };
  HyperVMousePipeMessage    message;
  
  message.type = kHyperVPipeMessageTypeData;
  message.size = sizeof (HyperVMouseMessageInitialDeviceInfoAck);
  
  message.deviceInfoAck.header.type = kHyperVMouseMessageTypeInitialDeviceInfoAck;
  message.deviceInfoAck.header.size = sizeof (HyperVMouseMessageInitialDeviceInfoAck) - sizeof (HyperVMouseMessageHeader);
  message.deviceInfoAck.reserved = 0;
  
  request.sendData = &message;
  request.sendDataLength = sizeof (message);
  request.sendPacketType = kVMBusPacketTypeDataInband;
  
  DBGLOG("Sending device info ack");
  hvDevice->doRequest(&request);
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
