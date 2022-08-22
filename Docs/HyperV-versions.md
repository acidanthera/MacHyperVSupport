# Hyper-V supported features per version
These lists attempt to describe feature support, availability, and limits across different versions of Hyper-V.  
These lists are informational only and may not be completely correct.

## Hyper-V general versioning
| Windows Client   | Windows Server             | Kernel     | Config | VMBus | macOS Supported |
|------------------|----------------------------|------------|--------|-------|-----------------|
| N/A              | Windows Server 2008 SP2    | 6.0.6002   | 2.x    | 0.13  | No (see note 1)
| N/A              | Windows Server 2008 R2 SP1 | 6.1.7601   | 3.x    | 1.1   | No (see note 1)
| Windows 8        | Windows Server 2012        | 6.2.9200   | 4.0    | 2.4   | No (see note 1)
| Windows 8.1      | Windows Server 2012 R2     | 6.3.9600   | 5.0    | 3.0   | Yes (Gen2 only)
| Windows 10 v1507 | N/A                        | 10.0.10240 | 6.2    | 3.0   | Yes (Gen2 only)
| Windows 10 v1511 | Windows Server 2016 TP4    | 10.0.10586 | 7.0    | 3.0   | Yes (Gen2 only)
| Windows 10 v1607 | Windows Server 2016        | 10.0.14393 | 8.0    | 4.0   | No (see note 2)
| Windows 10 v1703 | N/A                        | 10.0.15063 | 8.1    | 4.0   | No (see note 2)
| Windows 10 v1709 | Windows Server v1709 (SAC) | 10.0.16299 | 8.2    | 4.1   | No (see note 2)
| Windows 10 v1803 | Windows Server v1803 (SAC) | 10.0.17134 | 8.3    | 5.1   | No (see note 2)
| Windows 10 v1809 | Windows Server 2019        | 10.0.17763 | 9.0    | 5.2   | Yes (Gen2 only)
| Windows 10 v1903 | Windows Server v1903 (SAC) | 10.0.18362 | 9.1    | 5.2   | Yes (Gen2 only)
| Windows 10 v1909 | Windows Server v1909 (SAC) | 10.0.18363 | 9.1    | 5.2   | Yes (Gen2 only)
| Windows 10 v2004 | Windows Server v2004 (SAC) | 10.0.19041 | 9.2    | 5.2   | Yes (Gen2 only)
| Windows 10 v20H2 | Windows Server v20H2 (SAC) | 10.0.19042 | 9.2    | 5.2   | Yes (Gen2 only)
| Windows 10 v21H1 | N/A                        | 10.0.19043 | 9.2    | 5.2   | Yes (Gen2 only)
| Windows 10 v21H2 | N/A                        | 10.0.19044 | 9.2    | 5.2   | Yes (Gen2 only)
| Windows 11 v21H2 | Windows Server 2022        | 10.0.22000 | 10.0   | 5.3   | Yes (Gen2 only)

### Notes
1. DUET currently does not function under Generation 1 VMs
2. These versions of Hyper-V do not function properly with OpenRuntime, causing the memory map to be trashed when loaded.

## General features
* Generation 2 VMs are only supported in Windows 8.1 or Windows Server 2012 R2, or later.

| Windows Client   | Windows Server             | Maximum CPUs           | UEFI (Gen2) | Secure Boot (Gen2) | DDA |
|------------------|----------------------------|------------------------|-------------|--------------------|-----|
| N/A              | Windows Server 2008 SP2    | 4                      | No          | No                 | No  |
| N/A              | Windows Server 2008 R2 SP1 | 4                      | No          | No                 | No  |
| Windows 8        | Windows Server 2012        | 64                     | No          | No                 | No  |
| Windows 8.1      | Windows Server 2012 R2     | 64                     | Yes         | Windows            | No  |
| Windows 10 v1507 | N/A                        | 64                     | Yes         | Windows            | No  |
| Windows 10 v1511 | Windows Server 2016 TP4    | 64                     | Yes         | Windows            | No  |
| Windows 10 v1607 | Windows Server 2016        | 64 (Gen1) / 240 (Gen2) | Yes         | Windows, Linux     | Yes |
| Windows 10 v1703 | N/A                        | 64 (Gen1) / 240 (Gen2) | Yes         | Windows, Linux     | Yes |
| Windows 10 v1709 | Windows Server v1709 (SAC) | 64 (Gen1) / 240 (Gen2) | Yes         | Windows, Linux     | Yes |
| Windows 10 v1803 | Windows Server v1803 (SAC) | 64 (Gen1) / 240 (Gen2) | Yes         | Windows, Linux     | Yes |
| Windows 10 v1809 | Windows Server 2019        | 64 (Gen1) / 240 (Gen2) | Yes         | Windows, Linux     | Yes |
| Windows 10 v1903 | Windows Server v1903 (SAC) | 64 (Gen1) / 240 (Gen2) | Yes         | Windows, Linux     | Yes |
| Windows 10 v1909 | Windows Server v1909 (SAC) | 64 (Gen1) / 240 (Gen2) | Yes         | Windows, Linux     | Yes |
| Windows 10 v2004 | Windows Server v2004 (SAC) | 64 (Gen1) / 240 (Gen2) | Yes         | Windows, Linux     | Yes |
| Windows 10 v20H2 | Windows Server v20H2 (SAC) | 64 (Gen1) / 240 (Gen2) | Yes         | Windows, Linux     | Yes |
| Windows 10 v21H1 | N/A                        | 64 (Gen1) / 240 (Gen2) | Yes         | Windows, Linux     | Yes |
| Windows 10 v21H2 | N/A                        | 64 (Gen1) / 240 (Gen2) | Yes         | Windows, Linux     | Yes |
| Windows 11 v21H2 | Windows Server 2022        | 64 (Gen1) / 240 (Gen2) | Yes         | Windows, Linux     | Yes |

## Memory features
| Windows Client   | Windows Server             | Maximum memory                | Dynamic memory | Hot add/remove memory |
|------------------|----------------------------|-------------------------------|----------------|-----------------------|
| N/A              | Windows Server 2008 SP2    | 32GB (STD/HV) / 64GB (ENT/DC) | No             | No                    |
| N/A              | Windows Server 2008 R2 SP1 | 32GB (STD) / 64GB (HV/ENT/DC) | Yes            | No                    |
| Windows 8        | Windows Server 2012        | 1TB                           | Yes            | No                    |
| Windows 8.1      | Windows Server 2012 R2     | 1TB                           | Yes            | No                    |
| Windows 10 v1507 | N/A                        | 1TB                           | Yes            | No                    |
| Windows 10 v1511 | Windows Server 2016 TP4    | 1TB                           | Yes            | No                    |
| Windows 10 v1607 | Windows Server 2016        | 1TB (Gen1) / 12TB (Gen2)      | Yes            | Yes                   |
| Windows 10 v1703 | N/A                        | 1TB (Gen1) / 12TB (Gen2)      | Yes            | Yes                   |
| Windows 10 v1709 | Windows Server v1709 (SAC) | 1TB (Gen1) / 12TB (Gen2)      | Yes            | Yes                   |
| Windows 10 v1803 | Windows Server v1803 (SAC) | 1TB (Gen1) / 12TB (Gen2)      | Yes            | Yes                   |
| Windows 10 v1809 | Windows Server 2019        | 1TB (Gen1) / 12TB (Gen2)      | Yes            | Yes                   |
| Windows 10 v1903 | Windows Server v1903 (SAC) | 1TB (Gen1) / 12TB (Gen2)      | Yes            | Yes                   |
| Windows 10 v1909 | Windows Server v1909 (SAC) | 1TB (Gen1) / 12TB (Gen2)      | Yes            | Yes                   |
| Windows 10 v2004 | Windows Server v2004 (SAC) | 1TB (Gen1) / 12TB (Gen2)      | Yes            | Yes                   |
| Windows 10 v20H2 | Windows Server v20H2 (SAC) | 1TB (Gen1) / 12TB (Gen2)      | Yes            | Yes                   |
| Windows 10 v21H1 | N/A                        | 1TB (Gen1) / 12TB (Gen2)      | Yes            | Yes                   |
| Windows 10 v21H2 | N/A                        | 1TB (Gen1) / 12TB (Gen2)      | Yes            | Yes                   |
| Windows 11 v21H2 | Windows Server 2022        | 1TB (Gen1) / 12TB (Gen2)      | Yes            | Yes                   |

## Storage features
* All versions have the following limitations for disks:
  * 2 IDE controllers (2 disks per controller, 4 disks total)
  * 4 Synthetic SCSI controllers (64 disks per controller, 256 disks total)

| Windows Client   | Windows Server             | Protocol | Hot add/remove | Fibre Channel | VHDX | Online VHDX resize | QoS | NVDIMM | 
|------------------|----------------------------|----------|----------------|---------------|------|--------------------|-----|--------|
| N/A              | Windows Server 2008 SP2    | 2.0      | No             | No            | No   | No                 | No  | No     |
| N/A              | Windows Server 2008 R2 SP1 | 4.2      | Yes            | No            | No   | No                 | No  | No     |
| Windows 8        | Windows Server 2012        | 5.1      | Yes            | Yes, 4 max    | Yes  | No                 | No  | No     |
| Windows 8.1      | Windows Server 2012 R2     | 6.0      | Yes            | Yes, 4 max    | Yes  | Yes                | Yes | No     |
| Windows 10 v1507 | N/A                        | 6.2      | Yes            | Yes, 4 max    | Yes  | Yes                | Yes | No     |
| Windows 10 v1511 | Windows Server 2016 TP4    | 6.2      | Yes            | Yes, 4 max    | Yes  | Yes                | Yes | No     |
| Windows 10 v1607 | Windows Server 2016        | 6.2      | Yes            | Yes, 4 max    | Yes  | Yes                | Yes | No     |
| Windows 10 v1703 | N/A                        | 6.2      | Yes            | Yes, 4 max    | Yes  | Yes                | Yes | No     |
| Windows 10 v1709 | Windows Server v1709 (SAC) | 6.2      | Yes            | Yes, 4 max    | Yes  | Yes                | Yes | No     |
| Windows 10 v1803 | Windows Server v1803 (SAC) | 6.2      | Yes            | Yes, 4 max    | Yes  | Yes                | Yes | No     |
| Windows 10 v1809 | Windows Server 2019        | 6.2      | Yes            | Yes, 4 max    | Yes  | Yes                | Yes | Yes    |
| Windows 10 v1903 | Windows Server v1903 (SAC) | 6.2      | Yes            | Yes, 4 max    | Yes  | Yes                | Yes | Yes    |
| Windows 10 v1909 | Windows Server v1909 (SAC) | 6.2      | Yes            | Yes, 4 max    | Yes  | Yes                | Yes | Yes    |
| Windows 10 v2004 | Windows Server v2004 (SAC) | 6.2      | Yes            | Yes, 4 max    | Yes  | Yes                | Yes | Yes    |
| Windows 10 v20H2 | Windows Server v20H2 (SAC) | 6.2      | Yes            | Yes, 4 max    | Yes  | Yes                | Yes | Yes    |
| Windows 10 v21H1 | N/A                        | 6.2      | Yes            | Yes, 4 max    | Yes  | Yes                | Yes | Yes    |
| Windows 10 v21H2 | N/A                        | 6.2      | Yes            | Yes, 4 max    | Yes  | Yes                | Yes | Yes    |
| Windows 11 v21H2 | Windows Server 2022        | 6.2      | Yes            | Yes, 4 max    | Yes  | Yes                | Yes | Yes    |

## Networking features
* Versions before Windows Server 2019 have the following limitations for network adapters:
  * 4 legacy (Gen1 only)
  * 8 synthetic
* Windows Server 2019 and newer have the following limitations for network adapters:
  * 4 legacy (Gen1 only)
  * 64 synthetic

| Windows Client   | Windows Server             | Protocol | NDIS | TCP offload | UDP offload | VMQ | Jumbo frames | SR-IOV | IPsec offload | RSS | Hot add/remove |
|------------------|----------------------------|----------|------|-------------|-------------|-----|--------------|--------|---------------|-----|----------------|
| N/A              | Windows Server 2008 SP2    |          |      |             |             | No  | No           | No     | No            | No  | No             |
| N/A              | Windows Server 2008 R2 SP1 |          |      |             |             | Yes | Yes          | No     | No            | No  | No             |
| Windows 8        | Windows Server 2012        |          |      |             |             | Yes | Yes          | Yes    | Yes           | No  | No             |
| Windows 8.1      | Windows Server 2012 R2     |          |      |             |             | Yes | Yes          | Yes    | Yes           | Yes | No             |
| Windows 10 v1507 | N/A                        |          |      |             |             | Yes | Yes          | Yes    | Yes           | Yes | No             |
| Windows 10 v1511 | Windows Server 2016 TP4    |          |      |             |             | Yes | Yes          | Yes    | Yes           | Yes | No             |
| Windows 10 v1607 | Windows Server 2016        |          |      |             |             | Yes | Yes          | Yes    | Yes           | Yes | Yes            |
| Windows 10 v1703 | N/A                        |          |      |             |             | Yes | Yes          | Yes    | Yes           | Yes | Yes            |
| Windows 10 v1709 | Windows Server v1709 (SAC) |          |      |             |             | Yes | Yes          | Yes    | Yes           | Yes | Yes            |
| Windows 10 v1803 | Windows Server v1803 (SAC) |          |      |             |             | Yes | Yes          | Yes    | Yes           | Yes | Yes            |
| Windows 10 v1809 | Windows Server 2019        |          |      |             |             | Yes | Yes          | Yes    | Yes           | Yes | Yes            |
| Windows 10 v1903 | Windows Server v1903 (SAC) |          |      |             |             | Yes | Yes          | Yes    | Yes           | Yes | Yes            |
| Windows 10 v1909 | Windows Server v1909 (SAC) |          |      |             |             | Yes | Yes          | Yes    | Yes           | Yes | Yes            |
| Windows 10 v2004 | Windows Server v2004 (SAC) |          |      |             |             | Yes | Yes          | Yes    | Yes           | Yes | Yes            |
| Windows 10 v20H2 | Windows Server v20H2 (SAC) |          |      |             |             | Yes | Yes          | Yes    | Yes           | Yes | Yes            |
| Windows 10 v21H1 | N/A                        |          |      |             |             | Yes | Yes          | Yes    | Yes           | Yes | Yes            |
| Windows 10 v21H2 | N/A                        |          |      |             |             | Yes | Yes          | Yes    | Yes           | Yes | Yes            |
| Windows 11 v21H2 | Windows Server 2022        |          |      |             |             | Yes | Yes          | Yes    | Yes           | Yes | Yes            |

## Integration services
| Windows Client   | Windows Server             | Shutdown | Time sync | KVP | Heartbeat | VSS | File Copy | AVMA | Enhanced Session | PowerShell Direct | Battery status |
|------------------|----------------------------|----------|-----------|-----|-----------|-----|-----------|------|------------------|-------------------|----------------|
| N/A              | Windows Server 2008 SP2    | Yes      | Yes       | Yes | Yes       | Yes | No        | No   | No               | No                | No             |
| N/A              | Windows Server 2008 R2 SP1 | Yes      | Yes       | Yes | Yes       | Yes | No        | No   | No               | No                | No             |
| Windows 8        | Windows Server 2012        | Yes      | Yes       | Yes | Yes       | Yes | No        | No   | No               | No                | No             |
| Windows 8.1      | Windows Server 2012 R2     | Yes      | Yes       | Yes | Yes       | Yes | Yes       | Yes  | Yes              | No                | No             |
| Windows 10 v1507 | N/A                        | Yes      | Yes       | Yes | Yes       | Yes | Yes       | Yes  | Yes              | Yes               | No             |
| Windows 10 v1511 | Windows Server 2016 TP4    | Yes      | Yes       | Yes | Yes       | Yes | Yes       | Yes  | Yes              | Yes               | No             |
| Windows 10 v1607 | Windows Server 2016        | Yes      | Yes       | Yes | Yes       | Yes | Yes       | Yes  | Yes              | Yes               | No             |
| Windows 10 v1703 | N/A                        | Yes      | Yes       | Yes | Yes       | Yes | Yes       | Yes  | Yes              | Yes               | No             |
| Windows 10 v1709 | Windows Server v1709 (SAC) | Yes      | Yes       | Yes | Yes       | Yes | Yes       | Yes  | Yes              | Yes               | Yes            |
| Windows 10 v1803 | Windows Server v1803 (SAC) | Yes      | Yes       | Yes | Yes       | Yes | Yes       | Yes  | Yes              | Yes               | Yes            |
| Windows 10 v1809 | Windows Server 2019        | Yes      | Yes       | Yes | Yes       | Yes | Yes       | Yes  | Yes              | Yes               | Yes            |
| Windows 10 v1903 | Windows Server v1903 (SAC) | Yes      | Yes       | Yes | Yes       | Yes | Yes       | Yes  | Yes              | Yes               | Yes            |
| Windows 10 v1909 | Windows Server v1909 (SAC) | Yes      | Yes       | Yes | Yes       | Yes | Yes       | Yes  | Yes              | Yes               | Yes            |
| Windows 10 v2004 | Windows Server v2004 (SAC) | Yes      | Yes       | Yes | Yes       | Yes | Yes       | Yes  | Yes              | Yes               | Yes            |
| Windows 10 v20H2 | Windows Server v20H2 (SAC) | Yes      | Yes       | Yes | Yes       | Yes | Yes       | Yes  | Yes              | Yes               | Yes            |
| Windows 10 v21H1 | N/A                        | Yes      | Yes       | Yes | Yes       | Yes | Yes       | Yes  | Yes              | Yes               | Yes            |
| Windows 10 v21H2 | N/A                        | Yes      | Yes       | Yes | Yes       | Yes | Yes       | Yes  | Yes              | Yes               | Yes            |
| Windows 11 v21H2 | Windows Server 2022        | Yes      | Yes       | Yes | Yes       | Yes | Yes       | Yes  | Yes              | Yes               | Yes            |
