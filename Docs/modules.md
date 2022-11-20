# Driver Module List

## Core Controller (HyperVController)
Core Hyper-V controller module.

| Boot argument  | Description |
|----------------|-------------|
| -hvctrldbg     | Enables debug printing in DEBUG builds

## CPU Disabler (HyperVCPU)
Disables additional CPUs under macOS 10.4.

| Boot argument  | Description |
|----------------|-------------|
| -hvcpudbg      | Enables debug printing in DEBUG builds

## File Copy (HyperVFileCopy)
Provides host to guest file copy support (Guest Services). Requires the `hvfilecopyd` userspace daemon to be running.

| Boot argument  | Description |
|----------------|-------------|
| -hvfcopydbg    | Enables debug printing in DEBUG builds
| -hvfcopymsgdbg | Enables debug printing of message data in DEBUG builds
| -hvfcopyoff    | Disables this module

## Graphics Bridge (HyperVGraphicsBridge)
Provides basic graphics support for macOS.

| Boot argument  | Description |
|----------------|-------------|
| -hvgfxbdbg     | Enables debug printing in DEBUG builds
| -hvgfxbmsgdbg  | Enables debug printing of message data in DEBUG builds
| -hvgfxboff     | Disables this module

## Heartbeat (HyperVHeartbeat)
Provides heartbeat reporting to Hyper-V.

| Boot argument  | Description |
|----------------|-------------|
| -hvheartdbg    | Enables debug printing in DEBUG builds
| -hvheartmsgdbg | Enables debug printing of message data in DEBUG builds
| -hvheartoff    | Disables this module

## Keyboard (HyperVKeyboard)
Provides keyboard support.

| Boot argument  | Description |
|----------------|-------------|
| -hvkbddbg      | Enables debug printing in DEBUG builds
| -hvkbdmsgdbg   | Enables debug printing of message data in DEBUG builds
| -hvkbdoff      | Disables this module

## Mouse (HyperVMouse)
Provides mouse support.

| Boot argument  | Description |
|----------------|-------------|
| -hvmousdbg     | Enables debug printing in DEBUG builds
| -hvmousmsgdbg  | Enables debug printing of message data in DEBUG builds
| -hvmousoff     | Disables this module

## Network (HyperVNetwork)
Provides networking support.

| Boot argument  | Description |
|----------------|-------------|
| -hvnetdbg      | Enables debug printing in DEBUG builds
| -hvnetmsgdbg   | Enables debug printing of message data in DEBUG builds
| -hvnetoff      | Disables this module

## PCI Bridge (HyperVPCIBridge)
Provides PCI passthrough support.

| Boot argument  | Description |
|----------------|-------------|
| -hvpcibdbg     | Enables debug printing in DEBUG builds
| -hvpcibmsgdbg  | Enables debug printing of message data in DEBUG builds
| -hvpciboff     | Disables this module

## PCI Module Device (HyperVModuleDevice)
Provides MMIO allocation/deallocation functions for PCI passthrough.

| Boot argument  | Description |
|----------------|-------------|
| -hvpcimdbg     | Enables debug printing in DEBUG builds

## PCI Provider (HyperVPCIProvider)
Provides IOACPIPlatformDevice nub on generation 2 VMS for fake PCI root bridge (HyperVPCIRoot).

| Boot argument  | Description |
|----------------|-------------|
| -hvpcipdbg     | Enables debug printing in DEBUG builds

## PCI Root Bridge (HyperVPCIRoot)
Provides a fake PCI root bridge for proper macOS functionality on generation 2 VMs, and provides support for PCI passthrough.

| Boot argument  | Description |
|----------------|-------------|
| -hvpcirdbg     | Enables debug printing in DEBUG builds

## Shutdown (HyperVShutdown)
Provides software shutdown through Virtual Machine Connection and PowerShell. Requires the `hvshutdownd` userspace daemon to be running.

| Boot argument  | Description |
|----------------|-------------|
| -hvshutdbg     | Enables debug printing in DEBUG builds
| -hvshutmsgdbg  | Enables debug printing of message data in DEBUG builds
| -hvshutoff     | Disables this module

## Storage (HyperVStorage)
Provides SCSI storage support.

| Boot argument  | Description |
|----------------|-------------|
| -hvstordbg     | Enables debug printing in DEBUG builds
| -hvstormsgdbg  | Enables debug printing of message data in DEBUG builds
| -hvstoroff     | Disables this module

## Time Synchronization (HyperVTimeSync)
Provides host to guest time synchronization support. Requires the `hvtimesyncd` userspace daemon to be running.

| Boot argument  | Description |
|----------------|-------------|
| -hvtimedbg     | Enables debug printing in DEBUG builds
| -hvtimemsgdbg  | Enables debug printing of message data in DEBUG builds
| -hvtimeoff     | Disables this module

## VMBus Controller (HyperVVMBus)
Provides root of VMBus devices and services.

| Boot argument  | Description |
|----------------|-------------|
| -hvvmbusdbg    | Enables debug printing in DEBUG builds
| -hvvmbusnocpu  | Force all channels to use CPU 0 on Windows Server 2012 / Windows 8 and newer.

## VMBus Device Nub (HyperVVMBusDevice)
Provides connection nub for child VMBus device modules.

| Boot argument  | Description |
|----------------|-------------|
| -hvvmbusdebdbg | Enables debug printing in DEBUG builds
