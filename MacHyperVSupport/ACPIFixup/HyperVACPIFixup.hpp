//
//  HyperVACPIFixup.hpp
//  Hyper-V ACPI fixup module
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#ifndef HyperVACPIFixup_hpp
#define HyperVACPIFixup_hpp

#include <IOKit/IOService.h>

#include "acpi.hpp"
#include "HyperV.hpp"

class HyperVACPIFixup {
  HVDeclareLogFunctionsNonIOKit("acpifix", "HyperVACPIFixup");

private:
  //
  // Global instance.
  //
  static HyperVACPIFixup *_instance;

  //
  // AcpiWalkNamespace wrapper
  //
  mach_vm_address_t _acpiWalkNamespaceAddr = 0;
  UInt64            _acpiWalkNamespaceOrig[2] {};
  static ACPI_STATUS wrapAcpiWalkNamespace(ACPI_OBJECT_TYPE Type, ACPI_HANDLE StartObject, UInt32 MaxDepth,
                                           ACPI_WALK_CALLBACK UserFunction, void *Context, void **ReturnValue);
  ACPI_WALK_CALLBACK _origAcpiWalkCallback = nullptr;
  static ACPI_STATUS acpiWalkCallback(ACPI_HANDLE ObjHandle, UInt32 NestingLevel, void *Context, void **ReturnValue);

  //
  // Initialization function.
  //
  void init();

public:
  //
  // Instance creator.
  //
  static HyperVACPIFixup *createInstance() {
    if (_instance == nullptr) {
      _instance = new HyperVACPIFixup;
      if (_instance != nullptr) {
        _instance->init();
      }
    }

    return _instance;
  }
};

#endif
