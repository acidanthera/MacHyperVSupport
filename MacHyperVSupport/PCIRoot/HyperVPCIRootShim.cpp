//
//  HyperVPCIRootShim.cpp
//  MacHyperVSupport
//
//  Created by John Davis on 6/2/21.
//

#include "HyperVPCIRootShim.hpp"

OSDefineMetaClassAndStructors(HyperVPCIRootShim, super);

IOService* HyperVPCIRootShim::probe(IOService *provider, SInt32 *score) {
  //
  // Wait for HyperVPCIRoot to be installed as the first IOPCIBridge.
  //
  // This will allow us to process reads/writes from IOPCIConfigurator
  // that would otherwise go to the real PCI bus on a Gen1 VM.
  //
  OSDictionary *pciMatching = IOService::serviceMatching("HyperVPCIRoot");
  if (pciMatching == NULL) {
    SYSLOG("Failed to create HyperVPCIRoot matching dictionary");
    return NULL;
  }
  
  DBGLOG("Waiting for HyperVPCIRoot");
  IOService *pciService = waitForMatchingService(pciMatching);
  pciMatching->release();
  if (pciService == NULL) {
    SYSLOG("Failed to locate HyperVPCIRoot");
    return NULL;
  }
  pciService->release();

  DBGLOG("HyperVPCIRoot is now loaded");
  return NULL;
}
