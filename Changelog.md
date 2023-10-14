MacHyperVSupport Changelog
============================
#### v0.9.7
- Fixed disks on a passed-in PCI device not being usable
- Improved PCI interrupt handling to support both MSI and MSI-X

#### v0.9.6
- Fixed extended registers not being correctly read/written

#### v0.9.5
- Fixed no packets being received on certain older versions of Hyper-V
- Added support for promiscuous mode

#### v0.9.4
- Added constants for macOS 14 support

#### v0.9.3
- Created daemons for each userspace function replacing hvutil
- Added support for host to guest file copy (Guest Services integration service)
- Fixed very high storage latency when heaving network I/O is occurring

#### v0.9.2
- Fixed crash when control key is pressed under macOS 10.4
- Fixed DMA allocations under macOS 10.4 and 10.5
- Refactored and cleaned up VMBus core logic and integration services
- Fixed crash caused by a buffer overrun in network packet sending
- Fixed intermittent hangs in storage and network drivers
- Added support for storage disk addition and removal while VM is running
- Added support for guest restart via Restart-VM
- Renamed hvshutdown daemon to hvutil to support all userspace-side functions
- Added guest time synchronization support
- Added support for "Type clipboard text" function

#### v0.9.1
- Added initial PCI passthrough support
- Fixed crash related to IOPCIBridge on 12.0 and newer
- Added support for macOS 10.4 and 10.5
- Added hvshutdown daemon to support shutdowns from Hyper-V
- Standardized boot arguments

#### v0.9
- Added constants for macOS 13 support

#### v0.8
- Latest Windows support

#### v0.7
- Added networking support

#### v0.6
- Added constants for macOS 12 support
- Enable loading on 32-bit macOS 10.6 and 10.7

#### v0.5
- Initial developer preview release
