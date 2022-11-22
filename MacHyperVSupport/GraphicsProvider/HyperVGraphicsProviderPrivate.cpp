//
//  HyperVGraphicsProviderPrivate.cpp
//  Hyper-V synthetic graphics provider
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#include "HyperVGraphicsProvider.hpp"

static const VMBusVersion graphicsVersions[] = {
  kHyperVGraphicsVersionV3_2,
  kHyperVGraphicsVersionV3_0
};

void HyperVGraphicsProvider::handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength) {
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
      if (_hvDevice->getPendingTransaction(kHyperVGraphicsVersionRequestTransactionID, &responseBuffer, &responseLength)) {
        memcpy(responseBuffer, pktData, responseLength);
        _hvDevice->wakeTransaction(kHyperVGraphicsVersionRequestTransactionID);
      }
      break;

    default:
      break;
  }
}

IOReturn HyperVGraphicsProvider::sendGraphicsMessage(HyperVGraphicsMessage *gfxMessage, HyperVGraphicsMessage *gfxMessageResponse, UInt32 gfxMessageResponseSize) {
  gfxMessage->pipeHeader.type = kHyperVGraphicsPipeMessageTypeData;
  gfxMessage->pipeHeader.size = gfxMessage->gfxHeader.size;

  return _hvDevice->writeInbandPacketWithTransactionId(gfxMessage, gfxMessage->gfxHeader.size + sizeof (gfxMessage->pipeHeader),
                                                       kHyperVGraphicsVersionRequestTransactionID, gfxMessageResponse != nullptr,
                                                       gfxMessageResponse, gfxMessageResponseSize);
}

IOReturn HyperVGraphicsProvider::negotiateVersion(VMBusVersion version) {
  IOReturn status;
  HyperVGraphicsMessage gfxMsg = { };

  gfxMsg.gfxHeader.type = kHyperVGraphicsMessageTypeVersionRequest;
  gfxMsg.gfxHeader.size = sizeof (gfxMsg.gfxHeader) + sizeof (gfxMsg.versionRequest);
  gfxMsg.versionRequest.version = version;

  HVDBGLOG("Trying version %u.%u", version.major, version.minor);
  status = sendGraphicsMessage(&gfxMsg, &gfxMsg, sizeof (gfxMsg));
  if (status != kIOReturnSuccess) {
    HVSYSLOG("Failed to send negotiate version with status 0x%X", status);
    return status;
  }

  HVDBGLOG("Version %u.%u accepted: 0x%X (actual version %u.%u) max video outputs: %u", version.major, version.minor,
           gfxMsg.versionResponse.accepted, gfxMsg.versionResponse.version.major,
           gfxMsg.versionResponse.version.minor, gfxMsg.versionResponse.maxVideoOutputs);
  return gfxMsg.versionResponse.accepted != 0 ? kIOReturnSuccess : kIOReturnUnsupported;
}

IOReturn HyperVGraphicsProvider::connectGraphics() {
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

  return kIOReturnSuccess;
}