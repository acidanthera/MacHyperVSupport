//
//  VMBusDriver.hpp
//  Hyper-V VMBus driver-side structures
//
//  Copyright © 2021 Goldfish64. All rights reserved.
//

#ifndef VMBusDriver_hpp
#define VMBusDriver_hpp

#include "VMBus.hpp"

#define kVMBusArrayInitialChildrenCount        10
#define kHyperVVMBusInterruptControllerName   "HyperVVMBusInterruptController"

#define kHyperVMaxChannels                    256

//
// Unknown why this is the start handle, Linux and BSD both do this.
//
#define kHyperVGpadlStartHandle               0xE1E10

typedef struct {
  IOBufferMemoryDescriptor  *bufDesc;
  IODMACommand              *dmaCmd;
  mach_vm_address_t         physAddr;
  void                      *buffer;
  size_t                    size;
} HyperVDMABuffer;

//
// Channel status.
//
typedef enum {
  kVMBusChannelStatusNotPresent = 0,
  kVMBusChannelStatusClosed,
  kVMBusChannelStatusGpadlConfigured,
  kVMBusChannelStatusOpen
} VMBusChannelStatus;

//
// Used for per-channel tracking of buffers and stats.
//
typedef struct {
  VMBusChannelStatus              status;
  uuid_string_t                   typeGuidString;
  VMBusChannelMessageChannelOffer offerMessage;
  
  //
  // Unique GPADL handle for this channel.
  //
  UInt32                          gpadlHandle;
  HyperVDMABuffer                 dataBuffer;
  HyperVDMABuffer                 eventBuffer;
  
  //
  // Index into ring buffer where receive pages begin.
  //
  UInt16                          rxPageIndex;
  
  VMBusRingBuffer                 *txBuffer;
  VMBusRingBuffer                 *rxBuffer;
  
} VMBusChannel;

#endif
