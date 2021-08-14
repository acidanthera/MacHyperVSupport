//
//  HyperVNetworkPrivate.cpp
//  Hyper-V network driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVNetwork.hpp"
#include "HyperVNetworkRegs.hpp"


void HyperVNetwork::handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count) {
  DBGLOG("Interrupt");
}

bool HyperVNetwork::connectNetwork() {
  DBGLOG("start");
  
  HyperVNetworkMessage netMsg;
  memset(&netMsg, 0, sizeof (netMsg));
  netMsg.messageType = kHyperVNetworkMessageTypeInit;
  netMsg.init.initVersion.maxProtocolVersion = kHyperVNetworkProtocolVersion1;
  netMsg.init.initVersion.minProtocolVersion = kHyperVNetworkProtocolVersion1;
  
  
  
  HyperVVMBusDeviceRequest request;
  request.sendData = &netMsg;
  request.responseRequired = true;
  request.sendDataLength = sizeof (netMsg);
  request.responseData = &netMsg;
  request.responseDataLength = sizeof (netMsg);
  request.sendPacketType = kVMBusPacketTypeDataInband;
  
  hvDevice->doRequest(&request);
  
  DBGLOG("type %X", netMsg.messageType);
  
  DBGLOG("response: ver %X max mdl %X status %X", netMsg.init.initComplete.negotiatedProtocolVersion, netMsg.init.initComplete.maxMdlChainLength, netMsg.init.initComplete.status);
  
  return true;
}
