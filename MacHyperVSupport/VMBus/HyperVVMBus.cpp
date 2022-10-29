//
//  HyperVVMBus.cpp
//  Hyper-V VMBus controller
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#include "HyperVVMBus.hpp"

#include "HyperVVMBusDevice.hpp"

OSDefineMetaClassAndStructors(HyperVVMBus, super);

inline void
guid_unparse(const uuid_t uu, uuid_string_t out) {
  snprintf(out,
    sizeof(uuid_string_t),
    "%02x%02x%02x%02x-"
    "%02x%02x-"
    "%02x%02x-"
    "%02x%02x-"
    "%02x%02x%02x%02x%02x%02x",
    uu[3], uu[2], uu[1], uu[0],
    uu[5], uu[4],
    uu[7], uu[6],
    uu[8], uu[9],
    uu[10], uu[11], uu[12], uu[13], uu[14], uu[15]);
}

//
// Supported VMBus versions.
//
const UInt32 VMBusVersions[] = {
 // kVMBusVersionWIN10_V4_1,
//  kVMBusVersionWIN10,
//  kVMBusVersionWIN8_1,
 // kVMBusVersionWIN8,
  kVMBusVersionWIN7,
  kVMBusVersionWS2008
};

//
// VMBus message type to struct mappings.
//
const VMBusMessageTypeTableEntry
VMBusMessageTypeTable[kVMBusChannelMessageTypeMax] = {
  { kVMBusChannelMessageTypeInvalid, 0 },
  { kVMBusChannelMessageTypeChannelOffer, sizeof (VMBusChannelMessage) },
  { kVMBusChannelMessageTypeRescindChannelOffer, sizeof (VMBusChannelMessageChannelRescindOffer) },
  { kVMBusChannelMessageTypeRequestChannels, sizeof (VMBusChannelMessage) },
  { kVMBusChannelMessageTypeRequestChannelsDone, sizeof (VMBusChannelMessage) },
  { kVMBusChannelMessageTypeChannelOpen, sizeof (VMBusChannelMessageChannelOpen) },
  { kVMBusChannelMessageTypeChannelOpenResponse, sizeof (VMBusChannelMessageChannelOpenResponse) },
  { kVMBusChannelMessageTypeChannelClose, sizeof (VMBusChannelMessageChannelClose) },
  { kVMBusChannelMessageTypeGPADLHeader, 0 },
  { kVMBusChannelMessageTypeGPADLBody, 0 },
  { kVMBusChannelMessageTypeGPADLCreated, sizeof (VMBusChannelMessageGPADLCreated) },
  { kVMBusChannelMessageTypeGPADLTeardown, sizeof (VMBusChannelMessageGPADLTeardown) },
  { kVMBusChannelMessageTypeGPADLTeardownResponse, sizeof (VMBusChannelMessageGPADLTeardownResponse) },
  { kVMBusChannelMessageTypeChannelFree, sizeof (VMBusChannelMessageChannelFree) },
  { kVMBusChannelMessageTypeConnect, sizeof (VMBusChannelMessageConnect) },
  { kVMBusChannelMessageTypeConnectResponse, sizeof (VMBusChannelMessageConnectResponse) },
  { kVMBusChannelMessageTypeDisconnect, sizeof (VMBusChannelMessage) }
};

bool HyperVVMBus::attach(IOService *provider) {
  HVCheckDebugArgs();
  
  if (!super::attach(provider)) {
    HVSYSLOG("Superclass failed to attach");
    return false;
  }
  
  bool result = false;
  do {
    hvController = OSDynamicCast(HyperVController, provider);
    if (hvController == nullptr) {
      HVSYSLOG("Provider is not HyperVController");
      break;
    }
    
    _cmdGate = IOCommandGate::commandGate(this);
    getWorkLoop()->addEventSource(_cmdGate);
    if (!allocateInterruptEventSources()) {
      HVSYSLOG("Failed to configure VMBus management interrupts");
      break;
    }
    
    if (!allocateVMBusBuffers()) {
      break;
    }

    if (!connectVMBus()) {
      HVSYSLOG("Failed to connect to the VMBus");
      break;
    }
    
    if (!scanVMBus()) {
      HVSYSLOG("Failed to scan the VMBus");
      break;
    }
    
    result = true;
  } while (false);
  
  return result;
}

bool HyperVVMBus::sendVMBusMessage(VMBusChannelMessage *message, VMBusChannelMessageType responseType, VMBusChannelMessage *response) {
  if (responseType != kVMBusChannelMessageTypeInvalid && response == NULL) {
    return false;
  }
  return _cmdGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBus::sendVMBusMessageGated), message, NULL, &responseType, response) == kIOReturnSuccess;
}

bool HyperVVMBus::sendVMBusMessageWithSize(VMBusChannelMessage *message, UInt32 messageSize, VMBusChannelMessageType responseType, VMBusChannelMessage *response) {
  if (responseType != kVMBusChannelMessageTypeInvalid && response == NULL) {
    return false;
  }
  return _cmdGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBus::sendVMBusMessageGated), message, &messageSize, &responseType, response) == kIOReturnSuccess;
}

IOReturn HyperVVMBus::sendVMBusMessageGated(VMBusChannelMessage *message, UInt32 *messageSize, VMBusChannelMessageType *responseType, VMBusChannelMessage *responseMessage) {
  
  UInt32 hvStatus = kHypercallStatusSuccess;
  IOReturn returnStatus = kIOReturnSuccess;
  bool postCompleted = false;
  
  const VMBusMessageTypeTableEntry *msgEntry = &VMBusMessageTypeTable[message->header.type];
  UInt32 size = messageSize != NULL ? *messageSize : msgEntry->size;
  
  //
  // Multiple hypercalls may fail due to lack of resources on the host
  // side, just try again if that happens.
  //
  for (int i = 0; i < kHyperVHypercallRetryCount; i++) {
    HVDBGLOG("Sending message on connection ID %u, type %u, %u bytes", _vmbusMsgConnectionId, msgEntry->type, size);
    hvStatus = hvController->hypercallPostMessage(_vmbusMsgConnectionId, kHyperVMessageTypeChannel, message, (UInt32) size);
    
    switch (hvStatus) {
      case kHypercallStatusSuccess:
        returnStatus = kIOReturnSuccess;
        postCompleted = true;
        break;
        
      case kHypercallStatusInsufficientMemory:
      case kHypercallStatusInsufficientBuffers:
        returnStatus = kIOReturnNoResources;
        break;
        
      default:
        returnStatus = kIOReturnIOError;
        postCompleted = true;
    }
    
    if (postCompleted) {
      break;
    }
    IODelay(10);
  }
  
  if (returnStatus != kIOReturnSuccess) {
    HVSYSLOG("Hypercall message type 0x%X failed with status 0x%X", msgEntry->type, hvStatus);
    return returnStatus;
  }
  
  if (*responseType != kVMBusChannelMessageTypeInvalid) {
    //
    // Wait for response.
    //
    _vmbusWaitForMessageType = *responseType;
    _cmdGate->commandSleep(&_cmdGateEvent);

    HVDBGLOG("Awoken from sleep, message type is %u with size %u", _vmbusWaitMessage.type, _vmbusWaitMessage.size);
    memcpy(responseMessage, _vmbusWaitMessage.data, VMBusMessageTypeTable[*responseType].size);
  }
  
  return kIOReturnSuccess;
}

void HyperVVMBus::processIncomingVMBusMessage(UInt32 cpu) {
  //
  // Sometimes the interrupt will fire for the same message, and by the time this
  // handler is invoked for that second interrupt, the message will be cleared.
  //
  HyperVMessage *vmbusMessage = hvController->getPendingMessage(cpu, kVMBusInterruptMessage);
  if (vmbusMessage->type == kHyperVMessageTypeNone) {
    return;
  }
  
  HVDBGLOG("CPU %u has a message (type %u)", cpu, vmbusMessage->type);
  
  //
  // Check if we are waiting for an incoming VMBus message.
  //
  if (vmbusMessage->type == kHyperVMessageTypeChannel) {
    VMBusChannelMessage *msg = (VMBusChannelMessage*) &vmbusMessage->data[0];
    HVDBGLOG("Incoming VMBus message type %u on CPU %u", msg->header.type, cpu);
    
    if (_vmbusWaitForMessageType != kVMBusChannelMessageTypeInvalid && _vmbusWaitForMessageType == msg->header.type) {
      HVDBGLOG("Woke for response %u", _vmbusWaitForMessageType);
      _vmbusWaitForMessageType = kVMBusChannelMessageTypeInvalid;
      
      //
      // Store message response.
      //
      memcpy(&_vmbusWaitMessage, vmbusMessage, sizeof (_vmbusWaitMessage));
      hvController->sendSynICEOM(cpu);
      
      _cmdGate->commandWakeup(&_cmdGateEvent);
      return;
    }
    
    //
    // Add offered channels to array.
    //
    if (msg->header.type == kVMBusChannelMessageTypeChannelOffer) {
      addVMBusDevice((VMBusChannelMessageChannelOffer*) msg);
    } else if (msg->header.type == kVMBusChannelMessageTypeRescindChannelOffer) {
      removeVMBusDevice((VMBusChannelMessageChannelRescindOffer*) msg);
    } else {
      HVDBGLOG("Unknown message type %u", msg->header.type);
    }
    hvController->sendSynICEOM(cpu);
    
  } else if (vmbusMessage->type == kVMBusConnIdEvent) {
    HVDBGLOG("Incoming VMBus event on CPU %u", cpu);
  }
}

bool HyperVVMBus::connectVMBus() {
  for (int i = 0; i < arrsize(VMBusVersions); i++) {
    _vmbusVersion = VMBusVersions[i];
    if (negotiateVMBus(_vmbusVersion)) {
      HVDBGLOG("Negotiated VMBus version 0x%X with host", _vmbusVersion);
      return true;
    }
  }
  
  _vmbusVersion = 0;
  HVSYSLOG("Unable to negotiate compatible VMBus version with host");
  return false;
}

bool HyperVVMBus::negotiateVMBus(UInt32 version) {
  VMBusChannelMessageConnect connectMsg;
  connectMsg.header.type      = kVMBusChannelMessageTypeConnect;
  connectMsg.targetProcessor  = 0;
  connectMsg.protocolVersion  = version;
  connectMsg.monitorPage1     = _vmbusMnf1.physAddr;
  connectMsg.monitorPage2     = _vmbusMnf2.physAddr;
  
  // Older hosts used connection ID 1 for VMBus, but Windows 10 v5.0 and higher use ID 4.
  // TODO: Seems to have issues getting a response when using ID: 4, which is what Linux uses.
  if (_vmbusVersion >= kVMBusVersionWIN10_V5) {
    _vmbusMsgConnectionId      = kVMBusConnIdMessage4;
    connectMsg.messageInt     = kVMBusInterruptMessage;
  } else {
    _vmbusMsgConnectionId      = kVMBusConnIdMessage1;
    connectMsg.interruptPage  = vmbusEventFlags.physAddr;
  }
  
  // Windows Server 2008 and 2008 R2 use the event flag bits.
  useLegacyEventFlags = (_vmbusVersion == kVMBusVersionWS2008 || _vmbusVersion == kVMBusVersionWIN7);
  if (useLegacyEventFlags) {
    HVDBGLOG("Legacy event flags will be used for messages");
  }
  
  hvController->enableInterrupts(useLegacyEventFlags ? vmbusRxEventFlags : nullptr);
  
  HVDBGLOG("Trying version 0x%X and connection ID %u", connectMsg.protocolVersion, _vmbusMsgConnectionId);
  
  VMBusChannelMessageConnectResponse resp;
  if (!sendVMBusMessage((VMBusChannelMessage*) &connectMsg, kVMBusChannelMessageTypeConnectResponse, (VMBusChannelMessage*) &resp)) {
    hvController->disableInterrupts();
    return false;
  }
  
  HVDBGLOG("Version 0x%X is %s (connection ID %u)",
           connectMsg.protocolVersion, resp.supported ? "supported" : "not supported", resp.messageConnectionId);
  if (resp.supported != 0 && _vmbusVersion >= kVMBusVersionWIN10_V5) {
    // We'll use indicated connection ID for future messages.
    _vmbusMsgConnectionId = resp.messageConnectionId;
  }
  
  return resp.supported != 0;
}

bool HyperVVMBus::scanVMBus() {
  //
  // Initialize children array.
  //
  memset(_vmbusChannels, 0, sizeof (_vmbusChannels));
  _nextGpadlHandle = kHyperVGpadlStartHandle;
  
  //
  // Send request channels message.
  // This begins the process of enumerating children of the VMBus.
  //
  // Once all children are offered, a completion message is sent.
  //
  VMBusChannelMessage chanReqMsg;
  chanReqMsg.header.type = kVMBusChannelMessageTypeRequestChannels;
  
  HVDBGLOG("VMBus scan started");
  VMBusChannelMessage resp;
  bool result = sendVMBusMessage(&chanReqMsg, kVMBusChannelMessageTypeRequestChannelsDone, &resp);
  HVDBGLOG("VMBus scan completed");
  
  return result;
}

bool HyperVVMBus::addVMBusDevice(VMBusChannelMessageChannelOffer *offerMessage) {
  //
  // Add offer message to channel array.
  //
  UInt32 channelId = offerMessage->channelId;
  if (channelId >= kVMBusMaxChannels || _vmbusChannels[channelId].status != kVMBusChannelStatusNotPresent) {
    HVDBGLOG("Channel %u is invalid or already present", channelId);
    return false;
  }
  
  //
  // Copy offer message and create GUID type string.
  //
  memcpy(&_vmbusChannels[channelId].offerMessage, offerMessage, sizeof (VMBusChannelMessageChannelOffer));
  guid_unparse(offerMessage->type, _vmbusChannels[channelId].typeGuidString);
  memcpy(_vmbusChannels[channelId].instanceId, offerMessage->instance, sizeof (offerMessage->instance));
  _vmbusChannels[channelId].status = kVMBusChannelStatusClosed;
  
  if (!registerVMBusDevice(&_vmbusChannels[channelId])) {
    HVDBGLOG("Failed to register channel %u", channelId);
    cleanupVMBusDevice(&_vmbusChannels[channelId]);
    return false;
  }
  
  HVDBGLOG("Registered channel %u (%s)", channelId, _vmbusChannels[channelId].typeGuidString);
  HVDBGLOG("Channel %u flags 0x%X, MIMO size %u bytes, pipe mode 0x%X", channelId,
           _vmbusChannels[channelId].offerMessage.flags, _vmbusChannels[channelId].offerMessage.mmioSizeMegabytes,
           _vmbusChannels[channelId].offerMessage.pipe.mode);
  HVDBGLOG("Channel %u mon id %u, monitor alloc %u, dedicated int %u, conn ID %u", channelId,
           _vmbusChannels[channelId].offerMessage.monitorId, _vmbusChannels[channelId].offerMessage.monitorAllocated,
           _vmbusChannels[channelId].offerMessage.dedicatedInterrupt, _vmbusChannels[channelId].offerMessage.connectionId);

  return true;
}

void HyperVVMBus::removeVMBusDevice(VMBusChannelMessageChannelRescindOffer *rescindOfferMessage) {
  UInt32 channelId = rescindOfferMessage->channelId;
  if (channelId >= kVMBusMaxChannels || _vmbusChannels[channelId].status == kVMBusChannelStatusNotPresent) {
    HVDBGLOG("Channel %u is invalid or is not active", channelId);
    return;
  }
  
  HVDBGLOG("Removing channel %u", channelId);
  
  //
  // Notify nub to terminate.
  //
  if (_vmbusChannels[channelId].deviceNub != NULL) {
    _vmbusChannels[channelId].deviceNub->terminate();
    _vmbusChannels[channelId].deviceNub->release();
    _vmbusChannels[channelId].deviceNub = NULL;
  }
  HVDBGLOG("Channel %u has been asked to terminate", channelId);
}

bool HyperVVMBus::registerVMBusDevice(VMBusChannel *channel) {
  //
  // Allocate and initialize child VMBus device object.
  //
  HyperVVMBusDevice *childDevice = OSTypeAlloc(HyperVVMBusDevice);
  if (childDevice == NULL) {
    return false;
  }
  
  //
  // Create property objects for channel information.
  //
  OSString *devType     = OSString::withCString(channel->typeGuidString);
  OSData   *devInstance = OSData::withBytes(channel->instanceId, sizeof (channel->instanceId));
  OSNumber *channelNumber = OSNumber::withNumber(channel->offerMessage.channelId, 32);
  if (devType == NULL || devInstance == NULL || channelNumber == NULL) {
    OSSafeReleaseNULL(devType);
    OSSafeReleaseNULL(devInstance);
    OSSafeReleaseNULL(channelNumber);
    childDevice->release();
    return false;
  }
  
  //
  // Create dictionary and set properties, releasing them after completion.
  //
  OSDictionary *dict = OSDictionary::withCapacity(5);
  if (dict == NULL) {
    devType->release();
    devInstance->release();
    channelNumber->release();
    childDevice->release();
    return false;
  }
  
  bool result = dict->setObject(kHyperVVMBusDeviceChannelTypeKey, devType) &&
                dict->setObject(kHyperVVMBusDeviceChannelInstanceKey, devInstance) &&
                dict->setObject(kHyperVVMBusDeviceChannelIDKey, channelNumber);
  
  devType->release();
  devInstance->release();
  channelNumber->release();
  
  if (!result) {
    dict->release();
    childDevice->release();
    return false;
  }
  
  if (!hvController->addInterruptProperties(dict, channel->offerMessage.channelId)) {
    dict->release();
    childDevice->release();
    return false;
  }

  //
  // Initialize and attach nub.
  //
  result = childDevice->init(dict) && childDevice->attach(this);
  dict->release();
  
  if (!result) {
    childDevice->release();
    return false;
  }
  
  childDevice->registerService();
  channel->deviceNub = childDevice;

  return true;
}

void HyperVVMBus::cleanupVMBusDevice(VMBusChannel *channel) {
  channel->status = kVMBusChannelStatusNotPresent;
}

void HyperVVMBus::freeVMBusChannel(UInt32 channelId) {
  VMBusChannel *channel = &_vmbusChannels[channelId];
  
  //
  // Free channel ID to be reused later on by Hyper-V.
  //
  VMBusChannelMessageChannelFree freeMsg;
  freeMsg.header.type      = kVMBusChannelMessageTypeChannelFree;
  freeMsg.header.reserved  = 0;
  freeMsg.channelId        = channelId;
  
  bool result = sendVMBusMessage((VMBusChannelMessage*) &freeMsg);
  if (!result) {
    HVSYSLOG("Failed to send channel free message for channel %u", channelId);
  }
  HVDBGLOG("Channel %u is now freed", channelId);
  channel->status = kVMBusChannelStatusNotPresent;
}
