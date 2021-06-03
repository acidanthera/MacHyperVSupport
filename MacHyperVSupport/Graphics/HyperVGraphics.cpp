//
//  HyperVGraphics.cpp
//  Hyper-V basic graphics driver
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#include "HyperVGraphics.hpp"

OSDefineMetaClassAndStructors(HyperVGraphics, super);

bool HyperVGraphics::start(IOService *provider) {
  if (!PE_state.initialized) {
    SYSLOG("PE state is not initialized");
    return false;
  }
  
  //
  // Pull video parameters from kernel.
  //
  UInt32 videoBaseAddress  = (UInt32) PE_state.video.v_baseAddr;
  UInt32 videoWidth        = (UInt32) PE_state.video.v_width;
  UInt32 videoHeight       = (UInt32) PE_state.video.v_height;
  UInt32 videoDepth        = (UInt32) PE_state.video.v_depth;
  UInt32 videoBytesPerRow  = (UInt32) PE_state.video.v_rowBytes;
  
  if (!super::start(provider)) {
    return false;
  }
  
  IOPCIDevice *device = createNub(OSDictionary::withCapacity(1));
  device->init(OSDictionary::withCapacity(5));
  
  initializeNub(device, OSDictionary::withCapacity(1));

  OSData *            prop;
      const OSSymbol *    nameProp;
  
  nameProp = OSSymbol::withCString( "display" );
      if (nameProp)
      {
        
        device->setProperty( gIONameKey, (OSSymbol *) nameProp);
          nameProp->release();
      }

      prop = OSData::withBytes( "display" , static_cast<unsigned int>(strlen("display") + 1));
      if (prop)
      {
        device->setProperty("name", prop );
          prop->release();
      }
  
  UInt32 classcpde = 0x030000;
  prop = OSData::withBytes(&classcpde, sizeof (classcpde));
  device->setProperty("class-code", prop);
  prop->release();
  
   classcpde = 0x1414;
  prop = OSData::withBytes(&classcpde, sizeof (classcpde));
  device->setProperty("vendor-id", prop);
  prop->release();
   classcpde = 0x5353;
  prop = OSData::withBytes(&classcpde, sizeof (classcpde));
  device->setProperty("device-id", prop);
  prop->release();
  
  prop = OSData::withBytes( "Hyper-V Graphics" , static_cast<unsigned int>(strlen("Hyper-V Graphics") + 1));
      if (nameProp)
      {
        
        device->setProperty("model", prop );
          prop->release();
      }

  
  //
  // Create framebuffer memory properties.
  // IONDRVFramebuffer will pull this information on start.
  //
  IODeviceMemory::InitElement fbMemList[1];
  fbMemList[0].start  = videoBaseAddress;
  fbMemList[0].length = videoHeight * videoBytesPerRow;
  DBGLOG("Framebuffer memory start 0x%llX length 0x%llX", fbMemList[0].start, fbMemList[0].length);
  
  OSArray *fbMemArray = IODeviceMemory::arrayFromList(fbMemList, 1);
  device->setDeviceMemory(fbMemArray);
  fbMemArray->release();
  
  //
  // IONDRVFramebuffer looks for either IOPCIDevice or IOPlatformDevice named "display".
  //
  //registerService();
  publishNub(device, 0);
  
  SYSLOG("Framebuffer is at 0x%X (width %u height %u bits %u bytes/row %u)", videoBaseAddress, videoWidth, videoHeight, videoDepth, videoBytesPerRow);
  return true;
}

