//
//  HyperVGraphicsPrivate.cpp
//  Hyper-V synthetic graphics driver
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "HyperVGraphics.hpp"
#include "HyperVPCIRoot.hpp"
#include "HyperVPlatformProvider.hpp"

void HyperVGraphics::handleRefreshTimer(IOTimerEventSource *sender) {
  if (_fbReady) {
    updateFramebufferImage();
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
      //
      // Refresh display states on a feature change.
      //
      HVDBGLOG("Got feature change: img %u pos %u shape %u res %u", gfxMsg->featureUpdate.isImageUpdateNeeded, gfxMsg->featureUpdate.isCursorPositionNeeded,
               gfxMsg->featureUpdate.isCursorShapeNeeded, gfxMsg->featureUpdate.isResolutionUpdateNeeded);
      if (gfxMsg->featureUpdate.isResolutionUpdateNeeded) {
        updateScreenResolution(0, 0, true);
      }
      if (gfxMsg->featureUpdate.isImageUpdateNeeded) {
        updateFramebufferImage();
      }
      if (gfxMsg->featureUpdate.isCursorShapeNeeded) {
        updateCursorShape(nullptr, 0, 0, 0, 0, true);
      }
      if (gfxMsg->featureUpdate.isCursorPositionNeeded) {
        updateCursorPosition(0, 0, false, true);
      }
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
  PE_Video consoleInfo = { };
  bool foundVersion = false;
  IOReturn status;

  //
  // Negotiate graphics system version.
  //
  VMBusVersion graphicsVersion;
  switch (_hvDevice->getVMBusVersion()) {
    case kVMBusVersionWIN8:
    case kVMBusVersionWIN8_1:
    case kVMBusVersionWIN10:
    case kVMBusVersionWIN10_V4_1: // TODO: Check if this is correct.
      graphicsVersion.value = kHyperVGraphicsVersionV3_2;
      break;

    case kVMBusVersionWIN7:
    case kVMBusVersionWS2008:
      graphicsVersion.value = kHyperVGraphicsVersionV3_0;
      break;

    default:
      graphicsVersion.value = kHyperVGraphicsVersionV3_5;
      break;
  }

  status = negotiateVersion(graphicsVersion);
  if (status == kIOReturnSuccess) {
    foundVersion = true;
    _gfxVersion = graphicsVersion;
  }

  if (!foundVersion) {
    HVSYSLOG("Could not negotiate graphics version");
    return kIOReturnUnsupported;
  }
  HVDBGLOG("Using graphics version %u.%u", _gfxVersion.major, _gfxVersion.minor);

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

  //
  // Pull console info and set resolution.
  //
  if (getPlatform()->getConsoleInfo(&consoleInfo) != kIOReturnSuccess) {
    HVSYSLOG("Failed to get console info");
    return false;
  }
  HVDBGLOG("Console is at 0x%X (%ux%u, bpp: %u, bytes/row: %u)",
         consoleInfo.v_baseAddr, consoleInfo.v_width, consoleInfo.v_height, consoleInfo.v_depth, consoleInfo.v_rowBytes);
  status = updateScreenResolution(static_cast<UInt32>(consoleInfo.v_width), static_cast<UInt32>(consoleInfo.v_height), false);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to update screen resolution with status 0x%X", status);
    return status;
  }

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
  _fbTotalLength = _gfxLength = static_cast<UInt32>(mmioBytesNumber->unsigned64BitValue());
  HVDBGLOG("Framebuffer MMIO size: %p bytes", _gfxLength);
  _fbInitialLength = consoleInfo.v_height * consoleInfo.v_rowBytes;

  
  _gfxBase = 0xF8000000;//0xFFB00000; // TODO: Fix for Gen2

  return kIOReturnSuccess;
}

IOReturn HyperVGraphics::updateFramebufferImage() {
  HyperVGraphicsMessage gfxMsg = { };
  IOReturn              status;

  //
  // Send screen image update to Hyper-V.
  //
  gfxMsg.gfxHeader.type = kHyperVGraphicsMessageTypeImageUpdate;
  gfxMsg.gfxHeader.size = sizeof (gfxMsg.gfxHeader) + sizeof (gfxMsg.imageUpdate);

  gfxMsg.imageUpdate.videoOutput = 0;
  gfxMsg.imageUpdate.count       = 1;
  gfxMsg.imageUpdate.rects[0].x1 = 0;
  gfxMsg.imageUpdate.rects[0].y1 = 0;
  gfxMsg.imageUpdate.rects[0].x2 = _screenWidth;
  gfxMsg.imageUpdate.rects[0].y2 = _screenHeight;

  status = sendGraphicsMessage(&gfxMsg);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to send image update with status 0x%X", status);
  }
  return status;
}

IOReturn HyperVGraphics::updateGraphicsMemoryLocation() {
  IOReturn status;
  HyperVGraphicsMessage gfxMsg = { };

  //
  // Send location of graphics memory (VRAM)._gfxMmioLength
  //
  HVDBGLOG("Graphics memory located at %p length 0x%X", _gfxBase, _gfxLength);
  gfxMsg.gfxHeader.type = kHyperVGraphicsMessageTypeVRAMLocation;
  gfxMsg.gfxHeader.size = sizeof (gfxMsg.gfxHeader) + sizeof (gfxMsg.vramLocation);
  gfxMsg.vramLocation.context = gfxMsg.vramLocation.vramGPA = _gfxBase;
  gfxMsg.vramLocation.isVRAMGPASpecified = 1;

  status = sendGraphicsMessage(&gfxMsg, kHyperVGraphicsMessageTypeVRAMAck, &gfxMsg);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to send graphics memory location with status 0x%X", status);
    return status;
  }
  if (gfxMsg.vramAck.context != _gfxBase) {
    HVSYSLOG("Returned context 0x%llX is incorrect, should be %p", gfxMsg.vramAck.context, _gfxBase);
    return kIOReturnIOError;
  }

  HVDBGLOG("Updated graphics memory location successfully");
  return kIOReturnSuccess;
}

IOReturn HyperVGraphics::updateCursorShape(const UInt8 *cursorData, UInt32 width, UInt32 height, UInt32 hotX, UInt32 hotY, bool featureChange) {
  IOReturn              status;
  UInt32                cursorSize = 0;

  if (!featureChange) {
    if (cursorData != nullptr) {
      //
      // Check that cursor is valid.
      //
      if ((width > kHyperVGraphicsCursorMaxWidth) || (height > kHyperVGraphicsCursorMaxHeight)
          || (hotX > width) || (hotY > height)) {
        HVSYSLOG("Invalid cursor image passed");
        return kIOReturnUnsupported;
      }
      cursorSize = (width * height * kHyperVGraphicsCursorARGBPixelSize);
      if (cursorSize > kHyperVGraphicsCursorMaxSize) {
        HVSYSLOG("Invalid cursor image passed");
        return kIOReturnUnsupported;
      }
      HVDBGLOG("Cursor data at %p size %ux%u hot %ux%u length %u bytes", cursorData, width, height, hotX, hotY, cursorSize);
    } else {
      cursorSize = (1 * 1 * kHyperVGraphicsCursorARGBPixelSize);
      width = 1;
      height = 1;
      hotX = 0;
      hotY = 0;
      HVDBGLOG("No cursor data passed, setting to no cursor");
    }
  }

  IOSimpleLockLock(_gfxMsgCursorShapeLock);

  //
  // Skip cursor update if we have yet to update the cursor.
  //
  if (featureChange && (_gfxMsgCursorShape == nullptr)) {
    IOSimpleLockUnlock(_gfxMsgCursorShapeLock);
    return kIOReturnUnsupported;
  }

  //
  // Allocate message if not already allocated.
  //
  if (_gfxMsgCursorShape == nullptr) {
    _gfxMsgCursorShapeSize = sizeof (*_gfxMsgCursorShape) + kHyperVGraphicsCursorMaxSize;
    _gfxMsgCursorShape = static_cast<HyperVGraphicsMessage*>(IOMalloc(_gfxMsgCursorShapeSize));
    if (_gfxMsgCursorShape == nullptr) {
      IOSimpleLockUnlock(_gfxMsgCursorShapeLock);
      HVSYSLOG("Failed to allocate cursor graphics message");
      return kIOReturnNoResources;
    }
  }

  //
  // If a feature change was triggered, just resend the last sent packet.
  //
  if (!featureChange) {
    //
    // Send cursor image.
    // Cursor format is ARGB if alpha is enabled, RGB otherwise.
    //
    bzero(_gfxMsgCursorShape, _gfxMsgCursorShapeSize);
    _gfxMsgCursorShape->gfxHeader.type = kHyperVGraphicsMessageTypeCursorShape;
    _gfxMsgCursorShape->gfxHeader.size = sizeof (_gfxMsgCursorShape->gfxHeader) + sizeof (_gfxMsgCursorShape->cursorShape) + cursorSize;

    _gfxMsgCursorShape->cursorShape.partIndex = kHyperVGraphicsCursorPartIndexComplete;
    _gfxMsgCursorShape->cursorShape.isARGB    = 1;
    _gfxMsgCursorShape->cursorShape.width     = width;
    _gfxMsgCursorShape->cursorShape.height    = height;
    _gfxMsgCursorShape->cursorShape.hotX      = hotX;
    _gfxMsgCursorShape->cursorShape.hotY      = hotY;

    if (cursorData != nullptr) {
      //
      // Copy cursor data.
      // macOS provides cursor image inverted heightwise, flip here during the copy.
      //
      UInt32 stride = width * kHyperVGraphicsCursorARGBPixelSize;
      for (UInt32 dstY = 0, srcY = (height - 1); dstY < height; dstY++, srcY--) {
        memcpy(&_gfxMsgCursorShape->cursorShape.data[dstY * stride], &cursorData[srcY * stride], stride);
      }
    } else {
      //
      // For no cursor use 1x1 transparent square.
      //
      _gfxMsgCursorShape->cursorShape.data[0] = 0;
      _gfxMsgCursorShape->cursorShape.data[1] = 1;
      _gfxMsgCursorShape->cursorShape.data[2] = 1;
      _gfxMsgCursorShape->cursorShape.data[3] = 1;
    }
  }

  status = sendGraphicsMessage(_gfxMsgCursorShape);
  if (status != kIOReturnSuccess) {
    IOSimpleLockUnlock(_gfxMsgCursorShapeLock);
    HVSYSLOG("Failed to send cursor shape with status 0x%X", status);
    return status;
  }

  IOSimpleLockUnlock(_gfxMsgCursorShapeLock);
  HVDBGLOG("Updated cursor data successfully");
  return kIOReturnSuccess;
}

IOReturn HyperVGraphics::updateCursorPosition(SInt32 x, SInt32 y, bool isVisible, bool featureChange) {
  HyperVGraphicsMessage gfxMsg = { };
  IOReturn              status;

  static bool cursorPositionSent = false;
  static SInt32 lastX         = 0;
  static SInt32 lastY         = 0;
  static bool   lastIsVisible = true;

  //
  // If a cursor position was never sent, do not send on a feature change.
  //
  if (featureChange && cursorPositionSent) {
    return kIOReturnUnsupported;
  }

  //
  // If a feature change was triggered, just resend the last sent packet.
  //
  if (!featureChange) {
    lastX = x;
    lastY = y;
    lastIsVisible = isVisible;
  }

  //
  // Send cursor position and visibility.
  //
  gfxMsg.gfxHeader.type = kHyperVGraphicsMessageTypeCursorPosition;
  gfxMsg.gfxHeader.size = sizeof (gfxMsg.gfxHeader) + sizeof (gfxMsg.cursorPosition);

  gfxMsg.cursorPosition.isVisible = lastIsVisible;
  gfxMsg.cursorPosition.videoOutput = 0;
  gfxMsg.cursorPosition.x = lastX;
  gfxMsg.cursorPosition.y = lastY;

  status = sendGraphicsMessage(&gfxMsg);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to send cursor position with status 0x%X", status);
  } else {
    cursorPositionSent = true;
  }
  return status;
}

IOReturn HyperVGraphics::updateScreenResolution(UInt32 width, UInt32 height, bool featureChange) {
  IOReturn              status;
  PE_Video              consoleInfo;
  HyperVGraphicsMessage gfxMsg = { };

  //
  // If a feature change was triggered, just resend the last sent packet.
  //
  if (featureChange) {
    width  = _screenWidth;
    height = _screenHeight;
  }

  //
  // Check bounds.
  //
  if (_gfxVersion.value == kHyperVGraphicsVersionV3_0) {
    if ((width > kHyperVGraphicsMaxWidth2008) || (height > kHyperVGraphicsMaxHeight2008)) {
      HVSYSLOG("Invalid screen resolution %ux%u", width, height);
      return kIOReturnBadArgument;
    }
  }
  if ((width * height * (_bitDepth / 8)) > _gfxLength) {
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
  HVDBGLOG("Desired screen resolution %ux%u (%u bpp)", width, height, _bitDepth);

  //
  // Send screen resolution and pixel depth information.
  //
  gfxMsg.gfxHeader.type = kHyperVGraphicsMessageTypeResolutionUpdate;
  gfxMsg.gfxHeader.size = sizeof (gfxMsg.gfxHeader) + sizeof (gfxMsg.resolutionUpdate);

  gfxMsg.resolutionUpdate.context = 0;
  gfxMsg.resolutionUpdate.videoOutputCount = 1;
  gfxMsg.resolutionUpdate.videoOutputs[0].active = 1;
  gfxMsg.resolutionUpdate.videoOutputs[0].vramOffset = 0;
  gfxMsg.resolutionUpdate.videoOutputs[0].depth = _bitDepth;
  gfxMsg.resolutionUpdate.videoOutputs[0].width = width;
  gfxMsg.resolutionUpdate.videoOutputs[0].height = height;
  gfxMsg.resolutionUpdate.videoOutputs[0].pitch = width * (_bitDepth / 8);

  status = sendGraphicsMessage(&gfxMsg, kHyperVGraphicsMessageTypeResolutionUpdateAck, &gfxMsg);
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to send screen resolution with status 0x%X", status);
    return status;
  }

  _screenWidth  = width;
  _screenHeight = height;
  HVDBGLOG("Updated screen resolution successfully");
  return kIOReturnSuccess;
}
