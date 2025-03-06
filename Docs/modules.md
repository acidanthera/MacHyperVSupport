# Driver Module List

## ACPI Fixup (HyperVACPIFixup)
Prevents ACPI0007 device objects from being enumerated by AppleACPIPlatform in macOS 10.6 and older.

| Boot argument  | Description |
|----------------|-------------|
| -hvacpifixdbg  | Enables debug printing in DEBUG builds

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

## Graphics (HyperVGraphics)
Provides graphics support.

| Boot argument  | Description |
|----------------|-------------|
| -hvgfxdbg      | Enables debug printing in DEBUG builds
| -hvgfxmsgdbg   | Enables debug printing of message data in DEBUG builds
| -hvgfxoff      | Disables this module

## Graphics Bridge (HyperVGraphicsBridge)
Provides basic graphics support for macOS in generation 2 VMs.

| Boot argument  | Description |
|----------------|-------------|
| -hvgfxbdbg     | Enables debug printing in DEBUG builds
| -hvgfxbmsgdbg  | Enables debug printing of message data in DEBUG builds
| -hvgfxboff     | Disables this module

## Graphics Framebuffer (HyperVGraphicsFramebuffer)
Provides enhanced graphics support (resolution switching and hardware cursor) for macOS.
This module is contained in the separate MacHyperVFramebuffer.kext kernel extension.

| Boot argument  | Description |
|----------------|-------------|
| -hvgfxfbdbg    | Enables debug printing in DEBUG builds
| -hvgfxfboff    | Disables this module

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

## Keyboard (PS/2) (HyperVPS2Keyboard)
Provides PS/2 keyboard support in generation 1 VMs.

| Boot argument  | Description |
|----------------|-------------|
| -hvps2kbddbg   | Enables debug printing in DEBUG builds
| -hvps2kbdoff   | Disables this module

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
| -hvnetdatadbg  | Enables debug printing of packet data in DEBUG builds
| -hvnetmsgdbg   | Enables debug printing of message data in DEBUG builds
| -hvnetoff      | Disables this module

## PCI Bridge (HyperVPCIBridge)
Provides PCI passthrough support.

| Boot argument  | Description |
|----------------|-------------|
| -hvpcibdbg     | Enables debug printing in DEBUG builds
| -hvpcibmsgdbg  | Enables debug printing of message data in DEBUG builds
| -hvpciboff     | Disables this module

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

## Snapshot (HyperVSnapshot)
Provides support for consistent backup support and production checkpoints. Requires the `hvsnapshotd` userspace daemon to be running.

| Boot argument  | Description |
|----------------|-------------|
| -hvsnapdbg     | Enables debug printing in DEBUG builds
| -hvsnapuserdbg | Enables debug printing of user client data in DEBUG builds
| -hvsnapmsgdbg  | Enables debug printing of message data in DEBUG builds
| -hvsnapoff     | Disables this module

## Storage (HyperVStorage)
Provides SCSI storage support.

| Boot argument  | Description |
|----------------|-------------|
| -hvstordbg     | Enables debug printing in DEBUG builds
| -hvstordatadbg | Enables debug printing of packet data in DEBUG builds
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

## VMBus Device Nub (HyperVVMBusDevice)
Provides connection nub for child VMBus device modules.

| Boot argument  | Description |
|----------------|-------------|
| -hvvmbusdebdbg | Enables debug printing in DEBUG builds
