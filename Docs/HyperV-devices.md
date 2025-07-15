# Hyper-V devices
This page describes the different services and devices exposed to a virtual machine over the VMBus.

### Automatic virtual machine activation (AVMA)
[Provides](https://learn.microsoft.com/en-us/windows-server/get-started/automatic-vm-activation) an activation method for Windows Server guests on Windows Server Datacenter hosts.  
* Availability: Windows Server 2012 R2 and later (Datacenter edition only)
* GUID: `3375baf4-9e15-4b30-b765-67acb10d607b`
* Support status: No

### Backup (volume snapshot)
[Provides](https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/reference/integration-services#hyper-v-volume-shadow-copy-requestor) Volume Shadow Copy support to guests. 
* Availability: Windows Server 2008 SP2 and later
* GUID: `35fa2e29-ea23-4236-96ae-3a6ebacba440`
* Support status: Handled by HyperVSnapshot

### Data Exchange
[Provides](https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/reference/integration-services#hyper-v-data-exchange-service-kvp) a mechanism to exchange basic metadata between the guest and the host via a key/value pair system.
* Availability: Windows Server 2008 and later
* GUID: `a9a0f4e7-5a45-4d96-b827-8a841e8c03e6`
* Support status: Planned

### Dynamic memory
[Provides](https://learn.microsoft.com/en-us/previous-versions/windows/it-pro/windows-server-2012-R2-and-2012/hh831766(v=ws.11)) overprovisioning of memory in guests. 
* Availability: Windows Server 2008 R2 SP1 and later
* GUID: `525074dc-8985-46e2-8057-a307dc18a502`
* Support status: No

### Guest services
[Provides](https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/reference/integration-services#hyper-v-guest-service-interface) an interface to allow for files to be copied to/from a guest and the host.  
* Availability: Windows Server 2008 and later
* GUID: `34d14be3-dee4-41c8-9ae7-6b174977c192`
* Support status: Handled by HyperVFileCopy

### Heartbeat
[Provides](https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/reference/integration-services#hyper-v-heartbeat-service) an interface to allow the host to ensure the guest has booted and operating correctly.  
* Availability: Windows Server 2008 and later
* GUID: `57164f39-9115-4e78-ab55-382f3bd5422d`
* Support status: Handled by HyperVHeartbeat

### Operating system shutdown
[Provides](https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/reference/integration-services#hyper-v-guest-shutdown-service) an interface to allow the host to gracefully shutdown or restart the guest.  
* Availability: Windows Server 2008 and later
* GUID: `0e0b6031-5213-4934-818b-38d90ced39db`
* Support status: Handled by HyperVShutdown

### PCI passthrough (Discrete Device Assignment)
[Provides](https://learn.microsoft.com/en-us/windows-server/virtualization/hyper-v/deploy/deploying-graphics-devices-using-dda) PCI passthrough support to a guest.
* Availability: Windows Server 2016 and later
* GUID: `44c4f61d-4444-4400-9d52-802e27ede19f`
* Support status: Handled by HyperVPCIBridge

### Remote Desktop control channel
* GUID: `f8e65716-3cb3-4a06-9a60-1889c5cccab5`
* Support status: No

### Remote Desktop virtualization
* GUID: `276aacf4-ac15-426c-98dd-7521ad3f01fe`
* Support status: No

### Synthetic Fibre Channel adapter
[Provides](https://learn.microsoft.com/en-us/previous-versions/windows/it-pro/windows-server-2012-R2-and-2012/hh831413(v=ws.11)) a virtual interface to connect to Fibre Channel storage devices from a guest.
* Availability: Windows Server 2012 and later
* GUID: `2f9bcc4a-0069-4af3-b76b-6fd0be528cda`
* Support status: No

### Synthetic graphics framebuffer
Provides virtual graphics framebuffer support.
* Availability: Windows Server 2008 and later
* GUID: `da0a7802-e377-4aac-8e77-0558eb1073f8`
* Support status: Handled by HyperVGraphics (requires HyperVFramebuffer for full functionality)

### Synthetic IDE controller
[Provides](https://learn.microsoft.com/en-us/windows-server/administration/performance-tuning/role/hyper-v-server/storage-io-performance#ide) enhanced performance over the emulated IDE controller in a Gen1 guest. Virtual hard disks will appear on both the emulated controller and the synthetic controller, CD devices will appear only on the emulated controller.
* Availability: Windows Server 2008 and later (Gen1 only)
* GUID: `32412632-86cb-44a2-9b5c-50d1417354f5`
* Support status: Handled by HyperVStorage

### Synthetic keyboard
Provides virtual keyboard support.
* Availability: Windows Server 2012 and later
* GUID: `f912ad6d-2b17-48ea-bd65-f927a61c7684`
* Support status: Handled by HyperVKeyboard

### Synthetic mouse
Provides virtual mouse support.
* Availability: Windows Server 2008 and later
* GUID: `cfa8b69e-5b4a-4cc0-b98b-8ba1a1f3f95a`
* Support status: Handled by HyperVMouse

### Synthetic network controller
Provides virtual network support.
* Availability: Windows Server 2008 and later
* GUID: `f8615163-df3e-46c5-913f-f2d2f965ed0e`
* Support status: Handled by HyperVNetwork

### Synthetic RDMA
* Availability: ?
* GUID: `8c2eaf3d-32a7-4b09-ab99-bd1f1c86b501`
* Support status: No

### Synthetic SCSI controller
Provides virtual SCSI storage support. Booting from the SCSI controller requires a Gen2 VM.
* Availability: Windows Server 2008 and later
* GUID: `ba6163d9-04a1-4d29-b605-72e2ffb1dc7f`
* Support status: Handled by HyperVStorage

### Time synchronization
[Provides](https://learn.microsoft.com/en-us/virtualization/hyper-v-on-windows/reference/integration-services#hyper-v-time-synchronization-service) an interface for time synchronization between the guest and the host.
* Availability: Windows Server 2008 and later
* GUID: `9527e630-d0ae-497b-adce-e80ab0175caf`
* Support status: Handled by HyperVTimeSync
