//
//  HyperVNetwork.cpp
//  Hyper-V network driver
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#ifndef HyperVNetwork_hpp
#define HyperVNetwork_hpp

#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOMbufMemoryCursor.h>
#include <IOKit/network/IONetworkMedium.h>
#include <IOKit/network/IOOutputQueue.h>

#include "HyperVVMBusDevice.hpp"
#include "HyperVNetworkRegs.hpp"

extern "C" {
#include <sys/kpi_mbuf.h>
}

typedef struct HyperVNetworkRNDISRequest {
  HyperVNetworkRNDISMessage message;
  UInt8                     messageOverflow[PAGE_SIZE];
  
  HyperVNetworkRNDISRequest *next;
  IOLock                    *lock;
  bool                      isSleeping;
  
  HyperVDMABuffer           dmaBuffer;
} HyperVNetworkRNDISRequest;

class HyperVNetwork : public IOEthernetController {
  OSDeclareDefaultStructors(HyperVNetwork);
  HVDeclareLogFunctionsVMBusChild("net");
  typedef IOEthernetController super;

private:
  HyperVVMBusDevice *_hvDevice = nullptr;

  //
  // Network structures.
  //
  HyperVNetworkProtocolVersion _netVersion;
  bool                         _isNetworkEnabled = false;
  IOEthernetInterface          *_ethInterface    = nullptr;
  IOEthernetAddress            _ethAddress       = { };
  bool                         _isLinkUp         = false;
  IONetworkMedium              *_networkMedium   = nullptr;

  //
  // Receive buffer.
  //
  HyperVDMABuffer _receiveBuffer      = { };
  UInt32          _receiveBufferSize  = 0;
  UInt32          _receiveGpadlHandle = kHyperVGpadlNullHandle;

  //
  // Send buffer and tracking info.
  //
  HyperVDMABuffer _sendBuffer             = { };
  UInt32          _sendBufferSize         = 0;
  UInt32          _sendGpadlHandle        = kHyperVGpadlNullHandle;
  UInt32          _sendSectionSize        = 0;
  UInt32          _sendSectionCount       = 0;
  UInt32          *_sendIndexMap          = nullptr;
  size_t          _sendIndexMapSize       = 0;
  UInt32          _sendIndexesOutstanding = 0;
  UInt32                        oldSends = 0;
  UInt64    totalbytes = 0;
  UInt64    totalRX = 0;
  UInt64 preCycle = 0;
  UInt64 midCycle = 0;
  UInt64 postCycle = 0;
  UInt64 stalls = 0;
  
  IOLock                        *rndisLock = NULL;
  UInt32                        rndisTransId = 0;
  
  HyperVNetworkRNDISRequest     *rndisRequests;
  

  

  
  void handleTimer();
  bool wakePacketHandler(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength);
  void handlePacket(VMBusPacketHeader *pktHeader, UInt32 pktHeaderLength, UInt8 *pktData, UInt32 pktDataLength);
  
  
  bool negotiateProtocol(HyperVNetworkProtocolVersion protocolVersion);
  
  //
  // Send/receive buffers.
  //
  IOReturn initSendReceiveBuffers();
  void freeSendReceiveBuffers();
  UInt32 getNextSendIndex();
  UInt32 getFreeSendIndexCount();
  void releaseSendIndex(UInt32 sendIndex);
  
  bool connectNetwork();
  
  void handleRNDISRanges(VMBusPacketTransferPages *pktPages, UInt32 pktLength);
  void handleCompletion(void *pktData, UInt32 pktLength);

  bool processRNDISPacket(UInt8 *data, UInt32 dataLength);
  void processIncoming(UInt8 *data, UInt32 dataLength);
  
  //
  // RNDIS setup and operations.
  //
  HyperVNetworkRNDISRequest *allocateRNDISRequest(size_t additionalLength = 0);
  void freeRNDISRequest(HyperVNetworkRNDISRequest *rndisRequest);
  UInt32 getNextRNDISTransId();
  bool sendRNDISRequest(HyperVNetworkRNDISRequest *rndisRequest, bool waitResponse = false);
  
  bool initializeRNDIS();
  IOReturn getRNDISOID(HyperVNetworkRNDISOID oid, void *value, UInt32 *valueSize);
  IOReturn setRNDISOID(HyperVNetworkRNDISOID oid, void *value, UInt32 valueSize);
  
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
  bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
  void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
  
  //
  // IOEthernetController overrides.
  //
  IOReturn getHardwareAddress(IOEthernetAddress *addrP) APPLE_KEXT_OVERRIDE;
  
  UInt32 outputPacket(mbuf_t m, void *param) APPLE_KEXT_OVERRIDE;
  
  virtual IOReturn enable(IONetworkInterface *interface) APPLE_KEXT_OVERRIDE;
  virtual IOReturn disable(IONetworkInterface *interface) APPLE_KEXT_OVERRIDE;
};

#endif
