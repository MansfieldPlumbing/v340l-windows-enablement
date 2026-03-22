# ADDENDUM 11 — DAY 2 RECONNAISSANCE
## AMD Radeon Pro V340L MxGPU Windows Activation Project
## Date: March 2026
## Status: Switchtec driver bound. Code 10. One registry key from MRPC surface.

---

## CITATION PROTOCOL

All topology claims derived from live hardware session.
Day 1 primary source: SURVEYDDA_OUTPUT.txt
Day 2 primary source: PowerShell session logs, Device Manager, sc.exe output.

---

## GATE STATUS UPDATE

| Gate | Question | Previous | New |
|---|---|---|---|
| 4 | Dual-die IOMMU grouping | CLOSED — independent | CLOSED |
| 5 | 686C on bare metal Win11 Pro | OPEN | OPEN |
| 7 | SCH register index | OPEN | OPEN |
| 10 | PnP surprise removal | OPEN | OPEN |
| **11** | **Switchtec driver bound to upstream port** | **OPEN** | **CLOSED — Switchtec service confirmed via DEVPKEY_Device_Service** |
| **12** | **Switchtec driver initializes (Code 10 resolved)** | **N/A** | **OPEN — missing WDF registry parameters** |

---

## DAY 2 FINDINGS

### GPU-Z Confirmation
Both dies confirmed alive via GPU-Z before any driver work:
- GPU #0: AMD Vega10 GLXT SERVER, PCIe x16 Gen3, HBM2 2048-bit, 3584 shaders
- GPU #1: Identical
- GPU #2: Zotac RTX 3090 (host display)
- Clock/memory/shader readings blank on both V340L dies — expected, no driver

### Windows 11 24H2 GUI Override Path — DEAD
The "Have Disk" + uncheck "Show compatible hardware" flow that worked on
Windows 10 and earlier Win11 builds is non-functional on Build 26100 (24H2).
Windows pre-filters INFs against class codes before allowing selection regardless
of the checkbox state. This path is not viable for any future driver work on
this build.

### pci.sys Driver Rank — CONFIRMED BLOCKER
pci.sys is an inbox WHQL driver and outranks all 3rd party drivers at CC_060400
via Windows driver ranking algorithm. pnputil /scan-devices will always rebind
the upstream port to pci.sys after remove/rescan cycle. Registry injection is
the only viable bind path.

### Management Endpoint (Function 1) — ABSENT
101:0.1 did not appear in PnP enumeration or Get-PnpDevice output. Three
VEN_11F8 devices enumerated, all CC_060400 (PCI-to-PCI Bridge). No CC_058000
or CC_068000 management endpoint visible to Windows. ADDENDUM10 "all FF /
absent" outcome confirmed. RWEverything scan still pending to determine if
endpoint is suppressed in eNVM or missed by Windows ARI enumeration.

### Switchtec Driver Bind — ACHIEVED VIA REGISTRY INJECTION
pnputil and GUI paths both failed. Successful bind achieved via:

1. Copy Switchtec.sys to C:\Windows\System32\drivers\
2. Create kernel service via sc.exe:
```
sc.exe create Switchtec type= kernel start= demand error= normal
  binPath= "\SystemRoot\System32\drivers\Switchtec.sys"
  DisplayName= "Switchtec Management Service"
```
3. Manually create class subkey:
```
HKLM\SYSTEM\CurrentControlSet\Control\Class\{1DED99DE-B4AE-442D-A180-B5FF691AE552}\0000
  DriverDesc    = "Switchtec PFX 48xG3 NTB EP"
  InfPath       = "oem61.inf"
  InfSection    = "Switchtec_Device.NT"
  ProviderName  = "MicroSemi Corporation"
  MatchingDeviceId = "PCI\VEN_11F8&DEV_8533"
```
4. Force device registry keys:
```
HKLM\SYSTEM\CurrentControlSet\Enum\PCI\VEN_11F8&DEV_8533&SUBSYS_BEEF11F8&REV_00\U500E004A0000000100
  Service   = "Switchtec"
  ClassGUID = "{1DED99DE-B4AE-442D-A180-B5FF691AE552}"
  Class     = "switch"
  Driver    = "{1DED99DE-B4AE-442D-A180-B5FF691AE552}\0000"
```
5. sc.exe start Switchtec → STATE: RUNNING
6. pnputil /restart-device → Code 10

DEVPKEY_Device_Service confirmed returning "Switchtec" — bind is real.

### Code 10 Root Cause — IDENTIFIED
sc.exe service creation bypasses the INF install process entirely. The INF
normally writes KMDF WDF parameters to the service registry key during
installation. Because we used sc.exe directly, these were never written.

Missing key:
```
HKLM\SYSTEM\CurrentControlSet\Services\Switchtec\Parameters\Wdf
  KmdfLibraryVersion = "1.15"
```

The Switchtec.inf specifies KmdfLibraryVersion = 1.15 in [Switchtec_wdfsect].
The KMDF runtime reads this from the registry at driver load. Without it,
DriverEntry fails and PnP reports Code 10.

### Switchtec CLI Location
Installer deploys to: C:\Program Files\Switchtec\switchtec.exe
switchtec.exe list returns empty until device interface is registered by
the driver. Device interface registration happens in EvtDriverDeviceAdd,
which only fires after DriverEntry succeeds. DriverEntry fails at Code 10.
Fix the WDF key, fix the Code 10, fix the list output. Sequential dependency.

---

## NEXT SESSION — EXACT STARTING POINT

Run these commands in order from an elevated PowerShell:

```powershell
# 1. Write the missing KMDF parameter
New-Item -Path "HKLM:\SYSTEM\CurrentControlSet\Services\Switchtec\Parameters\Wdf" -Force
Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Services\Switchtec\Parameters\Wdf" -Name "KmdfLibraryVersion" -Value "1.15"

# 2. Restart the device
pnputil /restart-device "PCI\VEN_11F8&DEV_8533&SUBSYS_BEEF11F8&REV_00\U500E004A0000000100"

# 3. Check status
Get-PnpDevice | Where-Object { $_.InstanceId -like "*U500E004A0000000100*" } | Select-Object Status, Class, FriendlyName
Get-PnpDevice | Where-Object { $_.InstanceId -like "*U500E004A0000000100*" } | Get-PnpDeviceProperty -KeyName DEVPKEY_Device_ProblemCode | Select-Object Data

# 4. If Code 10 is gone:
cd "C:\Program Files\Switchtec"
.\switchtec.exe list
```

Expected success output:
```
switchtec0    PFX 48xG3    ...
```

If Code 10 persists after the WDF key, check Event Viewer →
System log for Switchtec service errors to identify next failure mode.

---

## THERMAL STATUS
Woozoo PCFSC1ST globe fan pointed at P520 front intake — airflow confirmed
moving through passive V340L heatsink fin stack (air felt at rear bracket
with system powered off). Thermal shutdown risk assessed as acceptable for
Day 3. RTX 3090 remains in adjacent slot below V340L — waste heat contribution
is a known variable. Woozoo appears sufficient. Monitor temps if possible once
daemon is running.
