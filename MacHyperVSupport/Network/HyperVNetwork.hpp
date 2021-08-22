//
//  HyperVNetwork.cpp
//  Hyper-V network driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVNetwork_hpp
#define HyperVNetwork_hpp

#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>

#include "HyperVVMBusDevice.hpp"
#include "HyperVNetworkRegs.hpp"

#define super IOEthernetController

#define SYSLOG(str, ...) SYSLOG_PRINT("HyperVNetwork", str, ## __VA_ARGS__)
#define DBGLOG(str, ...) DBGLOG_PRINT("HyperVNetwork", str, ## __VA_ARGS__)

#define MBit 1000000

typedef struct HyperVNetworkRNDISRequest {
  HyperVNetworkRNDISMessage message;
  UInt8                     messageOverflow[PAGE_SIZE];
  
  HyperVNetworkRNDISRequest *next;
  IOLock                    *lock;
  bool                      isSleeping;
  
  IOBufferMemoryDescriptor  *memDescriptor;
  mach_vm_address_t         messagePhysicalAddress;
} HyperVNetworkRNDISRequest;

class HyperVNetwork : public IOEthernetController {
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
  HyperVVMBusDeviceRequestNew   *vmbusRequests;
  
  IOEthernetInterface           *ethInterface;
  IOEthernetAddress             ethAddress;
  
  bool                          isLinkUp = false;
  OSDictionary                  *mediumDict;
  UInt32                        currentMediumIndex;
  
  void handleInterrupt(OSObject *owner, IOInterruptEventSource *sender, int count);
  
  
  bool negotiateProtocol(HyperVNetworkProtocolVersion protocolVersion);
  bool initBuffers();
  bool connectNetwork();
  
  void handleRNDISRanges(VMBusPacketTransferPages *pktPages, UInt32 headerSize, UInt32 pktSize);
  void handleCompletion();

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
  
  //
  // Private
  //
  void addNetworkMedium(UInt32 index, UInt32 type, UInt32 speed);
  void createMediumDictionary();
  bool readMACAddress();
  void updateLinkState(HyperVNetworkRNDISMessageIndicateStatus *indicateStatus);
  
public:
  //
  // IOService overrides.
  //
  virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  
  //
  // IOEthernetController overrides.
  //
  IOReturn getHardwareAddress(IOEthernetAddress *addrP) APPLE_KEXT_OVERRIDE;
};

#endif /* HyperVNetwork_hpp */
