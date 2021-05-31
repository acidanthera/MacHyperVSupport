MacHyperVSupport
================

[![Build Status](https://github.com/acidanthera/MacHyperVSupport/workflows/CI/badge.svg?branch=master)](https://github.com/acidanthera/MacHyperVSupport/actions)

Hyper-V integration services for macOS. Requires a Generation 2 virtual machine on Windows 8.1 or higher.

#### Supported Hyper-V devices and services
- Heartbeat
- Synthetic mouse
- Synthetic keyboard
- Synthetic SCSI controller

#### Additional information
- The following SSDTs should be used for proper operation:
  - [SSDT-HV-CPU](https://github.com/acidanthera/OpenCorePkg/blob/master/Docs/AcpiSamples/Source/SSDT-HV-CPU.dsl): Required on Windows 10 as Hyper-V on this version may not expose processors as Processor objects
  - [SSDT-HV-PLUG](https://github.com/acidanthera/OpenCorePkg/blob/master/Docs/AcpiSamples/Source/SSDT-HV-PLUG.dsl): Ensures VMPlatformPlugin loads on Big Sur, avoids freezes with the default PlatformPlugin
  - [SSDT-HV-VMBUS](https://github.com/acidanthera/OpenCorePkg/blob/master/Docs/AcpiSamples/Source/SSDT-HV-VMBUS.dsl): Enables correct Startup Disk operation

- On older versions of macOS, IOSCSIParallelFamily (`com.apple.iokit.IOSCSIParallelFamily`) may need to be Force injected. Refer to the OpenCore Configuration manual for details.
- Booter quirks
  - `AllowRelocationBlock` - required for macOS 10.7 and older
  - `RebuildAppleMemoryMap` - required for macOS 10.6 and older
- Kernel quirks
  - `ProvideCurrentCpuInfo` - required for proper TSC/FSB values and CPU topology values.
- [UEFIGraphicsFB](https://github.com/acidanthera/UEFIGraphicsFB) is currently required for display output as Hyper-V Generation 2 VMs do not have a PCI bus
- Installer images can either be passed in from USB hard disks, or converted from a DMG to a VHDX image using `qemu-img`:
  - `qemu-img convert -f raw -O vhdx Installer.dmg Installer.vhdx`

#### Credits
- [Apple](https://www.apple.com) for macOS
- [Goldfish64](https://github.com/Goldfish64) for this software
- [Microsoft Hypervisor Top-Level Functional Specification](https://docs.microsoft.com/en-us/virtualization/hyper-v-on-windows/reference/tlfs)
- [Linux](https://github.com/torvalds/linux/tree/master/drivers/hv) and [FreeBSD](https://github.com/freebsd/freebsd-src/tree/main/sys/dev/hyperv) Hyper-V integration services
