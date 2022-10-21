MacHyperVSupport
================

[![Build Status](https://github.com/acidanthera/MacHyperVSupport/workflows/CI/badge.svg?branch=master)](https://github.com/acidanthera/MacHyperVSupport/actions) [![Scan Status](https://scan.coverity.com/projects/23212/badge.svg?flat=1)](https://scan.coverity.com/projects/23212)

Hyper-V integration services for macOS. Requires a Generation 2 virtual machine on Windows Server 2012 R2 / Windows 8.1 or higher. Windows Server 2016 is currently unsupported.

All Intel macOS versions are supported. macOS 12.0 and newer should use `MacHyperVSupportMonterey.kext` instead.

### Supported Hyper-V devices and services
- Heartbeat
- Guest shutdown (with daemon)
- Time synchronization (with daemon)
- Host to guest file copy (with daemon)
- PCI passthrough (partial support)
- Synthetic graphics (partial support)
- Synthetic keyboard
- Synthetic mouse
- Synthetic network controller
- Synthetic SCSI controller

### OpenCore configuration
#### ACPI
- [SSDT-HV-VMBUS](https://github.com/acidanthera/OpenCorePkg/blob/master/Docs/AcpiSamples/Source/SSDT-HV-VMBUS.dsl): Enables correct Startup Disk operation, ensure patches described within are also configured.
- [SSDT-HV-DEV](https://github.com/acidanthera/OpenCorePkg/blob/master/Docs/AcpiSamples/Source/SSDT-HV-DEV.dsl): Required on Windows Server 2019 / Windows 10 and newer, provides proper processor objects and disables incompatible virtual devices under macOS.
- [SSDT-HV-DEV-WS2022](https://github.com/acidanthera/OpenCorePkg/blob/master/Docs/AcpiSamples/Source/SSDT-HV-DEV-WS2022.dsl): Required on Windows Server 2022 / Windows 11 and newer, disables addiitonal incompatible virtual devices under macOS.
- [SSDT-HV-PLUG](https://github.com/acidanthera/OpenCorePkg/blob/master/Docs/AcpiSamples/Source/SSDT-HV-PLUG.dsl): Ensures VMPlatformPlugin loads on Big Sur and above, avoids freezes with the default PlatformPlugin.
* Ensure all patches described in above SSDTs are present in `ACPI->Patch`.

#### Booter quirks
- `AllowRelocationBlock` - required for macOS 10.7 and older
- `AvoidRuntimeDefrag` - required
- `ForceExitBootServices` - required for macOS 10.7 and older
- `ProvideCustomSlide` - required
- `RebuildAppleMemoryMap` - required for macOS 10.6 and older

#### Kernel
- Quirks
  - `ProvideCurrentCpuInfo` - required for proper TSC/FSB values and CPU topology values.
- The following additional kernel extensions are required:
  - [Lilu](https://github.com/acidanthera/Lilu) - patching and library functions
  - [VirtualSMC](https://github.com/acidanthera/VirtualSMC) - SMC emulator
- Block
  - com.apple.driver.AppleEFIRuntime
    - Required for 32-bit versions of macOS (10.4 and 10.5, and 10.6 in 32-bit mode). EFI runtime services and NVRAM are unavailable in those versions due to incompatiblities with the Hyper-V UEFI.
- Force
  - On older versions of macOS, the following kernel extensions may need to be Force injected. Refer to the OpenCore Configuration manual for details.
  - IONetworkingFamily (`com.apple.iokit.IONetworkingFamily`)
  - IOSCSIParallelFamily (`com.apple.iokit.IOSCSIParallelFamily`)
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
    - MinKernel = `9.5.0`
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

#### NVRAM
- Boot arguments
  - `-legacy` is required for running 32-bit versions of macOS (10.4 - 10.5, 10.6 if running in 32-bit mode). 64-bit applications and NVRAM support are unavailable in those versions.

#### UEFI
- Quirks
  - `DisableSecurityPolicy` - required on Windows Server 2019 / Windows 10 and newer

### Installer image creation
- Installer images can either be passed in from USB hard disks, or converted from a DMG to a VHDX image using `qemu-img`:
  - DMGs need to be in a read/write format first.
  - `qemu-img convert -f raw -O vhdx Installer.dmg Installer.vhdx`

### Boot arguments
| Module              | Debug            | Message Debug     | Off               |
|---------------------|------------------|-------------------|-------------------|
| CPU disabler (10.4) | -hvcpudbg        | N/A               | N/A               |
| Graphics bridge     | -hvgfxbdbg       | -hvgfxbmsgdbg     | -hvgfxboff        |
| Heartbeat           | -hvheartdbg      | -hvheartmsgdbg    | -hvheartoff       |
| Keyboard            | -hvkbddbg        | -hvkbdmsgdbg      | -hvkbdoff         |
| Mouse               | -hvmousdbg       | -hvmousmsgdbg     | -hvmousoff        |
| Network             | -hvnetdbg        | -hvnetmsgdbg      | -hvnetoff         |
| PCI bridge          | -hvpcibdbg       | -hvpcibmsgdbg     | -hvpciboff        |
| PCI module support  | -hvpcimdbg       | N/A               | N/A               |
| PCI provider        | -hvpcipdbg       | N/A               | N/A               |
| PCI root bridge     | -hvpcirdbg       | N/A               | N/A               |
| Shutdown            | -hvshutdbg       | -hvshutmsgdbg     | -hvshutoff        |
| Storage             | -hvstordbg       | -hvstormsgdbg     | -hvstoroff        |
| Time sync           | -hvtimedbg       | -hvtimemsgdbg     | -hvtimeoff        |
| VMBus controller    | -hvvmbusdbg      | N/A               | N/A               |
| VMBus device nub    | -hvvmbusdevdbg   | N/A               | N/A               |

### Credits
- [Apple](https://www.apple.com) for macOS
- [Goldfish64](https://github.com/Goldfish64) for this software
- [vit9696](https://github.com/vit9696) for [Lilu.kext](https://github.com/acidanthera/Lilu) and providing assistance
- [flagers](https://github.com/flagersgit) for file copy implementation and providing assistance
- [Microsoft Hypervisor Top-Level Functional Specification](https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/reference/tlfs)
- [Linux](https://github.com/torvalds/linux/tree/master/drivers/hv) and [FreeBSD](https://github.com/freebsd/freebsd-src/tree/main/sys/dev/hyperv) Hyper-V integration services
