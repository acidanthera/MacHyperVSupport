//
//  Interrupts.cpp
//  MacHyperVServices
//
//  Created by John Davis on 5/8/21.
//

#include "HyperVVMBusController.hpp"
#include "HyperVVMBusInternal.hpp"

void HyperVVMBusController::initVector(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) {
  DBGLOG("vector %u", vectorNumber);
}
