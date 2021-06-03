//
//  HyperVPCIRootShim.hpp
//  MacHyperVSupport
//
//  Created by John Davis on 6/2/21.
//

#ifndef HyperVPCIRootShim_hpp
#define HyperVPCIRootShim_hpp

#include "HyperV.hpp"
#include <IOKit/IOService.h>

#define super IOService

#define SYSLOG(str, ...) SYSLOG_PRINT("HyperVPCIRoot", str, ## __VA_ARGS__)
#define DBGLOG(str, ...) DBGLOG_PRINT("HyperVPCIRoot", str, ## __VA_ARGS__)

class HyperVPCIRootShim : public IOService {
  OSDeclareDefaultStructors(HyperVPCIRootShim);
  
public:
  //
  // IOService overrides.
  //
  virtual IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
};

#endif
