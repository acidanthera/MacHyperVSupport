//
//  HyperVNetwork.cpp
//  Hyper-V network driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVNetwork_hpp
#define HyperVNetwork_hpp

#include "HyperVVMBusDevice.hpp"
#include "HyperVNetworkRegs.hpp"

#define super IOService

#define SYSLOG(str, ...) SYSLOG_PRINT("HyperVNetwork", str, ## __VA_ARGS__)
#define DBGLOG(str, ...) DBGLOG_PRINT("HyperVNetwork", str, ## __VA_ARGS__)

typedef struct HyperVNetworkRNDISRequest {
  HyperVNetworkRNDISMessage message;
  UInt8                     messageOverflow[PAGE_SIZE];
  
  HyperVNetworkRNDISRequest *next;
  IOLock                    *lock;
  bool                      isSleeping;
  
  IOBufferMemoryDescriptor  *memDescriptor;
  mach_vm_address_t         messagePhysicalAddress;
} HyperVNetworkRNDISRequest;

class HyperVNetwork : public IOService {
  OSDeclareDefaultStructors(HyperVNetwork);

private:
  //
  // Parent VMBus device.
  //
  HyperVVMBusDevice       *hvDevice;
  
  HyperVNetworkProtocolVersion  netVersion;
  UInt32                        receiveBufferSize;
  UInt32                        receiveGpadlHandle;
  UInt8                          *receiveBuffer;
  
  UInt32                        sendBufferSize;
  UInt32                        sendGpadlHandle;
  void                          *sendBuffer;
  UInt32                        sendSectionSize;
  UInt32                        sendSectionCount;
  
  IOLock                        *rndisLock = NULL;
  UInt32                        rndisTransId = 0;
  
  HyperVNetworkRNDISRequest     *rndisRequests;
  
  void handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count);
  
  
  bool negotiateProtocol(HyperVNetworkProtocolVersion protocolVersion);
  bool initBuffers();
  bool connectNetwork();
  
  void handleRNDISRanges(VMBusPacketTransferPages *pktPages, UInt32 headerSize, UInt32 pktSize);

  bool processRNDISPacket(UInt8 *data, UInt32 dataLength);
  
  //
  // RNDIS
  //
  HyperVNetworkRNDISRequest *allocateRNDISRequest();
  void freeRNDISRequest(HyperVNetworkRNDISRequest *rndisRequest);
  UInt32 getNextRNDISTransId();
  bool sendRNDISMessage(HyperVNetworkRNDISRequest *rndisRequest, bool waitResponse = false);
  
  bool initializeRNDIS();
  bool queryRNDISOID(HyperVNetworkRNDISOID oid, void *value, UInt32 *valueSize);
  
public:
  //
  // IOService overrides.
  //
  virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#endif /* HyperVNetwork_hpp */
