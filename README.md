MacHyperVSupport
================

[![Build Status](https://github.com/acidanthera/MacHyperVSupport/actions/workflows/main.yml/badge.svg?branch=master)](https://github.com/acidanthera/MacHyperVSupport/actions) [![Scan Status](https://scan.coverity.com/projects/23212/badge.svg?flat=1)](https://scan.coverity.com/projects/23212)

Hyper-V integration services for macOS running on Windows Server 2008 R2 or higher (generation 1) or Windows 8.1 / Windows Server 2012 R2 or higher (generation 2). Windows Server 2016 is currently unsupported when using a generation 2 VM.

All Intel macOS versions are supported.

### Supported Hyper-V devices and services
- Heartbeat
- Guest shutdown (with daemon)
- Time synchronization (with daemon)
- Host to guest file copy (with daemon)
- PCI passthrough (partial support)
- PS/2 keyboard (applies to generation 1 VMs only)
- Synthetic graphics (full support requires HyperVFramebuffer.kext to be loaded or installed)
- Synthetic keyboard
- Synthetic mouse
- Synthetic network controller
- Synthetic IDE controller (applies to generation 1 VMs only, currently only virtual hard disks)
- Synthetic SCSI controller

### Binaries
- MacHyperVSupport.kext: Core Hyper-V support kext for macOS 10.4 to 11.0.
- MacHyperVSupportMonterey.kext: Core Hyper-V support kext for macOS 12.0 and newer.
- MacHyperVFramebuffer.kext: Basic framebuffer support kext for all macOS versions. This extension must be installed and kext signing disabled in SIP on macOS 11.0 and newer due to macOS requirements.
- hvfilecopyd: File copy userspace daemon.
- hvshutdownd: Shutdown userspace daemon.
- hvtimesyncd: Time synchronization userspace daemon.

### OpenCore configuration
#### ACPI
- [SSDT-HV-VMBUS](https://github.com/acidanthera/OpenCorePkg/blob/master/Docs/AcpiSamples/Source/SSDT-HV-VMBUS.dsl): Enables correct Startup Disk operation, ensure patches described within are also configured.
- [SSDT-HV-DEV](https://github.com/acidanthera/OpenCorePkg/blob/master/Docs/AcpiSamples/Source/SSDT-HV-DEV.dsl): Required on Windows Server 2019 / Windows 10 and newer, provides proper processor objects and disables incompatible virtual devices under macOS.
- [SSDT-HV-PLUG](https://github.com/acidanthera/OpenCorePkg/blob/master/Docs/AcpiSamples/Source/SSDT-HV-PLUG.dsl): Ensures VMPlatformPlugin loads on Big Sur and above, avoids freezes with the default PlatformPlugin.
* Ensure all SSDTs are added in the order above. Legacy iasl will need to be used when using older versions of macOS prior to 10.7.
* Ensure all patches described in above SSDTs are present in `ACPI->Patch`.

#### Booter quirks
- `AllowRelocationBlock` - required for macOS 10.7 and older
- `AvoidRuntimeDefrag` - required
- `ClearTaskSwitchBit` - required for macOS 10.7 and older
- `ForceExitBootServices` - required for macOS 10.7 and older
- `ProvideCustomSlide` - required
- `RebuildAppleMemoryMap` - required for macOS 10.6 and older

#### Kernel
- Quirks
  - `ProvideCurrentCpuInfo` - required for proper TSC/FSB values and CPU topology values.
- The following additional kernel extensions are required:
  - [Lilu](https://github.com/acidanthera/Lilu) - patching and library functions
  - [VirtualSMC](https://github.com/acidanthera/VirtualSMC) - SMC emulator
- Force
  - On older versions of macOS, the following kernel extensions may need to be Force injected. Refer to the OpenCore Configuration manual for details.
  - IONetworkingFamily (`com.apple.iokit.IONetworkingFamily`)
  - IOSCSIParallelFamily (`com.apple.iokit.IOSCSIParallelFamily`)
  - If injecting MacHyperVFramebuffer on supported versions, IOGraphicsFamily (`com.apple.iokit.IOGraphicsFamily`) must also be injected with `Force`
- Patch
  - Disable _hpet_init
    - Arch = `i386`
    - Base = `_hpet_init`
    - Comment = `Disables _hpet_init due to no HPET hardware present`
    - Count = `1`
    - Identifier = `kernel`
    - MaxKernel = `9.5.99`
    - Replace = `C3`
  - Disable IOHIDDeviceShim::newTransportString()
    - Arch = `i386`
    - Base = `__ZNK15IOHIDDeviceShim18newTransportStringEv`
    - Comment = `Fix crash in IOHIDDeviceShim::newTransportString() caused by NULL _deviceType`
    - Count = `1`
    - Identifier = `com.apple.iokit.IOHIDFamily`
    - MaxKernel = `9.6.99`
    - MinKernel = `9.4.0`
    - Replace = `31C0C3`
  - Disable scaling factor for X/Y mouse movement
    - Arch = `i386`
    - Base = `__ZN16IOHIDEventDriver21handleInterruptReportE12UnsignedWideP18IOMemoryDescriptor15IOHIDReportTypem`
    - Comment = `Workaround for absence of AbsoluteAxisBoundsRemovalPercentage in 10.4`
    - Identifier = `com.apple.iokit.IOHIDFamily`
    - Find = `BA1F85EB51`
    - MaxKernel = `8.11.99`
    - MinKernel = `8.0.0`
    - Replace = `BA00000000`
- Emulate
  - DummyPowerManagement and CPU spoofing may be required depending on the host CPU for older versions of macOS.

#### UEFI
- Quirks
  - `DisableSecurityPolicy` - required on Windows Server 2019 / Windows 10 and newer

### Installer image creation
- Installer images can either be passed in from USB hard disks, or converted from a DMG to a VHD/VHDX image using `qemu-img`:
  - DMGs need to be in a read/write format first.
  - VHD: `qemu-img convert -f raw -O vpc Installer.dmg Installer.vhd`
  - VHDX: `qemu-img convert -f raw -O vhdx Installer.dmg Installer.vhdx`

### Boot arguments
See the [module list](Docs/modules.md) for boot arguments for each module.

### Credits
- [Apple](https://www.apple.com) for macOS
- [Goldfish64](https://github.com/Goldfish64) for this software
- [vit9696](https://github.com/vit9696) for [Lilu.kext](https://github.com/acidanthera/Lilu) and providing assistance
- [flagers](https://github.com/flagersgit) for file copy implementation and providing assistance
- [Microsoft Hypervisor Top-Level Functional Specification](https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/reference/tlfs)
- [Linux](https://github.com/torvalds/linux/tree/master/drivers/hv) and [FreeBSD](https://github.com/freebsd/freebsd-src/tree/main/sys/dev/hyperv) Hyper-V integration services
