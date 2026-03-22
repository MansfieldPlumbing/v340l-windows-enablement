###### V340L RESEARCH BRIEF v12 — UNIFIED MASTER DOCUMENT

###### AMD Radeon Pro V340L MxGPU Windows Activation Project

###### Date: March 2026

###### Status: DAY 1 EMPIRICAL — Hardware alive, topology mapped. Activation testing in progress.
###### Switchtec endpoint and dual-die PCIe topology confirmed on Windows.
###### No community precedent exists for any of the three paths.
###### All gates marked CLOSED are confirmed from primary source or live hardware observation.
###### Activation sequence not yet fired — thermal shutdown terminated Day 1 before attempt.

###### Changes over v11:

###### 1\. OQ-3 CLOSED — switchtec-kmdf-0.6 confirmed in Intel AIC package (S64). WHQL-signed. DriverVer 04/17/2018,11.21.4.279.

###### 2\. NVMe dependency CONFIRMED FIXED — verbatim from Firmware Release Notes.txt. Load-bearing for V340L: no external NVMe slots exist on this card.

###### 3\. GFMS\_BIND payload structure CLOSED — full wire format confirmed from switchtec-user lib/switchtec.c. See SWITCHTEC.md.

###### 4\. PDFID discovery CLOSED — gfms\_dump reports pdfid\_start/pdfid\_end per port. All bind field values discoverable at runtime pre-activation.

###### 5\. fabric CLI Linux-only CONFIRMED — entire fabric subcommand group is \#ifdef \_\_linux\_\_ in cli/fabric.c. Pre-built exe cannot send GFMS commands on Windows. Daemon must be compiled from source.

###### 6\. GIM Vega source CONFIRMED UNAVAILABLE — GPUOpen MxGPU-Virtualization archived April 2025, S7150/Tonga only. No open-source Vega GIM exists. MMIO sequence in Section 7 derived from AMD kernel headers — unaffected.

###### 7\. New gate added — MMIO required from host: OPEN — Day 1 empirical. Fire gfms\_bind only first. See Section 8 Path C.

###### 8\. S64 added to source registry — Intel AIC package, PRIMARY SOURCE IN HAND.

###### 9\. SWITCHTEC.md added as companion document — primary source record for all Switchtec findings.

###### 10\. Section 12 updated — methodology note added distinguishing AI-assisted analysis from AI-discovered findings.

\---

## SECTION 1 — PROJECT STATE

The V340L MxGPU Windows Activation Project is a reverse-engineering effort
to enable a $50 enterprise GPU (AMD Radeon Pro V340L, 16GB HBM2, dual
Vega10 dies) to operate as a fully functional inference device under Windows
Server 2022/2025. The activation is a stateless, software-only sequence
targeting volatile SRAM — zero brick risk throughout.

**Three activation paths are now defined:**

**Path A — v340ctl KMDF (original):** Windows Server host, KMDF kernel
driver replicates GIM activation sequence, DEV\_686C VFs passed via
Hyper-V DDA into Windows guest with 21.Q4 WX 8200 driver (Vulkan 1.2).
Novel. Fully specified. No precedent in community.

**Path B — ESXi nested via DDA (new, fastest to validate):** Windows Server
host passes raw 6864 PFs via Hyper-V DDA into nested ESXi guest VM.
ESXi VIB (amdgpuv 2.0.8) handles MxGPU activation natively. 686C VFs
appear inside ESXi and are assigned to Windows VMs with the official
19.Q2 V340 guest driver. AMD and Lenovo co-certified this exact hardware
combination. P520 + V340L is the reference platform per VIB descriptor.

**Path C — Switchtec userspace daemon (confirmed architecture, unvalidated):**
switchtec-kmdf-0.6 confirmed in Intel AIC package (S64) — WHQL-signed,
covers VEN\_11F8&DEV\_854x&CC\_058000, NVMe dependency confirmed fixed.
A daemon compiled against libswitchtec calls switchtec\_gfms\_bind() to
open fabric routing, then services the GPU mailbox protocol. Full GFMS\_BIND
wire format confirmed from lib/switchtec.c. The fabric CLI subcommand group
is Linux-only (#ifdef \_\_linux\_\_) — daemon must be compiled from source.
OPEN: whether gfms\_bind alone triggers DEV\_686C or whether MMIO sequence
is also required from host. See Section 8 and SWITCHTEC.md.

**Pre-hardware research is complete for all three paths.**
All implementation specifications are fully defined from primary sources.
No path has been validated on hardware. Day 1 will determine which path
is viable and in what order to attempt them.

Install card. Run Day 1 sequence. Observe what enumerates.
Follow the decision tree in Section 9.

\---

## SECTION 2 — LOCKED PHYSICAL TOPOLOGY

**Status: LOCKED — DO NOT RE-RESEARCH OR SUGGEST ALTERNATIVES**

|Property|Value|
|-|-|
|System|Lenovo ThinkStation P520|
|CPU|Intel Xeon W-2145 (Skylake-W, 8c/16t, 3.7GHz base, 4.5GHz turbo)|
|PCIe lanes|48 PCIe 3.0 lanes, CPU-direct|
|Chipset|Intel C422 — native ACS, ARI, full ECAM on all slots|
|RAM|4-channel DDR4-2666 ECC, 85.3 GB/s|
|Physical addressing|46-bit|
|OS|Windows Server 2022 + Hyper-V (upgrade to 2025 planned — see OQ-1)|
|Internal PSU|900W|
|Primary GPU|RTX 3090, Slot 2, PCIe 3.0 x16, CPU-direct — host display only|
|V340L Deployment|Passive PCIe 3.0 mining riser → P520 PCIe x4/x8 slot|
|Negotiated link|x1 or x4 — acceptable for inference load profile|
|Dedicated PSU|EVGA SuperNOVA 1300W G2 Gold — V340L and riser only|
|Thermals|External to chassis, dedicated blower on V340L shroud|

**P520 is the reference platform.** AMD and Lenovo co-certified the V340L
on the ThinkStation P520. The amdgpuv VIB descriptor explicitly lists
Lenovo subsystem ID 17AA0C00 alongside AMD subsystem ID 0C001002. Both
are the same 0C00 device ID — one reports Lenovo as subsystem vendor, one
AMD. Which your specific card reports is determined empirically on Day 1.
Either way, both IDs are in the VIB. You are on the reference platform.

**Note on brief v5 topology:** Brief v5 describes V340L in Slot 2 native
x16, no riser. Actual deployment uses passive riser to x4/x8 slot on
external PSU. Link width degrades to x1/x4. This is acceptable: model
loading is one-time across the riser, inference is bound by on-die HBM2
(484 GB/s per die) thereafter. Do not re-derive lane counts from the brief.

\---

## SECTION 3 — UNIFIED GATE TABLE

|Gate|Question|Status|
|-|-|-|
|1|gfx900 mailbox register offsets|**CLOSED** — nbio\_6\_1\_offset.h|
|2|COMM\_BLK accessible without VIB|**CLOSED** — Microchip S24|
|3|CONTROL / TRN\_DW3 address overlap|**CLOSED** — Disproved|
|4|Dual-die IOMMU grouping on P520|**CLOSED** — Independent. Separate downstream switch ports. BDFs 105:0.0 and 108:0.0. SURVEYDDA_OUTPUT.txt|
|5|686C loads in Hyper-V DDA guest|OPEN — first driver load|
|6|21.Q4 WX 8200 Vulkan 1.2|**CLOSED** — api\_version 1.2.196 confirmed|
|7|World switch timer SCH register index|OPEN — MmMapIoSpaceEx read on arrival|
|8|FB size write target and encoding|**CLOSED** — gim\_gpuiov.c + gim\_pci.h|
|9|GIM mailbox opcode sequence|**CLOSED** — mxgpu\_ai.c + mxgpu\_ai.h|
|10|Windows PnP surprise removal on 6864→686C|OPEN — empirical|
|11|Guest driver heartbeat architecture|**CLOSED** — mxgpu\_ai.c|
|12|vBIOS required at VF FB offset 0|**CLOSED** — amdgv\_sriovmsg.h|
|13|PF2VF struct required at VF FB + 64KB|**CLOSED** — amdgv\_sriovmsg.h|
|14|Checksum computation algorithm|**CLOSED** — amdgpu\_virt.c fetched|
|15|GPUIOV command opcodes|**CLOSED** — gim\_gpuiov.h|
|16|PCI\_GPUIOV\_\* register offsets|**CLOSED** — gim\_pci.h|
|—|Switchtec endpoint visible in Windows|**CLOSED** — 101:0.0 upstream bridge, pci.sys. PFX 48xG3 DEV_8533. SURVEYDDA_OUTPUT.txt|
|—|switchtec-kmdf availability|**CLOSED** — switchtec-kmdf-0.6 in Intel AIC package S64|
|—|V340L DEV ID in INF coverage range|**CLOSED** — Actual DEV_8533 (PFX 48xG3). Prediction was DEV_8543 (PSX). Confirmed covered by INF. SURVEYDDA_OUTPUT.txt|
|—|switchtec-kmdf-0.6 binds on Windows 11|OPEN — thermal shutdown before driver bind attempt. Next session.|
|—|GFMS\_BIND payload structure|**CLOSED** — lib/switchtec.c confirmed verbatim. See SWITCHTEC.md|
|—|PDFID discovery method|**CLOSED** — gfms\_dump reports pdfid\_start/pdfid\_end per port|
|—|fabric CLI available on Windows|**CLOSED NEGATIVE** — \#ifdef \_\_linux\_\_ cli/fabric.c. Daemon required|
|—|GIM Vega source available|**CLOSED NEGATIVE** — GPUOpen archived Apr 2025. S7150 only|
|—|MMIO sequence required from host|OPEN — Day 1 empirical. Fire gfms\_bind first, observe 686C|
|—|Switchtec factory state partition init|OPEN — Low risk, eNVM pre-populates|
|—|Multi-card BDF ordering|OPEN — Day 2|

\---

## SECTION 4 — PRE-CARD RESEARCH ITEMS

**Gate 6 is CLOSED.** amd-vulkan64.json from 21.Q4 WX 8200 Win10 package
confirms api\_version 1.2.196. llama.cpp Vulkan backend available.

**Remaining pre-card items:**

**1. OQ-1 — Server 2025 DDA Confirmation**
Status: INFERRED HIGH — no primary source fetch yet.
Fetch Microsoft Server 2025 Hyper-V DDA documentation.
Confirm: Dismount-VMHostAssignableDevice / Add-VMAssignableDevice
unchanged, no new device class restrictions, 686C passes DDA eligibility,
INF-edited 21.Q4 driver still installable in 2025 guest.
Target: https://learn.microsoft.com/en-us/windows-server/virtualization/hyper-v/deploy/deploying-graphics-devices-using-dda

**2. ESXi ISO for P520**
Download Lenovo-customized ESXi 6.5 U1 ISO:
VMware-ESXi-6.5.0.update01-7388607-LNV-20180201.iso
This is the ISO used in the Lenovo deployment guide. Lenovo customized
ESXi images include P520-specific drivers not in the generic VMware ISO.

**3. Switchtec Windows build test (MSYS2)**
Install MSYS2. Clone MansfieldPlumbing/switchtec-user.
Attempt configure/make in MINGW64 shell per upstream README.
Document what builds and what fails. This is groundwork for Path C
regardless of whether the Switchtec endpoint is visible in Device Manager.

**4. v340ctl KMDF Driver Skeleton (Path A)**
Begin writing minimal KMDF driver per Section 6 architecture.
WDK PCIDRV sample as reference. Compile and test-sign before card arrival.

**5. vBIOS Fallback Binary**
Source pre-extracted vBIOS from TechPowerUp VGA BIOS Collection.
Search Radeon Pro WX 8200 or Radeon Instinct MI25 (same gfx900 silicon).

\---

## SECTION 5 — CONFIRMED REGISTER VALUES AND CONSTANTS

**ALL HEX VALUES IN THIS SECTION ARE IMMUTABLE.
Confirmed character-for-character from primary source headers.
Do not reorder, convert, or paraphrase any numeric value.**

### NBIO 6.1 Mailbox Registers

**Source: nbio\_6\_1\_offset.h — PRIMARY SOURCE IN HAND**
All offsets are DWORD offsets. Byte address = NBIO\_BAR\_base + (offset × 4)

|Register|DWORD Offset|Byte Offset|Notes|
|-|-|-|-|
|mmMAILBOX\_INDEX|0x0135|0x04D4|Selects mailbox slot|
|mmBIF\_BX\_PF0\_MAILBOX\_MSGBUF\_TRN\_DW0|0x0136|0x04D8|Request opcode|
|mmBIF\_BX\_PF0\_MAILBOX\_MSGBUF\_TRN\_DW1|0x0137|0x04DC|always 0|
|mmBIF\_BX\_PF0\_MAILBOX\_MSGBUF\_TRN\_DW2|0x0138|0x04E0|always 0|
|mmBIF\_BX\_PF0\_MAILBOX\_MSGBUF\_TRN\_DW3|0x0139|0x04E4|always 0|
|mmBIF\_BX\_PF0\_MAILBOX\_MSGBUF\_RCV\_DW0|0x013A|0x04E8|Response opcode|
|mmBIF\_BX\_PF0\_MAILBOX\_MSGBUF\_RCV\_DW1|0x013B|0x04EC||
|mmBIF\_BX\_PF0\_MAILBOX\_MSGBUF\_RCV\_DW2|0x013C|0x04F0|checksum\_key written here|
|mmBIF\_BX\_PF0\_MAILBOX\_MSGBUF\_RCV\_DW3|0x013D|0x04F4||
|mmBIF\_BX\_PF0\_MAILBOX\_CONTROL|0x013E|0x04F8|Byte-write control|
|mmBIF\_BX\_PF0\_MAILBOX\_INT\_CNTL|0x013F|0x04FC|Interrupt control|
|mmBIF\_BX\_PF0\_BIF\_VMHV\_MAILBOX|0x0140|0x0500|Hypervisor mailbox|

**Mailbox Control Byte Addresses (from mxgpu\_ai.h):**

```
TRN\\\_OFFSET\\\_BYTE = mmBIF\\\_BX\\\_PF0\\\_MAILBOX\\\_CONTROL \\\* 4     = 0x04F8
RCV\\\_OFFSET\\\_BYTE = mmBIF\\\_BX\\\_PF0\\\_MAILBOX\\\_CONTROL \\\* 4 + 1 = 0x04F9
```

TRN byte at 0x04F8: bit 0 = valid, bit 1 = ack received
RCV byte at 0x04F9: write 0x01 to assert valid, write 0x02 to ack

\---

### IOV Function Identifier

**Source: amdgpu\_virt.h — PRIMARY SOURCE IN HAND**

|Register|Absolute DWORD Offset|Notes|
|-|-|-|
|mmRCC\_IOV\_FUNC\_IDENTIFIER|0x0DE5|Used in capability walk|

\---

### GC Block SR-IOV Registers

**Source: gc\_9\_0\_offset.h — PRIMARY SOURCE IN HAND**

|Register|DWORD Offset|Notes|
|-|-|-|
|mmRLC\_GPU\_IOV\_VF\_ENABLE|0x5b00|Write 1 to enable VF scheduling|
|mmRLC\_GPU\_IOV\_SCH\_0|0x5b38|World switch timer candidate|
|mmRLC\_GPU\_IOV\_ACTIVE\_FCN\_ID|0x5b39|Active VF ID|
|mmRLC\_GPU\_IOV\_SCH\_3|0x5b3a|World switch timer candidate|
|mmRLC\_GPU\_IOV\_SCH\_1|0x5b3b|World switch timer candidate|
|mmRLC\_GPU\_IOV\_SCH\_2|0x5b3c|World switch timer candidate|

**CRITICAL: SCH register names are NOT sequential by offset.**
SCH\_3 is at 0x5b3a. SCH\_1 is at 0x5b3b. SCH\_2 is at 0x5b3c.
Confirmed from gc\_9\_0\_offset.h. Do not reorder alphabetically.
Gate 7: read all four before activation, read all four after.
The one that changed to 0x186A0 is the world switch timer register.

\---

### Framebuffer Partition Registers

**Source: gc\_9\_0\_offset.h — PRIMARY SOURCE IN HAND**

|Register|DWORD Offset|Notes|
|-|-|-|
|mmMC\_VM\_FB\_SIZE\_OFFSET\_VF0|0x5a80|Sequential through VF15|
|mmMC\_VM\_FB\_SIZE\_OFFSET\_VF15|0x5a8f||

\---

### PCIe GPUIOV Capability Registers

**Source: gim\_pci.h — PRIMARY SOURCE IN HAND**
G = capability base, discovered at runtime by walking PCIe extended caps
from 0x100, finding VSEC ID = 0x02.

|Register|Offset|Width|Notes|
|-|-|-|-|
|PCI\_GPUIOV\_VSEC|G+0x04|32-bit|VSEC header|
|PCI\_GPUIOV\_CMD\_CONTROL|G+0x0C|8-bit|Command write — BYTE write|
|PCI\_GPUIOV\_FCN\_ID|G+0x0D|8-bit|Current function ID|
|PCI\_GPUIOV\_NXT\_FCN\_ID|G+0x0E|8-bit|Next function ID|
|PCI\_GPUIOV\_CMD\_STATUS|G+0x10|8-bit|Poll for COMMAND\_DONE = 0x00|
|PCI\_GPUIOV\_RESET\_CONTROL|G+0x14|8-bit||
|PCI\_GPUIOV\_RESET\_NOTIFICATION|G+0x18|32-bit||
|PCI\_GPUIOV\_VM\_INIT\_STATUS|G+0x1C|32-bit||
|PCI\_GPUIOV\_CNTXT|G+0x20|32-bit|Context pointer|
|PCI\_GPUIOV\_TOTAL\_FB\_AVAILABLE|G+0x24|16-bit|READ — expect 0x0200|
|PCI\_GPUIOV\_TOTAL\_FB\_CONSUMED|G+0x26|16-bit|WRITE — set to 0x0200|
|PCI\_GPUIOV\_VF0\_FB\_SIZE|G+0x2C|16-bit|WRITE — FB size|
|PCI\_GPUIOV\_VF0\_FB\_OFFSET|G+0x2E|16-bit|WRITE — FB offset|
|... VF1-VF14 sequential ...|||stride = 4 bytes per VF|
|PCI\_GPUIOV\_VF15\_FB\_SIZE|G+0x68|16-bit||
|PCI\_GPUIOV\_VF15\_FB\_OFFSET|G+0x6C|16-bit||

\---

### FB Size Encoding

**Source: gim\_gpuiov.c — PRIMARY SOURCE IN HAND**

```c
data = (((size\\\_mb) >> 4) - 1)       // low 16 bits: (size/16MB) - 1
     | ((offset\\\_mb >> 4) << 16)      // high 16 bits: offset/16MB
```

For VF0: size=8192MB, offset=0MB

* Low 16:  (8192/16) - 1 = 511 = 0x01FF
* High 16: 0
* Write 0x01FF to G+0x2C (VF0\_FB\_SIZE)
* Write 0x0000 to G+0x2E (VF0\_FB\_OFFSET)

\---

### GPUIOV Command Encodings

**Source: gim\_gpuiov.h + gim\_gpuiov.c — PRIMARY SOURCE IN HAND**

```c
INIT\\\_GPU on VF0: 0x00008017
RUN\\\_GPU on VF0:  0x00008014
```

After each command: poll G+0x10 (CMD\_STATUS) for 0x00 (COMMAND\_DONE).

\---

### Mailbox Protocol Constants

**Source: mxgpu\_ai.c + mxgpu\_ai.h — PRIMARY SOURCE IN HAND**

**IDH Request Values (VF→PF):**

|Name|Value|
|-|-|
|IDH\_REQ\_GPU\_INIT\_ACCESS|1|
|IDH\_REL\_GPU\_INIT\_ACCESS|2|
|IDH\_REQ\_GPU\_FINI\_ACCESS|3|
|IDH\_REL\_GPU\_FINI\_ACCESS|4|
|IDH\_REQ\_GPU\_RESET\_ACCESS|5|
|IDH\_REQ\_GPU\_INIT\_DATA|6|
|IDH\_READY\_TO\_RESET|201|

**IDH Event Values (PF→VF — v340ctl responds to these):**

|Name|Value|
|-|-|
|IDH\_CLR\_MSG\_BUF|0|
|IDH\_READY\_TO\_ACCESS\_GPU|1|
|IDH\_FLR\_NOTIFICATION|2|
|IDH\_FLR\_NOTIFICATION\_CMPL|3|
|IDH\_SUCCESS|4|
|IDH\_FAIL|5|
|IDH\_QUERY\_ALIVE|6|
|IDH\_REQ\_GPU\_INIT\_DATA\_READY|7|

**Timeout Constants:**

|Constant|Value|
|-|-|
|AI\_MAILBOX\_POLL\_ACK\_TIMEDOUT|500ms|
|AI\_MAILBOX\_POLL\_MSG\_TIMEDOUT|6000ms|
|AI\_MAILBOX\_POLL\_FLR\_TIMEDOUT|10000ms|

**PF Response Protocol (v340ctl implements this):**

```
On IDH\\\_REQ\\\_GPU\\\_INIT\\\_ACCESS (1) in RCV\\\_DW0:
  DWORD-write 0 to NBIO\\\_MMIO\\\[0x013C × 4]    (RCV\\\_DW2 = checksum\\\_key 0)
  DWORD-write 1 to NBIO\\\_MMIO\\\[0x013A × 4]    (RCV\\\_DW0 = READY\\\_TO\\\_ACCESS\\\_GPU)
  Byte-write 0x01 to (NBIO\\\_BASE + 0x04F9)    (assert RCV valid)

On IDH\\\_QUERY\\\_ALIVE (6) in RCV\\\_DW0:
  Byte-write 0x02 to (NBIO\\\_BASE + 0x04F9)    (ack only)
```

\---

### MRPC Opcodes (Switchtec BAR0)

**Source: mrpc.h (Microsemi switchtec-user) — PRIMARY SOURCE IN HAND**

|Command|Opcode|Sub-command|Value|Notes|
|-|-|-|-|-|
|MRPC\_GFMS\_BIND\_UNBIND|0x84|MRPC\_GFMS\_BIND|0x01|Open VF routing|
|MRPC\_GFMS\_BIND\_UNBIND|0x84|MRPC\_GFMS\_UNBIND|0x02|Close VF routing|
|MRPC\_PORTPARTP2P|0x0C|—|—|P2P binding (DirectPort Day 2)|
|MRPC\_PART\_INFO|0x2B|MRPC\_PART\_INFO\_GET\_ALL\_INFO|0x00|Query partition state|

\---

### World Switch Timer

**Source: gfx\_v9\_0.c — PRIMARY SOURCE IN HAND**

RefCLK = 25 MHz. 4ms time slice = 100,000 = **0x186A0**
Write to empirically confirmed SCH register (Gate 7).
Convergent confirmation: VMware adapter1\_conf time\_slice=4000,
GIM config sched\_interval=4ms — all encode 0x186A0.

\---

### VF Framebuffer Layout (V1)

**Source: amdgv\_sriovmsg.h — PRIMARY SOURCE IN HAND**

```
0x00000  \\\[64KB]  vBIOS image           ← v340ctl writes here
0x10000  \\\[1KB]   PF2VF data struct     ← v340ctl writes here
0x10400  \\\[1KB]   VF2PF data struct     ← guest driver writes here
0x10800  \\\[2KB]   Bad pages region
```

\---

### Minimum PF2VF Struct

**Source: amdgv\_sriovmsg.h — PRIMARY SOURCE IN HAND**
Struct is exactly 1024 bytes. Static assert enforced at compile time.

```c
header.size                  = 1024
header.version               = 2
checksum                     = \\\[COMPUTED — see algorithm below]
feature\\\_flags                = 0
fcn\\\_idx                      = 0
vf2pf\\\_update\\\_interval\\\_ms    = 2000
// all other fields = 0
```

**feature\_flags = 0 is critical.** Setting reg\_indirect\_acc = 1 forces
RLCG indirect path requiring additional GC register setup. Leave at 0.

\---

### Checksum Algorithm

**Source: amdgpu\_virt.c — FETCHED DIRECTLY
URL: https://raw.githubusercontent.com/torvalds/linux/master/drivers/gpu/drm/amd/amdgpu/amdgpu\_virt.c**

**CHECKSUM = 0 WILL FAIL. Guest driver returns -EINVAL. MUST COMPUTE.**

```c
unsigned int amd\\\_sriov\\\_msg\\\_checksum(void \\\*obj,
                unsigned long obj\\\_size,
                unsigned int key,       // key = 0 for version 2
                unsigned int checksum)  // the field value being validated
{
    unsigned int ret = key;
    unsigned long i = 0;
    unsigned char \\\*pos = (char \\\*)obj;

    for (i = 0; i < obj\\\_size; ++i)
        ret += \\\*(pos + i);

    pos = (char \\\*)\\\&checksum;
    for (i = 0; i < sizeof(checksum); ++i)
        ret -= \\\*(pos + i);

    return ret;
}
```

v340ctl Phase 7 sequence:

1. Zero-fill 1024-byte PF2VF struct
2. Populate required fields
3. Set checksum field = 0
4. Call amd\_sriov\_msg\_checksum(struct\_ptr, 1024, 0, 0)
5. Write returned value into checksum field
6. Write complete struct to VF\_FB\_BASE + 0x10000

\---

## SECTION 6 — CONFIRMED WINDOWS DRIVER STACK


**All hardware access uses Microsoft WDK APIs exclusively.**

### API 1 — PCIe Extended Config Space

**Source: Microsoft Learn — FETCHED DIRECTLY**

API: BUS\_INTERFACE\_STANDARD (GetBusData / SetBusData)
DataType: PCI\_WHICHSPACE\_CONFIG
Supports offsets 0x100–0xFFF (extended config space) when ECAM present.
Intel C422 on P520 provides ECAM. Confirmed.
IRQL constraint: IRQL <= APC\_LEVEL.

**VBS CONSTRAINT — CONFIRMED:**
VBS enabled = BSOD on non-standard PCI config access.
v340ctl MUST use BUS\_INTERFACE\_STANDARD exclusively.
Check VBS status on P520 before first activation run.

\---

### API 2 — BAR0 MMIO Register Access

**Source: Microsoft Learn — FETCHED DIRECTLY**

API: MmMapIoSpaceEx / MmUnmapIoSpace
IRQL constraint: <= DISPATCH\_LEVEL.

Protection flag selection:

|Operation|Flag|
|-|-|
|NBIO BAR mailbox register writes|PAGE\_READWRITE \| PAGE\_NOCACHE|
|GC BAR RLC scheduler register|PAGE\_READWRITE \| PAGE\_NOCACHE|
|GC BAR VF\_ENABLE write|PAGE\_READWRITE \| PAGE\_NOCACHE|
|VF framebuffer — vBIOS copy (64KB)|PAGE\_READWRITE \| PAGE\_WRITECOMBINE|
|VF framebuffer — PF2VF struct (1KB)|PAGE\_READWRITE \| PAGE\_WRITECOMBINE|
|ROM BAR — vBIOS read|PAGE\_READONLY|
|Switchtec BAR0 — MRPC|PAGE\_READWRITE \| PAGE\_NOCACHE|

Map at start of IOCTL handler → operate → unmap before return.
Use \_\_try/\_\_finally to guarantee unmap on all exit paths.

\---

### API 3 — KMDF Architecture

v340ctl is a minimal KMDF kernel driver exposing DeviceIoControl to a
userspace service. Obtains BUS\_INTERFACE\_STANDARD from parent bus driver.
Obtains BAR physical addresses via IoGetDeviceProperty / PnP resource list.
Zero third-party dependencies. WDK PCIDRV sample as reference.

\---

## SECTION 7 — IMPLEMENTATION SEQUENCE (v340ctl — Path A)

Executes per PF. Both dies activated sequentially.

**Phase 0:** Disable both 6864 PF device nodes in Device Manager.

**Phase 1 — GPUIOV Capability Discovery**
Walk PCIe extended caps from 0x100 using GetBusData.
Find VSEC ID = 0x02. Set G = capability base.

**Phase 2 — Framebuffer Carving**
GetBusData: Read G+0x24 — verify 0x0200
SetBusData: G+0x2C = 0x01FF, G+0x2E = 0x0000, G+0x26 = 0x0200

**Phase 3 — Context Setup**
SetBusData: G+0x20 = 0x00000001

**Phase 4 — GPUIOV Commands**
SetBusData: G+0x0C = 0x00008017. Poll G+0x10 == 0x00.
SetBusData: G+0x0C = 0x00008014. Poll G+0x10 == 0x00.

**Phase 5 — Switchtec MRPC**
Map Switchtec BAR0 via MmMapIoSpaceEx (PAGE\_NOCACHE).
Send MRPC 0x84 sub-command 0x01 (GFMS\_BIND). Unmap.

**Phase 6 — RLC Scheduler**
Map GC BAR via MmMapIoSpaceEx (PAGE\_NOCACHE).
Write 0x00000003 to mmRLC\_GPU\_IOV\_VF\_ENABLE (0x5b00 × 4). // VF0 + VF1 — value 1 enables VF0 only
Write 0x000186A0 to confirmed SCH register (Gate 7).
Unmap.

**Phase 7 — Framebuffer Data Region**
ROM BAR read → 64KB vBIOS → write to VF\_FB\_BASE + 0x00000.
Fallback: pre-extracted gfx900 vBIOS binary.
Build PF2VF struct, compute checksum → write to VF\_FB\_BASE + 0x10000.

**Phase 8 — Persistent Mailbox Service**
Poll NBIO\_MMIO\[0x013A × 4] (RCV\_DW0) every 10ms.
Respond to IDH\_REQ\_GPU\_INIT\_ACCESS (1) and IDH\_REQ\_GPU\_INIT\_DATA (6).
Note: opcode 6 in RCV\_DW0 is IDH\_REQ\_GPU\_INIT\_DATA (VF→PF request), not IDH\_QUERY\_ALIVE (PF→VF event). Must reply with IDH\_REQ\_GPU\_INIT\_DATA\_READY (7).

Repeat Phases 1–8 for Die 1 (second 6864 PF).

**Verification:** Poll for DEV\_686C in Device Manager.
If not present within 30s: power cycle, review, retry.

\---

## SECTION 8 — ALTERNATIVE ACTIVATION PATHS

### Path B — ESXi Nested via Hyper-V DDA

**Status: FULLY DOCUMENTED — primary source confirmed
Source: S59 (AMD deployment guide), S60 (Lenovo guide) — fetched directly**

AMD and Lenovo formally co-certified the V340L on the ThinkStation P520
for ESXi-based MxGPU deployments. The P520 is the reference platform.

**Host setup:**

1. Enable nested virtualization on the ESXi VM:
`Set-VMProcessor -VMName <ESXi\\\_VM> -ExposeVirtualizationExtensions $true`
2. Dismount both 6864 PFs from Windows host:
`Dismount-VMHostAssignableDevice -LocationPath <path> -Force`
3. Assign both PFs to ESXi guest VM via DDA:
`Add-VMAssignableDevice -VMName <ESXi\\\_VM> -LocationPath <path>`

**ESXi guest setup:**

1. Boot from Lenovo-customized ESXi 6.5 U1 ISO:
VMware-ESXi-6.5.0.update01-7388607-LNV-20180201.iso
2. Install VIB (amdgpuv 2.0.8 — already downloaded):
`esxcli software vib install --no-sig-check -v /vmfs/volumes/.../amdgpuv-2.0.8-1OEM.650.0.0.4598673.x86\\\_64.vib --maintenance-mode`
3. Configure MxGPU (4 VFs per die = 8 total):
`esxcfg-module -s "GPU1=4 GPU2=4 MxGPUi=0" amdgpuv`
4. Reboot ESXi.
5. Verify: Hardware tab should show 2 physical GPUs + 8 virtual GPUs.

**Windows guest setup (inside ESXi):**

1. Create Windows 10 VM. Set CPUs >= 4. Reserve all guest memory.
2. Add VF as PCI device in VM settings.
3. Install official V340 guest driver:
Radeon Pro Software for Enterprise on Windows 10 — 19.Q2 (S58)
Native 686C target. No INF edit required.
4. Validate with Furmark. GPU temperature will always show 88°C — normal.

**Known issue:** Local ESXi web console becomes unstable after AMD driver
installation. Use VMware Horizon client for remote access thereafter.

**Memory reservation requirement:** VM will fail to start after AMD driver
install if guest memory is not fully reserved. Set Reservation = RAM.

\---

### Path C — Switchtec Userspace Daemon

**Status: ARCHITECTURE CONFIRMED FROM PRIMARY SOURCE —
Day 1 empirical gates required. Unvalidated on hardware.**

**Driver — CONFIRMED:**
switchtec-kmdf-0.6 confirmed in Intel AIC package (S64).
WHQL-signed. DriverVer 04/17/2018,11.21.4.279.
Covers VEN\_11F8&DEV\_854x&CC\_058000 (PSX Gen3 management endpoints).
Actual V340L switch DEV ID confirmed DEV\_8533 (PFX 48xG3) from Day 1
hardware observation — explicitly covered by INF. Prediction of DEV\_8543
was incorrect. No impact on driver compatibility.
NVMe load dependency confirmed fixed in firmware B08C — verbatim from
Firmware Release Notes.txt: "Fix an issue with the Windows driver which
requires a drive to be present for the driver to load."
This fix is load-bearing for the V340L: no external NVMe slots exist
on the card. No NVMe workaround is possible or required.

Pre-stage driver before card arrival:
```
pnputil /add-driver "switchtec-kmdf-0.6\Switchtec.inf" /install
```

**GFMS\_BIND payload — CONFIRMED from lib/switchtec.c:**

Wire format (confirmed verbatim):
```c
struct {
    uint8_t  subcmd;              // MRPC_GFMS_BIND = 0x01
    uint8_t  host_sw_idx;
    uint8_t  host_phys_port_id;
    uint8_t  host_log_port_id;
    struct {
        uint16_t pdfid;           // Physical Device Function ID
        uint8_t  next_valid;      // 1 if another function follows
        uint8_t  reserved;
    } function[8];
} cmd;
```

Sent via switchtec\_cmd(dev, MRPC\_GFMS\_BIND\_UNBIND /\*0x84\*/, &cmd, ...).

All field values (host\_sw\_idx, host\_phys\_port\_id, host\_log\_port\_id,
pdfid) are discovered at runtime via gfms\_dump before activation.
PDFID is a Switchtec fabric-level identifier — not PCIe BDF.
gfms\_dump reports pdfid\_start/pdfid\_end per downstream port.

**CRITICAL — CLI platform guard:**
The entire fabric subcommand group (gfms\_bind, gfms\_unbind, gfms\_dump)
is compiled out on Windows:
```
#ifdef __linux__
    {"gfms_bind", gfms_bind, ...},
    {"gfms_dump", gfms_dump, ...},
#endif //__linux__
```
The pre-built switchtec-user-0.9.6.exe CANNOT send GFMS commands on Windows.
switchtec\_gfms\_bind() and switchtec\_cmd() in the library are NOT
platform-guarded and work on Windows via \\\\.\switchtec0 IOCTL path.
**A daemon compiled from source against libswitchtec is required.**

**Binding persistence:**
gfms\_bind is one-shot. The Switchtec holds binding state in volatile RAM.
No keep-alive required. Power cycle resets all bindings — daemon must
re-run gfms\_bind on every boot before guest driver loads.

**OPEN — MMIO sequence required from host:**
The Switchtec handles PCIe fabric routing only. It does not write AMD GPU
silicon registers. On Linux, GIM performs both the MRPC bind AND the
GPUIOV/FB/RLC MMIO sequence. Whether the MMIO sequence is required from
the Windows host, or handled internally by the GPU firmware/VIB after
receiving the PF via DDA, is unknown pre-hardware.

Day 1 experiment: fire gfms\_bind only. Observe whether DEV\_686C appears.

- If 686C appears: gfms\_bind alone is sufficient. Daemon is ~200 lines.
  No KMDF driver required.
- If 686C does not appear: add GPUIOV/FB/RLC sequence per Section 7
  via Path A KMDF components.

This is the single most consequential unknown remaining pre-hardware.

Fork: MansfieldPlumbing/switchtec-user (for V340L-specific tooling
and building against current upstream if 0.9.6 is insufficient)
Upstream: Microsemi/switchtec-user (active, last commit March 2026, MIT)

\---

## SECTION 9 — DAY 1 SEQUENCE AND DECISION TREE

**Immediate Sequence (in order, no skipping):**

1. Confirm P520 BIOS: SR-IOV ON, Above 4G Decoding ON, ARI ON,
VT-d ON, ACS ON. Note VBS status.
2. Insert V340L on external riser. Power from EVGA SuperNOVA only.
3. Boot. Confirm three PCIe devices enumerate:
two DEV\_6864 PFs + one Switchtec management endpoint.
Fewer than three = riser or power issue. Do not proceed.
4. **CHECK DEVICE MANAGER FOR SWITCHTEC ENDPOINT.**
Note exact device status: driver loaded / unknown device / not present.
This determines Path C feasibility.
5. Run SurveyDDA.ps1 — record IOMMU groups, BDFs for all devices (Gate 4).
6. Check subsystem ID on 6864 PFs: 0C001002 (AMD) or 0C0017AA (Lenovo).
Both are in the VIB. Document which your card reports.
7. Read all four mmRLC\_GPU\_IOV\_SCH\_x registers via v340ctl KMDF driver
(baseline before activation — Gate 7).
8. Disable both 6864 PF device nodes in Device Manager (Gate 10 mitigation).
9. Fire v340ctl activation sequence per Section 7.
10. Watch for DEV\_686C in Device Manager.
11. **IF 686C APPEARS: BUY 3 MORE CARDS BEFORE DOING ANYTHING ELSE.**
12. Re-enable devices.
13. Install guest driver in Hyper-V DDA guest.
Primary: 19.Q2 V340 official guest driver (S58) — native 686C, no edit.
Fallback: 21.Q4 WX 8200 with INF edit (S36 procedure) — Vulkan 1.2.
14. Run llama.cpp. Record tokens/second. Screenshot everything.
15. Push repo public.

**Guest driver strategy:**
Install 19.Q2 V340 official driver first — confirms 686C loads cleanly.
If inference performance is acceptable: done.
If Vulkan 1.2 features required for llama.cpp backend: swap to 21.Q4 WX 8200.
The official 19.Q2 driver targets Vulkan 1.1 per AMD deployment guide.
Gate 6 confirmed 1.2 from 21.Q4 only. Both drivers install without INF edit
against 686C — 19.Q2 natively, 21.Q4 with INF edit per S36.

**Day 1 Decision Tree:**

```
Card inserted → three devices enumerate?
  NO  → riser/power debug
  YES → Switchtec visible in Device Manager?
          YES → attempt Path C (switchtec-user userspace)
          NO  → Path A (v340ctl KMDF) only

v340ctl fires → 686C appears?
  NO  → debug activation sequence (Section 7)
  YES → 686C driver loads in guest?
          NO  → debug guest driver / DDA assignment
          YES → llama.cpp generates tokens?
                  NO  → debug Vulkan backend
                  YES → measure tokens/sec + x1 link utilization

                        tokens reasonable + x1 link low:
                          P2P routes through Switchtec crossbar natively
                          DirectPort shelved

                        tokens crawling (\\\~1 t/s) + x1 link pegged:
                          DIRECTPORT TRIGGER — see Section 10
```

\---

## SECTION 10 — DIRECTPORT (CONDITIONAL)

Trigger: tokens/sec crawling AND x1 riser link pegged at 100%.
Do not engage on Day 1. Only if trigger condition confirmed.

Three components only:

1. NT Handles — D3D12\_HEAP\_FLAG\_SHARED\_CROSS\_ADAPTER
2. Shared Fencing — D3D12\_FENCE\_FLAG\_SHARED\_CROSS\_ADAPTER
3. Vulkan Interop — VK\_KHR\_external\_memory\_win32

Integration point: pipeline split layer boundary only.

\---

## SECTION 11 — KNOWN POST-ACTIVATION RISKS

**Risk 1 — Hyper-V vIOMMU blocking P2P DMA**
Only affects DirectPort stretch goal. Does not block primary inference path.

**Risk 2 — SmartFusion2 MRPC signing**
Low probability. If MRPC 0x84 returns error, firmware may require signed
payloads. Zero brick risk on any MRPC failure.

**Risk 3 — ROM BAR read complexity**
ROM BAR enable bit must be set before mapping. Fallback: pre-extracted vBIOS.

**Risk 4 — Switchtec endpoint hidden from host**
RESOLVED — Day 1 confirmed Switchtec PFX 48xG3 fully visible at 101:0.0.
Path C is unblocked.

**Risk 5 — ESXi nested DDA PCIe fidelity**
DDA passes raw PCIe lanes to the ESXi guest VM. BAR MMIO writes from the
VIB through nested virtualization have not been validated on P520. High
probability of working given P520 is the reference platform. Empirical.

\---

## SECTION 12 — AGENT FABRICATION RECORD

**GPT Fabrications:**

* Mailbox registers 0x14C6–0x14CD — impossible on gfx900
* MRPC\_PART\_CFG=0x2C, MRPC\_PART\_ADD=0x2D, MRPC\_PART\_DEL=0x2E — do not exist
* lib/partition.c with function bodies — file is a 404
* amd\_sriov\_msg\_checksum DWORD iteration — wrong algorithm

**Qwen Fabrications:**

* MRPC opcode values for partition operations — all wrong
* switchtec-user version tag and commit hash — fabricated

**Gemini Fabrications:**

* Claimed verified dual-die working state in Session 3 — did not exist
* Claimed Device Manager observations for V340L on Windows — inferred
  from INF file, not from any hardware observation. No primary source.

**CRITICAL RULE: NO FETCH = INFERRED.**
Specific hex values, line numbers, commit hashes from any agent increase
suspicion, not trust. Fetch the source before accepting any value.

\---

**METHODOLOGY NOTE — AI-ASSISTED VS AI-DISCOVERED**

The Intel AIC package (S64) was identified through human-directed search
outside the conventional GPU namespace. The lead came from searching for
Windows Switchtec driver support by silicon family after identifying the
switch vendor from teardown documentation — not from any AI suggestion.

AI analysis confirmed the finding against primary source and traced the
implications through to the activation sequence. AI verified the INF
contents, the NVMe fix language, the GFMS\_BIND wire format, and the
CLI platform guard — all character-for-character from fetched source.
AI did not discover it.

This distinction is recorded here because the same methodology that
catches fabrications (NO FETCH = INFERRED) is what validated this
finding. The discipline cuts both ways: it prevents accepting false
claims and prevents dismissing real ones.

The fabrication log and the Intel AIC finding are two sides of the
same coin. Both exist because every specific claim was verified against
primary source before being recorded.

\---

## SECTION 13 — SOURCE REGISTRY

|Tag|Description|URL|Status|
|-|-|-|-|
|S2|ServeTheHome V340 launch teardown|https://www.servethehome.com/amd-radeon-pro-v340-dual-vega-32gb-vdi-solution-launched/|CONFIRMED|
|S6|AMD V340 driver page (official)|https://www.amd.com/en/support/downloads/drivers.html/graphics/radeon-pro/radeon-pro-v-series/radeon-pro-v340.html|CONFIRMED|
|S7|Dolphin / Switchtec 170ns latency|https://www.dolphinics.com/products/PCI\_Express\_fabric.html|CONFIRMED|
|S8|GPUOpen GIM legacy repo — ARCHIVED April 2025|https://github.com/GPUOpen-LibrariesAndSDKs/MxGPU-Virtualization|CONFIRMED — S7150/Tonga only. No Vega code.|
|S9|AMD KVM MxGPU MI300X|https://instinct.docs.amd.com/projects/virt-drv/en/mainline-8.2.0.k/userguides/Getting\_started\_with\_MxGPU.html|CONFIRMED|
|S13|Proxmox V340 thread|https://forum.proxmox.com/threads/shared-gpu-for-vdi-radeon-pro-v340.170416/|CONFIRMED|
|S16|SmartFusion2 SRAM architecture|https://www.microchip.com/en-us/application-notes/an2718|CONFIRMED|
|S23|AMD MxGPU Setup Guide Part 2|https://drivers.amd.com/relnotes/mxgpu-setup-guide-part2-advanced-vmware-mxgpu-setup.pdf|CONFIRMED|
|S24|Microchip SmartFusion2 COMM\_BLK|https://microchip.com/content/dam/mchp/documents/FPGA/ApplicationNotes/ApplicationNotes/m2s\_ac434\_sram\_puf\_system\_liberov11p4\_an\_v1.pdf|CONFIRMED|
|S36|Proxmox kowmangler INF fix|https://forum.proxmox.com/threads/shared-gpu-for-vdi-radeon-pro-v340.170416/|CONFIRMED|
|S37|AMD 21.Q4 WX 8200 driver page|https://www.amd.com/en/support/downloads/previous-drivers.html/graphics/radeon-pro/radeon-pro-wx-x200-series/radeon-pro-wx-8200.html|CONFIRMED|
|S38|gc\_9\_0\_offset.h|https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/amd/include/asic\_reg/gc/gc\_9\_0\_offset.h|CONFIRMED|
|S39–S51|Primary source headers (direct injection)|nbio\_6\_1\_offset.h, mxgpu\_ai.c/h, mrpc.h, amdgv\_sriovmsg.h, gim\_\*.c/h, amdgpu\_virt.h|PRIMARY SOURCE IN HAND|
|S52|MS Learn: PCI Config Space Access|https://learn.microsoft.com/en-us/windows-hardware/drivers/pci/accessing-pci-device-configuration-space|CONFIRMED|
|S53|MS Learn: BUS\_INTERFACE\_STANDARD|https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-\_bus\_interface\_standard|CONFIRMED|
|S54|MS Learn: GET\_SET\_DEVICE\_DATA|https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-get\_set\_device\_data|CONFIRMED|
|S55|MS Learn: MmMapIoSpaceEx|https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-mmmapiospaceex|CONFIRMED|
|S57|Server 2025 DDA assessment|Agent inference — fetch required|INFERRED HIGH|
|S58|AMD V340 official Windows 10 guest driver 19.Q2|https://www.amd.com/en/support/downloads/drivers.html/graphics/radeon-pro/radeon-pro-v-series/radeon-pro-v340.html|CONFIRMED — seen live|
|S59|AMD Radeon Pro V340 MxGPU Deployment Guide v2.0|https://drivers.amd.com/relnotes/v340\_deployment\_guide.pdf|CONFIRMED — fetched|
|S60|Lenovo V340 VMware deployment guide|https://download.lenovo.com/servers/mig/2018/12/10/19648/amd\_dd\_video\_2.0.6\_vmware\_x86-64.pdf|CONFIRMED — fetched live|
|S61|amdgpuv VIB descriptor (local)|amdgpuv-2.0.8-1OEM.650.0.0.4598673.x86\_64.vib/descriptor.xml|PRIMARY SOURCE IN HAND|
|S62|switchtec-user upstream repo|https://github.com/Microsemi/switchtec-user|CONFIRMED — live|
|S63|Zhihu GIM config / 0x186A0 corroboration|https://zhuanlan.zhihu.com/p/1903710763094348622|CONFIRMED|
|S64|Intel AXXP3SWX08040/08080 AIC firmware package|microchip\_4P\_8P\_PCIe\_switch\_fw\_and\_tools\_MR4\_1.0b\_B08C\_6-17-20\_PV|PRIMARY SOURCE IN HAND. Contains: switchtec-kmdf-0.6 (WHQL-signed), switchtec-user-0.9.6.exe (pre-built Windows binary), Firmware Release Notes.txt (NVMe fix verbatim). Origin: Intel NVMe storage expansion AIC product line.|
|S65|switchtec-user master source dump|switchtec-user-master\_2026-03-18 — lib/switchtec.c, inc/switchtec/switchtec.h, cli/fabric.c|PRIMARY SOURCE IN HAND. GFMS\_BIND wire format, PDFID struct, CLI platform guard confirmed.|

\---

## SECTION 14 — OPEN QUESTIONS

**OQ-1 — Windows Server 2025 DDA Compatibility**
Status: INFERRED HIGH. Fetch MS documentation before OS upgrade.

**OQ-2 — Switchtec endpoint visibility in Windows**
Status: CLOSED. Switchtec fabric fully visible at 101:0.0 as PCI Express
Upstream Switch Port (DEV_8533, pci.sys). Two downstream ports at 102:0.0
and 102:1.0 routing to dies at 105:0.0 and 108:0.0 respectively. Path C
is fully unblocked for testing. See ADDENDUM10.md and SURVEYDDA_OUTPUT.txt.

**OQ-3 — switchtec-kmdf availability**
Status: CLOSED. switchtec-kmdf-0.6 confirmed in Intel AIC package (S64).
WHQL-signed. DriverVer 04/17/2018,11.21.4.279.

**OQ-4 — 19.Q2 V340 guest driver Vulkan version**
Status: INFERRED 1.1 per AMD deployment guide API table.
Verify by extracting amd-vulkan64.json from 19.Q2 package.
If 1.1: use 21.Q4 WX 8200 for inference, 19.Q2 for initial validation only.
If 1.2: use 19.Q2 for everything.

**OQ-5 — MMIO sequence required from Windows host**
Status: OPEN — single most consequential unknown remaining.
Does gfms\_bind alone trigger DEV\_686C, or is the GPUIOV/FB/RLC sequence
also required from the host? Day 1 empirical. Fire gfms\_bind first.
Determines whether daemon is ~200 lines or daemon plus KMDF driver.

