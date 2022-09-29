//
//  HyperVVMBusPrivate.cpp
//  Hyper-V VMBus controller
//
//  Copyright © 2021-2022 Goldfish64. All rights reserved.
//

#include "HyperVVMBus.hpp"



bool HyperVVMBus::allocateVMBusBuffers() {
  //
  // Allocate common VMBus structures.
  //
  getHvController()->allocateDmaBuffer(&vmbusEventFlags, PAGE_SIZE);
  getHvController()->allocateDmaBuffer(&vmbusMnf1, PAGE_SIZE);
  getHvController()->allocateDmaBuffer(&vmbusMnf2, PAGE_SIZE);
  
  //
  // Event flag bits primarily used on Windows Server 2008 R2 and older.
  //
  vmbusRxEventFlags = (HyperVEventFlags*)vmbusEventFlags.buffer;
  vmbusTxEventFlags = (HyperVEventFlags*)((UInt8*)vmbusEventFlags.buffer + PAGE_SIZE / 2);
  
  return true;
}
