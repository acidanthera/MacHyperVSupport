//
//  HyperVACPIFixup.cpp
//  Hyper-V ACPI fixup module
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "HyperVACPIFixup.hpp"

HyperVACPIFixup *HyperVACPIFixup::_instance;

void HyperVACPIFixup::init() {
  HVCheckDebugArgs();
  HVDBGLOG("Initializing ACPI fixup");

  //
  // Patch AcpiWalkNamespace to call our wrapper function instead.
  //
  _acpiWalkNamespaceAddr = (mach_vm_address_t)AcpiWalkNamespace;

  // Save start of function.
  lilu_os_memcpy(_acpiWalkNamespaceOrig, (void *) _acpiWalkNamespaceAddr, sizeof (_acpiWalkNamespaceOrig));

  // Patch to call wrapper.
#if defined(__i386__)
  UInt64 patched[2] {0x25FF | ((_acpiWalkNamespaceAddr + 8) << 16), (UInt32)wrapAcpiWalkNamespace};
#elif defined(__x86_64__)
  UInt64 patched[2] {0x0225FF, (uintptr_t)wrapAcpiWalkNamespace};
#else
#error Unsupported arch
#endif
  if (MachInfo::setKernelWriting(true, KernelPatcher::kernelWriteLock) == KERN_SUCCESS) {
    lilu_os_memcpy((void *) _acpiWalkNamespaceAddr, patched, sizeof (patched));
    MachInfo::setKernelWriting(false, KernelPatcher::kernelWriteLock);
  }

  HVDBGLOG("Patched AcpiWalkNamespace");
}

ACPI_STATUS HyperVACPIFixup::wrapAcpiWalkNamespace(ACPI_OBJECT_TYPE Type, ACPI_HANDLE StartObject, UInt32 MaxDepth,
                                                   ACPI_WALK_CALLBACK UserFunction, void *Context, void **ReturnValue) {
  ACPI_STATUS status;

  //
  // Replace the callback function passed to AcpiWalkNamespace only if
  // we are walking ACPI Device objects.
  //
  _instance->HVDBGLOG("Walking namespace for object type 0x%X", Type);
  if (Type == ACPI_TYPE_DEVICE) {
    _instance->_origAcpiWalkCallback = UserFunction;
    _instance->HVDBGLOG("Walking namespace for ACPI Device objects, original callback is %p",
                        _instance->_origAcpiWalkCallback);
    UserFunction = acpiWalkCallback;
  }

  //
  // Restore and call original AcpiWalkNamespace function.
  //
  if (MachInfo::setKernelWriting(true, KernelPatcher::kernelWriteLock) == KERN_SUCCESS) {
    lilu_os_memcpy((void *) _instance->_acpiWalkNamespaceAddr, _instance->_acpiWalkNamespaceOrig, sizeof (_instance->_acpiWalkNamespaceOrig));
    MachInfo::setKernelWriting(false, KernelPatcher::kernelWriteLock);
  }
  status = FunctionCast(wrapAcpiWalkNamespace, _instance->_acpiWalkNamespaceAddr)(Type, StartObject, MaxDepth,
                                                                                  UserFunction, Context, ReturnValue);

  //
  // If we did not walk devices, patch again for another pass.
  //
  if (Type != ACPI_TYPE_DEVICE) {
    // Patch to call wrapper.
  #if defined(__i386__)
    UInt64 patched[2] {0x25FF | ((_instance->_acpiWalkNamespaceAddr + 8) << 16), (UInt32)wrapAcpiWalkNamespace};
  #elif defined(__x86_64__)
    UInt64 patched[2] {0x0225FF, (uintptr_t)wrapAcpiWalkNamespace};
  #else
  #error Unsupported arch
  #endif
    if (MachInfo::setKernelWriting(true, KernelPatcher::kernelWriteLock) == KERN_SUCCESS) {
      lilu_os_memcpy((void *) _instance->_acpiWalkNamespaceAddr, patched, sizeof (patched));
      MachInfo::setKernelWriting(false, KernelPatcher::kernelWriteLock);
    }
  } else {
    _instance->HVDBGLOG("Removing ACPI fixup");
    delete _instance;
  }

  return status;
}

ACPI_STATUS HyperVACPIFixup::acpiWalkCallback(ACPI_HANDLE ObjHandle, UInt32 NestingLevel, void *Context, void **ReturnValue) {
  ACPI_STATUS         status;
  ACPI_NAMESPACE_NODE *node;
  ACPI_DEVICE_ID      hid;
  bool                skipDevice = false;

  //
  // Get node from handle.
  //
  _instance->HVDBGLOG("Callback on ACPI handle %p, nest level %u", ObjHandle, NestingLevel);
  node = static_cast<ACPI_NAMESPACE_NODE*>(ObjHandle);

  //
  // Check _HID of node.
  // The large number of ACPI0007 devices in Windows Server 2019 and newer cause significant
  // issues in the ACPI implementations in macOS 10.6 and older.
  //
  status = AcpiUtExecute_HID(node, &hid);
  if (status == AE_OK) {
    if (strncmp(hid.Value, "ACPI0007", sizeof (hid.Value)) == 0) {
      _instance->HVDBGLOG("Skipping unsupported ACPI0007 device: %.4s", node->Name.Ascii);
      skipDevice = true;
    }
  }

  return skipDevice ? AE_OK : _instance->_origAcpiWalkCallback(ObjHandle, NestingLevel, Context, ReturnValue);
}
