//
//  HyperVGraphicsPrivate.cpp
//  Hyper-V synthetic graphics driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "HyperVGraphics.hpp"
#include "HyperVModuleDevice.hpp"
#include "HyperVPCIRoot.hpp"
#include "HyperVPlatformProvider.hpp"

static const VMBusVersion graphicsVersions[] = {
  kHyperVGraphicsVersionV3_2,
  kHyperVGraphicsVersionV3_0
};

/*bool HyperVGraphics::wakePacketHandler(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  HyperVGraphicsMessage *gfxMsg = (HyperVGraphicsMessage*) pktData;
  HVDBGLOG("Received packet type 0x%X (%u bytes)", gfxMsg->gfxHeader.type, gfxMsg->gfxHeader.size);
  switch (gfxMsg->gfxHeader.type) {
    case kHyperVGraphicsMessageTypeVersionResponse:
    case kHyperVGraphicsMessageTypeVRAMAck:
    case kHyperVGraphicsMessageTypeResolutionUpdateAck:
      return true;

    default:
      break;
  }
  
  return false;
}*/

void HyperVGraphics::handleRefreshTimer(IOTimerEventSource *sender) {
  HyperVGraphicsMessage gfxMsg = { };

  if (_fbReady) {
    //
    // Send screen image update to Hyper-V.
    //
    gfxMsg.gfxHeader.type = kHyperVGraphicsMessageTypeDIRT;
    gfxMsg.gfxHeader.size = sizeof (gfxMsg.gfxHeader) + sizeof (gfxMsg.dirt);

    gfxMsg.dirt.videoOutput     = 0;
    gfxMsg.dirt.dirtCount       = 1;
    gfxMsg.dirt.dirtRects[0].x1 = 0;
    gfxMsg.dirt.dirtRects[0].y1 = 0;
    gfxMsg.dirt.dirtRects[0].x2 = _screenWidth;
    gfxMsg.dirt.dirtRects[0].y2 = _screenHeight;

    sendGraphicsMessage(&gfxMsg);
    _timerEventSource->setTimeoutMS(kHyperVGraphicsDIRTRefreshRateMS);
  }
}

void HyperVGraphics::handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
  HyperVGraphicsMessage *gfxMsg = (HyperVGraphicsMessage*) pktData;
  void                  *responseBuffer;
  UInt32                responseLength;

  if (gfxMsg->pipeHeader.type != kHyperVGraphicsPipeMessageTypeData || gfxMsg->pipeHeader.size < __offsetof (HyperVGraphicsMessage, gfxHeader.size)) {
    HVDBGLOG("Invalid pipe packet receieved (type 0x%X, size %u)", gfxMsg->pipeHeader.type, gfxMsg->pipeHeader.size);
    return;
  }

  HVDBGLOG("Received packet type 0x%X (%u bytes)", gfxMsg->gfxHeader.type, gfxMsg->gfxHeader.size);
  switch (gfxMsg->gfxHeader.type) {
    case kHyperVGraphicsMessageTypeVersionResponse:
    case kHyperVGraphicsMessageTypeVRAMAck:
    case kHyperVGraphicsMessageTypeResolutionUpdateAck:
      if (_hvDevice->getPendingTransaction(kHyperVGraphicsRequestTransactionBaseID + gfxMsg->gfxHeader.type,
                                           &responseBuffer, &responseLength)) {
        memcpy(responseBuffer, pktData, responseLength);
        _hvDevice->wakeTransaction(kHyperVGraphicsRequestTransactionBaseID + gfxMsg->gfxHeader.type);
      }
      break;
      
    case kHyperVGraphicsMessageTypeFeatureChange:
      HVDBGLOG("Got feature change res %u dirt %u ", gfxMsg->featureUpdate.isResolutionUpdateNeeded, gfxMsg->featureUpdate.isDIRTNeeded);
      break;

    default:
      break;
  }
}

IOReturn HyperVGraphics::sendGraphicsMessage(HyperVGraphicsMessage *gfxMessage, HyperVGraphicsMessageType responseType,
                                             HyperVGraphicsMessage *gfxMessageResponse) {
  gfxMessage->pipeHeader.type = kHyperVGraphicsPipeMessageTypeData;
  gfxMessage->pipeHeader.size = gfxMessage->gfxHeader.size;

  return _hvDevice->writeInbandPacketWithTransactionId(gfxMessage, gfxMessage->gfxHeader.size + sizeof (gfxMessage->pipeHeader),
                                                       kHyperVGraphicsRequestTransactionBaseID + responseType,
                                                       gfxMessageResponse != nullptr, gfxMessageResponse,
                                                       gfxMessageResponse != nullptr ? sizeof(*gfxMessageResponse) : 0);
}

IOReturn HyperVGraphics::negotiateVersion(VMBusVersion version) {
  IOReturn status;
  HyperVGraphicsMessage gfxMsg = { };

  gfxMsg.gfxHeader.type = kHyperVGraphicsMessageTypeVersionRequest;
  gfxMsg.gfxHeader.size = sizeof (gfxMsg.gfxHeader) + sizeof (gfxMsg.versionRequest);
  gfxMsg.versionRequest.version = version;

  HVDBGLOG("Trying version %u.%u", version.major, version.minor);
  status = sendGraphicsMessage(&gfxMsg, kHyperVGraphicsMessageTypeVersionResponse, &gfxMsg);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to send negotiate version with status 0x%X", status);
    return status;
  }

  HVDBGLOG("Version %u.%u accepted: 0x%X (actual version %u.%u) max video outputs: %u", version.major, version.minor,
           gfxMsg.versionResponse.accepted, gfxMsg.versionResponse.version.major,
           gfxMsg.versionResponse.version.minor, gfxMsg.versionResponse.maxVideoOutputs);
  return gfxMsg.versionResponse.accepted != 0 ? kIOReturnSuccess : kIOReturnUnsupported;
}

IOReturn HyperVGraphics::connectGraphics() {
  bool foundVersion = false;
  IOReturn status;

  //
  // Negotiate graphics system version.
  //
  for (UInt32 i = 0; i < arrsize(graphicsVersions); i++) {
    status = negotiateVersion(graphicsVersions[i]);
    if (status == kIOReturnSuccess) {
      foundVersion = true;
      _currentGraphicsVersion = graphicsVersions[i];
      break;
    }
  }

  if (!foundVersion) {
    HVSYSLOG("Could not negotiate graphics version");
    return kIOReturnUnsupported;
  }
  HVDBGLOG("Using graphics version %u.%u", _currentGraphicsVersion.major, _currentGraphicsVersion.minor);

  //
  // Allocate graphics memory.
  //
  status = allocateGraphicsMemory();
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to allocate graphics memory with status 0x%X", status);
    return status;
  }

  //
  // Send location to Hyper-V.
  //
  status = updateGraphicsMemoryLocation();
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to send graphics memory location with status 0x%X", status);
    return status;
  }
  
 // updatePointer();
 // updateScreenResolution(1024, 768, false);

  return kIOReturnSuccess;
}

IOReturn HyperVGraphics::allocateGraphicsMemory() {
  PE_Video consoleInfo;
  OSNumber *mmioBytesNumber;

  //
  // Get HyperVPCIRoot instance used for allocating MMIO regions for Hyper-V Gen1 VMs.
  //
  HyperVPCIRoot *hvPCIRoot = HyperVPCIRoot::getPCIRootInstance();
  if (hvPCIRoot == nullptr) {
    return kIOReturnNoResources;
  }

  //
  // Pull console info. We'll use the base address but the length will be gathered from Hyper-V.
  //
  if (getPlatform()->getConsoleInfo(&consoleInfo) != kIOReturnSuccess) {
    HVSYSLOG("Failed to get console info");
    return kIOReturnIOError;
  }
  HVDBGLOG("Console is at 0x%X (%ux%u, bpp: %u, bytes/row: %u)",
           consoleInfo.v_baseAddr, consoleInfo.v_width, consoleInfo.v_height, consoleInfo.v_depth, consoleInfo.v_rowBytes);
  _fbBaseAddress = consoleInfo.v_baseAddr;

  //
  // Get MMIO bytes.
  //
  mmioBytesNumber = OSDynamicCast(OSNumber, _hvDevice->getProperty(kHyperVVMBusDeviceChannelMMIOByteCount));
  if (mmioBytesNumber == nullptr) {
    HVSYSLOG("Failed to get MMIO byte count");
    return kIOReturnNoResources;
  }
  _fbTotalLength = mmioBytesNumber->unsigned64BitValue();
  HVDBGLOG("Framebuffer MMIO size: %p bytes", _fbTotalLength);
  _fbInitialLength = consoleInfo.v_height * consoleInfo.v_rowBytes;

  
  _gfxMmioBase = 0xFFB00000; // TODO: Fix for Gen2

  return kIOReturnSuccess;
}

IOReturn HyperVGraphics::updateGraphicsMemoryLocation() {
  IOReturn status;
  HyperVGraphicsMessage gfxMsg = { };

  //
  // Send location of graphics memory (VRAM).
  //
  HVDBGLOG("Graphics memory located at %p (length %p)", _gfxMmioBase, _gfxMmioLength);
  gfxMsg.gfxHeader.type = kHyperVGraphicsMessageTypeVRAMLocation;
  gfxMsg.gfxHeader.size = sizeof (gfxMsg.gfxHeader) + sizeof (gfxMsg.vramLocation);
  gfxMsg.vramLocation.context = gfxMsg.vramLocation.vramGPA = _gfxMmioBase;
  gfxMsg.vramLocation.isVRAMGPASpecified = 1;

  status = sendGraphicsMessage(&gfxMsg, kHyperVGraphicsMessageTypeVRAMAck, &gfxMsg);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to send graphics memory location with status 0x%X", status);
    return status;
  }
  if (gfxMsg.vramAck.context != _gfxMmioBase) {
    HVSYSLOG("Returned context 0x%llX is incorrect, should be %p", gfxMsg.vramAck.context, _gfxMmioBase);
    return kIOReturnIOError;
  }

  HVDBGLOG("Updated graphics memory location successfully");
  return kIOReturnSuccess;
}

IOReturn HyperVGraphics::updatePointer() {
  IOReturn              status;
  HyperVGraphicsMessage gfxMsg = { };
  
  gfxMsg.gfxHeader.type = kHyperVGraphicsMessageTypePointerPosition;
  gfxMsg.gfxHeader.size = sizeof (gfxMsg.gfxHeader) + sizeof (gfxMsg.pointerPosition);
  
  gfxMsg.pointerPosition.isVisible = 1;
  gfxMsg.pointerPosition.videoOutput = 0;
  gfxMsg.pointerPosition.x = 0;
  gfxMsg.pointerPosition.y = 0;
  
  status = sendGraphicsMessage(&gfxMsg);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to send pointer position with status 0x%X", status);
    return status;
  }
  
  bzero(&gfxMsg, sizeof (gfxMsg));
  gfxMsg.gfxHeader.type = kHyperVGraphicsMessageTypePointerShape;
  gfxMsg.gfxHeader.size = sizeof (gfxMsg.gfxHeader) + sizeof (gfxMsg.pointerShape);
  
  gfxMsg.pointerShape.partIndex = -1;
  gfxMsg.pointerShape.isARGB = 1;
  gfxMsg.pointerShape.width = 1;
  gfxMsg.pointerShape.height = 1;
  gfxMsg.pointerShape.hotX = 0;
  gfxMsg.pointerShape.hotY = 0;
  gfxMsg.pointerShape.data[0] = 0;
  gfxMsg.pointerShape.data[1] = 1;
  gfxMsg.pointerShape.data[2] = 1;
  gfxMsg.pointerShape.data[3] = 1;
  
  status = sendGraphicsMessage(&gfxMsg);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to send pointer shape with status 0x%X", status);
    return status;
  }
  
  return kIOReturnSuccess;
}

IOReturn HyperVGraphics::updateScreenResolution(UInt32 width, UInt32 height, bool isBoot) {
  IOReturn              status;
  PE_Video              consoleInfo;
  HyperVGraphicsMessage gfxMsg = { };
  UInt8                 pixelDepth;

  //
  // Check bounds.
  //
  if (width > kHyperVGraphicsMaxWidth || height > kHyperVGraphicsMaxHeight) {
    HVSYSLOG("Invalid screen resolution %ux%u", width, height);
    return kIOReturnBadArgument;
  }

  //
  // Get current pixel depth.
  //
  status = getPlatform()->getConsoleInfo(&consoleInfo);
  if (status != kIOReturnSuccess) {
    return status;
  }
  pixelDepth = consoleInfo.v_depth;
  HVDBGLOG("Desired screen resolution %ux%u (%u bpp)", width, height, pixelDepth);

  //
  // Send screen resolution and pixel depth information.
  //
  gfxMsg.gfxHeader.type = kHyperVGraphicsMessageTypeResolutionUpdate;
  gfxMsg.gfxHeader.size = sizeof (gfxMsg.gfxHeader) + sizeof (gfxMsg.resolutionUpdate);

  gfxMsg.resolutionUpdate.context = 0;
  gfxMsg.resolutionUpdate.videoOutputCount = 1;
  gfxMsg.resolutionUpdate.videoOutputs[0].active = 1;
  gfxMsg.resolutionUpdate.videoOutputs[0].vramOffset = 0;
  gfxMsg.resolutionUpdate.videoOutputs[0].depth = pixelDepth;
  gfxMsg.resolutionUpdate.videoOutputs[0].width = width;
  gfxMsg.resolutionUpdate.videoOutputs[0].height = height;
  gfxMsg.resolutionUpdate.videoOutputs[0].pitch = width * (pixelDepth / 8);

  status = sendGraphicsMessage(&gfxMsg, kHyperVGraphicsMessageTypeResolutionUpdateAck, &gfxMsg);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to send screen resolution with status 0x%X", status);
    return status;
  }

  //
  // If still booting and pre-userspace, update console and boot logo.
  //
  /*if (isBoot) {
    //
    // Send new framebuffer physical address.
    //
    consoleInfo.v_offset   = 0;
    consoleInfo.v_baseAddr = _gfxMmioBase | 1;
    getPlatform()->setConsoleInfo(0, kPEDisableScreen);
    getPlatform()->setConsoleInfo(&consoleInfo, kPEBaseAddressChange);
    
    //
    // Change framebuffer screen resolution.
    //
    getPlatform()->getConsoleInfo(&consoleInfo);
    consoleInfo.v_width = width;
    consoleInfo.v_height = height;
    consoleInfo.v_rowBytes = width * (pixelDepth / 8);
    getPlatform()->setConsoleInfo(&consoleInfo, kPEEnableScreen);
    
    //
    // Redraw boot logo and progress bar.
    //
    drawBootLogo();
    HyperVPlatformProvider::getInstance()->resetProgressBar();
  }*/

  _screenWidth  = width;
  _screenHeight = height;
  HVDBGLOG("Updated screen resolution successfully");
  return kIOReturnSuccess;
}
