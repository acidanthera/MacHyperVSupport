#include "HyperVVMBusController.hpp"
#include "HyperVVMBusInternal.hpp"

const VMBusMessageTypeTableEntry
VMBusMessageTypeTable[kVMBusChannelMessageTypeMax] = {
  { kVMBusChannelMessageTypeInvalid, 0 },
  { kVMBusChannelMessageTypeChannelOffer, sizeof (VMBusChannelMessage) },
  { kVMBusChannelMessageTypeRescindChannelOffer, sizeof (VMBusChannelMessageChannelRescindOffer) },
  { kVMBusChannelMessageTypeRequestChannels, sizeof (VMBusChannelMessage) },
  { kVMBusChannelMessageTypeRequestChannelsDone, sizeof(VMBusChannelMessage) },
  { kVMBusChannelMessageTypeChannelOpen, sizeof (VMBusChannelMessageChannelOpen) },
  { kVMBusChannelMessageTypeChannelOpenResponse, sizeof (VMBusChannelMessageChannelOpenResponse) },
  { kVMBusChannelMessageTypeChannelClose, sizeof (VMBusChannelMessageChannelClose) },
  { kVMBusChannelMessageTypeGPADLHeader, 0 },
  { kVMBusChannelMessageTypeGPADLBody, 0 },
  { kVMBusChannelMessageTypeGPADLCreated, sizeof (VMBusChannelMessageGPADLCreated) },
  { kVMBusChannelMessageTypeGPADLTeardown, sizeof (VMBusChannelMessageGPADLTeardown) },
  { kVMBusChannelMessageTypeGPADLTeardownResponse, sizeof (VMBusChannelMessageGPADLTeardownResponse) },
  { kVMBusChannelMessageTypeChannelFree, 0 },
  { kVMBusChannelMessageTypeConnect, sizeof (VMBusChannelMessageConnect) },
  { kVMBusChannelMessageTypeConnectResponse, sizeof (VMBusChannelMessageConnectResponse) },
  { kVMBusChannelMessageTypeDisconnect, sizeof (VMBusChannelMessage) }
};

bool HyperVVMBusController::allocateVMBusBuffers() {
  
  
  
  //
  // Allocate common VMBus structures.
  //
  allocateDmaBuffer(&vmbusEventFlags, PAGE_SIZE);
  allocateDmaBuffer(&vmbusMnf1, PAGE_SIZE);
  allocateDmaBuffer(&vmbusMnf2, PAGE_SIZE);
  
  allocateDmaBuffer(&vmbusMsgBuffer, PAGE_SIZE);
  
  vmbusRxEventFlags = (UInt8*)vmbusEventFlags.buffer;
  vmbusTxEventFlags = (UInt8*)vmbusEventFlags.buffer + PAGE_SIZE / 2;
  
  return true;
}

bool HyperVVMBusController::sendVMBusMessage(VMBusChannelMessage *message, VMBusChannelMessageType responseType, VMBusChannelMessage *response) {
  if (responseType != kVMBusChannelMessageTypeInvalid && response == NULL) {
    return false;
  }
  return cmdGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBusController::sendVMBusMessageGated), message, NULL, &responseType, response) == kIOReturnSuccess;
}

bool HyperVVMBusController::sendVMBusMessageWithSize(VMBusChannelMessage *message, UInt32 messageSize, VMBusChannelMessageType responseType, VMBusChannelMessage *response) {
  if (responseType != kVMBusChannelMessageTypeInvalid && response == NULL) {
    return false;
  }
  return cmdGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &HyperVVMBusController::sendVMBusMessageGated), message, &messageSize, &responseType, response) == kIOReturnSuccess;
}

IOReturn HyperVVMBusController::sendVMBusMessageGated(VMBusChannelMessage *message, UInt32 *messageSize, VMBusChannelMessageType *responseType, VMBusChannelMessage *responseMessage) {
  const VMBusMessageTypeTableEntry *msgEntry = &VMBusMessageTypeTable[message->header.type];

  UInt32 size = messageSize != NULL ? *messageSize : msgEntry->size;
  DBGLOG("Preparing to send message of type %u and %u bytes", msgEntry->type, size);
  
  //
  // Hypercall message data must be page-aligned.
  //
  HyperVHypercallPostMessage *hypercallMsg = (HyperVHypercallPostMessage*)vmbusMsgBuffer.buffer;
  memset(hypercallMsg, 0, sizeof (*hypercallMsg));
  
  hypercallMsg->connectionId  = kVMBusConnIdMessage;
  hypercallMsg->messageType   = kHyperVMessageTypeChannel;
  hypercallMsg->size          = (UInt32) size;
  memcpy(&hypercallMsg->data[0], message, size);
  
  DBGLOG("Sending message of %u bytes", size);
  bool result = hypercallPostMessage(vmbusMsgBuffer.physAddr) == kHyperVStatusSuccess;
  if (!result) {
    return kIOReturnIOError;
  }
  
  if (*responseType != kVMBusChannelMessageTypeInvalid) {
    //
    // Wait for response.
    //
    vmbusWaitForMessageType = *responseType;
    cmdGate->commandSleep(&cmdGateEvent);
    
    HyperVMessage *newMsg = &cpuData.perCPUData[vmbusWaitMessageCpu].messages[kVMBusInterruptMessage];
    DBGLOG("Awoken from sleep, message type is %u with size %u", newMsg->type, newMsg->size);
   /* if (newMsg->size != VMBusMessageTypeTable[*responseType].size) {
      DBGLOG("Response size mismatch!");
      return kIOReturnIOError;
    }*/
    
    memcpy(responseMessage, &newMsg->data[0], VMBusMessageTypeTable[*responseType].size);
    newMsg->type = kHyperVMessageTypeNone;
    sendSynICEOM(vmbusWaitMessageCpu);
  }
  
  return kIOReturnSuccess;
}



void HyperVVMBusController::completeVMBusMessage(UInt32 cpu) {
  cpuData.perCPUData[cpu].messages[kVMBusInterruptMessage].type = kHyperVMessageTypeNone;
}

void HyperVVMBusController::processIncomingVMBusMessage(UInt32 cpu) {
  DBGLOG("CPU %u has a message", cpu);
  
  //
  // Check if we are waiting for an incoming VMBus message.
  //
  if (cpuData.perCPUData[cpu].messages[kVMBusInterruptMessage].type == kVMBusConnIdMessage) {
    VMBusChannelMessage *msg = (VMBusChannelMessage*) &cpuData.perCPUData[cpu].messages[kVMBusInterruptMessage].data[0];
    DBGLOG("Incoming VMBus message type %u on CPU %u", msg->header.type, cpu);
    
    if (vmbusWaitForMessageType != kVMBusChannelMessageTypeInvalid && vmbusWaitForMessageType == msg->header.type) {
   //   cmdGate->commandWakeup(&command_complete);
      DBGLOG("Woke for response %u", vmbusWaitForMessageType);
      vmbusWaitMessageCpu     = cpu;
      vmbusWaitForMessageType = kVMBusChannelMessageTypeInvalid;
      cmdGate->commandWakeup(&cmdGateEvent);
      return;
    }
    
    //
    // Add offered channels to array.
    //
    if (msg->header.type == kVMBusChannelMessageTypeChannelOffer) {
      addVMBusChannelInfo((VMBusChannelMessageChannelOffer*) msg);
    } else if (msg->header.type == kVMBusChannelMessageTypeRescindChannelOffer) {
      VMBusChannelMessageChannelRescindOffer *rOffer = (VMBusChannelMessageChannelRescindOffer*)msg;
      DBGLOG("Channel %u offer is now taken back!", rOffer->channelId);
    }
    
    completeVMBusMessage(cpu);
    sendSynICEOM(cpu);
  }
}

bool HyperVVMBusController::connectVMBus() {
  VMBusChannelMessageConnect connectMsg;
  connectMsg.header.type = kVMBusChannelMessageTypeConnect; //VMBUS_CHANMSG_TYPE_CONNECT;
  connectMsg.targetProcessor = 0;
  connectMsg.protocolVersion = kVMBusVersionWIN8_1; //((4 << 16) | (0)); // ((5 << 16) | (0));//((4 << 16) | (0));
  connectMsg.interruptPage = vmbusEventFlags.physAddr;
  connectMsg.monitorPage1 = vmbusMnf1.physAddr;
  connectMsg.monitorPage2 = vmbusMnf2.physAddr;
  DBGLOG("Version %X", connectMsg.protocolVersion);
  
  VMBusChannelMessageConnectResponse resp;
  sendVMBusMessage((VMBusChannelMessage*) &connectMsg, kVMBusChannelMessageTypeConnectResponse, (VMBusChannelMessage*) &resp);
  
  DBGLOG("Message here");
  
  DBGLOG("header %X %X %X", cpuData.perCPUData[0].messages[kVMBusInterruptMessage].type, resp.header.type, resp.supported);

  return true;
}

bool HyperVVMBusController::scanVMBus() {
  //
  // Initialize children array.
  //
  memset(vmbusChannels, 0, sizeof (vmbusChannels));
  nextGpadlHandle = kHyperVGpadlStartHandle;
  nextGpadlHandleLock = IOLockAlloc();
  vmbusChannelHighest = 0;
  
  //
  // Send request channels message.
  // This begins the process of enumerating children of the VMBus.
  //
  // Once all children are offered, a completion message is sent.
  //
  VMBusChannelMessage chanReqMsg;
  chanReqMsg.header.type = kVMBusChannelMessageTypeRequestChannels;
  
  DBGLOG("VMBus scan started");
  VMBusChannelMessage resp;
  bool result = sendVMBusMessage(&chanReqMsg, kVMBusChannelMessageTypeRequestChannelsDone, &resp);
  DBGLOG("VMBus scan completed");
  
  //
  // Initialize the children.
  //
  for (int i = 0; i <= vmbusChannelHighest; i++) {
    if (vmbusChannels[i].status == kVMBusChannelStatusNotPresent) {
      continue;
    }
    
    if (!registerVMBusDevice(&vmbusChannels[i])) {
      DBGLOG("Failed to register channel %u", i);
      return false; // TODO: cleanup
    }
    
    DBGLOG("Registered channel %u (%s)", i, vmbusChannels[i].typeGuidString);
    DBGLOG("Channel %u flags 0x%X, MIMO size %u bytes, pipe mode 0x%X", i,
           vmbusChannels[i].offerMessage.flags, vmbusChannels[i].offerMessage.mmioSizeMegabytes,
           vmbusChannels[i].offerMessage.pipe.mode);
    DBGLOG("Channel %u mon id %u, monitor alloc %u, dedicated int %u, conn ID %u", i,
           vmbusChannels[i].offerMessage.monitorId, vmbusChannels[i].offerMessage.monitorAllocated,
           vmbusChannels[i].offerMessage.dedicatedInterrupt, vmbusChannels[i].offerMessage.connectionId);
  }
  
  return result;
}

bool HyperVVMBusController::addVMBusChannelInfo(VMBusChannelMessageChannelOffer *offerMessage) {
  //
  // Add offer message to channel array.
  //
  if (offerMessage->channelId >= kHyperVMaxChannels || vmbusChannels[offerMessage->channelId].status != kVMBusChannelStatusNotPresent) {
    return false;
  }
  
  if (vmbusChannelHighest < offerMessage->channelId) {
    vmbusChannelHighest = offerMessage->channelId;
  }
  
  //
  // Copy offer message and create GUID type string.
  //
  memcpy(&vmbusChannels[offerMessage->channelId].offerMessage, offerMessage, sizeof (VMBusChannelMessageChannelOffer));
  guid_unparse(offerMessage->type, vmbusChannels[offerMessage->channelId].typeGuidString);
  vmbusChannels[offerMessage->channelId].status = kVMBusChannelStatusClosed;

  return true;
}

OSDictionary* HyperVVMBusController::getVMBusDevicePropertyDictionary(VMBusChannel *channel) {
  //
  // Create property objects for channel information.
  //
  OSString *devType     = OSString::withCString(channel->typeGuidString);
  OSNumber *channelNumber = OSNumber::withNumber(channel->offerMessage.channelId, 32);
  if (devType == NULL || channelNumber == NULL) {
    OSSafeReleaseNULL(devType);
    OSSafeReleaseNULL(channelNumber);
    return NULL;
  }
  
  //
  // Create interrupt specifier dictionaries.
  // These are used to reference us for various interrupt methods on IOService.
  //
  OSArray *interruptControllers = OSArray::withCapacity(1);
  OSArray *interruptSpecifiers = OSArray::withCapacity(1);
  if (interruptControllers == NULL || interruptSpecifiers == NULL) {
    OSSafeReleaseNULL(interruptControllers);
    OSSafeReleaseNULL(interruptSpecifiers);
    devType->release();
    channelNumber->release();
    return NULL;
  }
  interruptControllers->setObject(interruptControllerName);
  
  OSData *interruptSpecifierData = OSData::withBytes(&channel->offerMessage.channelId, sizeof (channel->offerMessage.channelId));
  interruptSpecifiers->setObject(interruptSpecifierData);
  interruptSpecifierData->release();
  
  //
  // Create dictionary and set properties, releasing them after completion.
  //
  OSDictionary *dict = OSDictionary::withCapacity(5);
  if (dict == NULL) {
    devType->release();
    channelNumber->release();
    interruptControllers->release();
    interruptSpecifiers->release();
    return NULL;
  }
  
  bool result = dict->setObject(kHyperVVMBusDeviceChannelTypeKey, devType) &&
                dict->setObject(kHyperVVMBusDeviceChannelIDKey, channelNumber) &&
                dict->setObject(gIOInterruptControllersKey, interruptControllers) &&
                dict->setObject(gIOInterruptSpecifiersKey, interruptSpecifiers);
  
  devType->release();
  channelNumber->release();
  interruptControllers->release();
  interruptSpecifiers->release();
  
  if (!result) {
    dict->release();
    return NULL;
  }
  return dict;
}

bool HyperVVMBusController::registerVMBusDevice(VMBusChannel *channel) {
  //
  // Allocate and initialize child VMBus device object.
  //
  HyperVVMBusDevice *childDevice = OSTypeAlloc(HyperVVMBusDevice);
  if (childDevice == NULL) {
    return false;
  }

  //
  // Add all device properties as a dictionary.
  //
  OSDictionary *childDict = getVMBusDevicePropertyDictionary(channel);
  if (childDict == NULL) {
    childDevice->release();
    return false;
  }
  
  bool result = childDevice->init(childDict) && childDevice->attach(this);
  if (result) {
    childDevice->registerService();
  }
  childDevice->release();

  return true;
}
