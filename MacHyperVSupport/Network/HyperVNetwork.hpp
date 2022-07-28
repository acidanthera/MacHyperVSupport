//
//  HyperVNetwork.cpp
//  Hyper-V network driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef HyperVNetwork_hpp
#define HyperVNetwork_hpp

#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOMbufMemoryCursor.h>
#include <IOKit/network/IONetworkMedium.h>

#include "HyperVVMBusDevice.hpp"
#include "HyperVNetworkRegs.hpp"

extern "C" {
#include <sys/kpi_mbuf.h>
}

#define HVSYSLOG(str, ...) HVSYSLOG_PRINT("HyperVNetwork", true, hvDevice->getChannelId(), str, ## __VA_ARGS__)
#define HVDBGLOG(str, ...) \
  if (this->debugEnabled) HVDBGLOG_PRINT("HyperVNetwork", true, hvDevice->getChannelId(), str, ## __VA_ARGS__)

#define MBit 1000000

#define kHyperVNetworkMaximumTransId  0xFFFFFFFF
#define kHyperVNetworkSendTransIdBits 0xFA00000000000000

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
  typedef IOEthernetController super;

private:
  //
  // Parent VMBus device.
  //
  HyperVVMBusDevice       *hvDevice;
  IOInterruptEventSource  *interruptSource;
  bool                    debugEnabled = false;
  
  bool                          isEnabled = false;
  
  HyperVNetworkProtocolVersion  netVersion;
  UInt32                        receiveBufferSize;
  UInt32                        receiveGpadlHandle;
  UInt8                          *receiveBuffer;
  
  UInt32                        sendBufferSize;
  UInt32                        sendGpadlHandle;
  UInt8                         *sendBuffer;
  UInt32                        sendSectionSize;
  UInt32                        sendSectionCount;
  UInt64                        *sendIndexMap;
  size_t                        sendIndexMapSize;
  
  IOLock                        *rndisLock = NULL;
  UInt32                        rndisTransId = 0;
  
  HyperVNetworkRNDISRequest     *rndisRequests;
  
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
  void processIncoming(UInt8 *data, UInt32 dataLength);
  
  //
  // RNDIS
  //
  UInt32 getNextSendIndex();
  void releaseSendIndex(UInt32 sendIndex);
  HyperVNetworkRNDISRequest *allocateRNDISRequest();
  void freeRNDISRequest(HyperVNetworkRNDISRequest *rndisRequest);
  UInt32 getNextRNDISTransId();
  bool sendRNDISRequest(HyperVNetworkRNDISRequest *rndisRequest, bool waitResponse = false);
  bool sendRNDISDataPacket(mbuf_t packet);
  
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
  
  UInt32 outputPacket(mbuf_t m, void *param) APPLE_KEXT_OVERRIDE;
  
  virtual IOReturn enable(IONetworkInterface *interface) APPLE_KEXT_OVERRIDE;
  virtual IOReturn disable(IONetworkInterface *interface) APPLE_KEXT_OVERRIDE;
};

#endif /* HyperVNetwork_hpp */
