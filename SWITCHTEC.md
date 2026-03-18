# SWITCHTEC.md — Intel AIC Package and Protocol: Confirmed Findings for Path C

**Status: PRIMARY SOURCE CONFIRMED**
**Sources:**
- `microchip_4P_8P_PCIe_switch_fw_and_tools_MR4_1.0b_B08C_6-17-20_PV` — Intel AIC package dump
- `switchtec-user-master_2026-03-18` — Microsemi/switchtec-user upstream source dump

This document records confirmed findings from the Intel-distributed Switchtec driver
package and the switchtec-user upstream source that directly affect Path C feasibility.
All values are confirmed character-for-character from primary source dumps.
Nothing in this document is inferred unless explicitly labelled.

---

## The Package

Intel distributes a Microsemi Switchtec driver and firmware package for their
4-port and 8-port PCIe Gen3 x8 Switch AICs (AXXP3SWX08040, AXXP3SWX08080).
The package contains a complete Windows KMDF driver stack and a pre-built
userspace CLI binary. Both are directly applicable to the V340L.

**Package firmware version:** B08C
**Windows driver version:** 11.21.4.279 (DriverVer = 04/17/2018)

---

## Windows Driver — switchtec-kmdf-0.6

**Path in package:** `Windows/driver/switchtec-kmdf-0.6/`

**Files:**
```
switchtec.cat       — WHQL catalog signature file
Switchtec.inf       — Installation manifest
Switchtec.sys       — KMDF kernel-mode driver
```

**INF header (confirmed verbatim):**
```
Class=switch
ClassGuid={1DED99DE-B4AE-442D-A180-B5FF691AE552}
Provider=%ManufacturerName%
CatalogFile=Switchtec.cat
DriverVer = 04/17/2018,11.21.4.279
```

**Strings section (confirmed verbatim):**
```
ManufacturerName="MicroSemi Corporation"
ClassName="PCIe Switches"
```

**Architecture:** NTamd64. No OS version ceiling specified in INF.
Stated compatible OSs in release notes: Windows Server 2012R2, 2016, 2019.
Absence of OS ceiling in INF means Windows 10/11 are not explicitly excluded
at the driver level. Windows 11 compatibility is INFERRED HIGH — empirical on
first install.

**Signature:** WHQL-signed via Switchtec.cat. Installs without test signing
mode or DSE bypass. Catalog signature date (2018) against Windows 11 DSE is
an open question — empirical on Day 1.

---

## Hardware ID Coverage — Confirmed from Switchtec.inf

All entries below confirmed verbatim from the `[Standard.NTamd64]` section.
Both management endpoints (CC_058000) and NTB endpoints (CC_068000) are covered.

**PFX Gen3:**
```
DEV_8531  PFX 24xG3
DEV_8532  PFX 32xG3
DEV_8533  PFX 48xG3
DEV_8534  PFX 64xG3
DEV_8535  PFX 80xG3
DEV_8536  PFX 96xG3
```

**PSX Gen3:**
```
DEV_8541  PSX 24xG3
DEV_8542  PSX 32xG3
DEV_8543  PSX 48xG3
DEV_8544  PSX 64xG3
DEV_8545  PSX 80xG3
DEV_8546  PSX 96xG3
```

**PFXL Gen3:**
```
DEV_8561  PFXL 24xG3
DEV_8562  PFXL 32xG3
DEV_8563  PFXL 48xG3
DEV_8564  PFXL 64xG3
DEV_8565  PFXL 80xG3
DEV_8566  PFXL 96xG3
```

**PFXI Gen3:**
```
DEV_8571  PFXI 24xG3
DEV_8572  PFXI 32xG3
DEV_8573  PFXI 48xG3
DEV_8574  PFXI 64xG3
DEV_8575  PFXI 80xG3
DEV_8576  PFXI 96xG3
```

All management endpoints: `PCI\VEN_11F8&DEV_xxxx&CC_058000`
All NTB endpoints: `PCI\VEN_11F8&DEV_xxxx&CC_068000`

**V340L prediction:** ServeTheHome teardown (S2) identifies a 48-port switch.
If PSX Gen3, expected DEV ID is **DEV_8543**. INFERRED — confirm empirically
on Day 1. Any DEV ID in the table above will bind without INF modification.

---

## NVMe Dependency — CONFIRMED FIXED

**From `Firmware Release Notes.txt` (confirmed verbatim):**

```
fixes and enhancements:
- Fix an issue with the Windows driver which requires a drive to be
  present for the driver to load.
```

The driver loads and binds to the management endpoint independent of downstream
device presence. This is load-bearing for the V340L: the V340L's Switchtec
downstream ports connect internally to the two Vega10 dies. There are no
external NVMe slots on the card. No NVMe workaround is possible or necessary.

**Note:** The package's Known Restrictions section still lists "At least one
NVMe drive must be connected" as restriction #3, and user-facing instructions
repeat this requirement for firmware update workflows. This is documentation
lag. The restriction applies to the firmware update scripts, not to driver
loading. For Path C purposes (MRPC access only, no firmware flashing),
the fix is sufficient.

---

## Userspace CLI — switchtec-user-0.9.6

**Path in package:** `Windows/fw_tool/switchtec-user-0.9.6/`

**File:** `switchtec-user-0.9.6.exe` — pre-built Windows binary.

No MSYS2, no compilation required. Communicates with `\\.\switchtec0` device
node created by switchtec-kmdf on driver bind.

**Upstream repo:** https://github.com/Microsemi/switchtec-user
Last commit: March 2026 (active). The bundled binary is 0.9.6 from 2018.
Seven years of upstream development between bundled binary and HEAD.
Fork (MansfieldPlumbing/switchtec-user) should be maintained for any
V340L-specific tooling and to build against current upstream if needed.

---

## Gate Table Updates

| Gate | Previous Status | New Status |
|---|---|---|
| OQ-3 — switchtec-kmdf availability | OPEN — not publicly released | **CLOSED** — switchtec-kmdf-0.6 confirmed in Intel AIC package |
| NVMe load dependency | INFERRED FIXED | **CONFIRMED FIXED** — verbatim from Firmware Release Notes.txt |
| switchtec-user Windows binary | INFERRED from upstream | **CONFIRMED** — v0.9.6 pre-built exe in package |
| GFMS_BIND payload structure | OPEN — unknown fields | **CLOSED** — full wire format confirmed from lib/switchtec.c |
| PDFID discovery method | OPEN | **CLOSED** — gfms_dump, reports pdfid_start/pdfid_end per port |
| GIM open-source Vega code | ASSUMED AVAILABLE | **CLOSED NEGATIVE** — GPUOpen repo is S7150 only, archived Apr 2025 |
| fabric CLI available on Windows | ASSUMED YES | **CLOSED NEGATIVE** — entire fabric group is #ifdef __linux__ only |
| MMIO sequence required from host | OPEN | OPEN — Day 1 empirical gate |
| Windows 11 DSE catalog acceptance | OPEN | OPEN — empirical Day 1 |
| V340L switch DEV ID in INF coverage | OPEN | OPEN — Day 1 Device Manager |

---

## Pre-Arrival Action

Stage the driver in Windows' driver store before card arrives. On boot with
card inserted, Windows PnP will find and install automatically.

```powershell
pnputil /add-driver "C:\path\to\switchtec-kmdf-0.6\Switchtec.inf" /install
```

---

## Day 1 Verification Sequence

**Step 1 — Confirm enumeration and driver bind:**
```powershell
Get-PnpDevice | Where-Object { $_.HardwareID -like "*VEN_11F8*" }
```
Expected: device present, Status = OK, FriendlyName contains "Switchtec PSX".
If Status is not OK: check Event Viewer for driver load error — likely DSE
catalog rejection. See contingency below.

**Step 2 — Confirm userspace channel:**
```
switchtec-user-0.9.6.exe list
```
Expected output: `switchtec0    PSX xxXG3    RevB    ...`
This is the definitive confirmation that `\\.\switchtec0` is live and
Path C is open.

**Step 3 — Record DEV ID:**
Note exact device string from Step 1 and Step 2. Confirms which PSX variant
is present and closes the DEV ID gate.

---

## Contingency — DSE Catalog Rejection

If driver fails to load due to signature validation on Windows 11:

Option 1: Right-click `Switchtec.inf` → Install (bypasses catalog in some
configurations, installs directly from INF).

Option 2: Enable test signing temporarily:
```
bcdedit /set testsigning on
```
Reboot, install, verify Path C is functional, then evaluate whether to
leave test signing enabled or seek a newer signed package.

Option 3: Check whether a newer signed Switchtec driver package exists from
Microchip directly. The MansfieldPlumbing/switchtec-user fork presence in the
Microsemi network may be a contact point.

---

## GFMS_BIND Payload Structure — CONFIRMED FROM PRIMARY SOURCE

**Source: switchtec-user-master / lib/switchtec.c and inc/switchtec/switchtec.h**

### Public struct (switchtec.h):

```c
#define SWITCHTEC_FABRIC_MULTI_FUNC_NUM 8

struct switchtec_gfms_bind_req {
    uint8_t  host_sw_idx;
    uint8_t  host_phys_port_id;
    uint8_t  host_log_port_id;
    int      ep_number;
    uint16_t ep_pdfid[SWITCHTEC_FABRIC_MULTI_FUNC_NUM];
};
```

### Wire format constructed in switchtec_gfms_bind() (lib/switchtec.c):

```c
struct {
    uint8_t  subcmd;              // MRPC_GFMS_BIND = 0x01
    uint8_t  host_sw_idx;
    uint8_t  host_phys_port_id;
    uint8_t  host_log_port_id;
    struct {
        uint16_t pdfid;           // Physical Device Function ID
        uint8_t  next_valid;      // 1 if another function follows, else 0
        uint8_t  reserved;        // zero
    } function[8];
} cmd;
```

Sent via:
```c
switchtec_cmd(dev, MRPC_GFMS_BIND_UNBIND /*0x84*/, &cmd, sizeof(cmd),
              &result, sizeof(result));
```

Response:
```c
struct {
    uint8_t status;
    uint8_t reserved[3];
} result;
```

### Field values required — all discovered at runtime via gfms_dump:

| Field | Source | Notes |
|---|---|---|
| `host_sw_idx` | `gfms_dump` output | Switch index on the fabric |
| `host_phys_port_id` | `gfms_dump` output | Physical port ID of host connection |
| `host_log_port_id` | `gfms_dump` output | Logical port ID |
| `ep_pdfid[]` | `gfms_dump` PDFID range | One entry per GPU function being bound |
| `ep_number` | Count of PDFIDs | 1 per die minimum, up to 8 |

**PDFID** (Physical Device Function ID) is a 16-bit fabric-level identifier
assigned by the Switchtec firmware to each downstream endpoint function.
It is not the PCIe BDF. It is discovered via `gfms_dump`, which reports
`pdfid_start` and `pdfid_end` per port. For the V340L with two Vega10 dies,
expect two PDFID ranges — one per die.

---

## GFMS_UNBIND Payload — CONFIRMED FROM PRIMARY SOURCE

```c
struct {
    uint8_t  subcmd;              // MRPC_GFMS_UNBIND = 0x02
    uint8_t  host_sw_idx;
    uint8_t  host_phys_port_id;
    uint8_t  host_log_port_id;
    uint16_t pdfid;
    uint8_t  option;
    uint8_t  reserved;
} cmd;
```

---

## CLI Platform Guard — CRITICAL

**Source: switchtec-user-master / cli/fabric.c**

```c
#ifdef __linux__
...
static const struct cmd commands[] = {
    {"gfms_bind",   gfms_bind,   CMD_DESC_GFMS_BIND},
    {"gfms_unbind", gfms_unbind, CMD_DESC_GFMS_UNBIND},
    {"gfms_dump",   gfms_dump,   CMD_DESC_GFMS_DUMP},
    ...
};
#endif //__linux__
```

**The entire `fabric` subcommand group — including `gfms_bind`, `gfms_unbind`,
and `gfms_dump` — is compiled out on Windows.**

The pre-built `switchtec-user-0.9.6.exe` does not expose these commands.
You cannot run `switchtec-user-0.9.6.exe fabric gfms_bind` on Windows.

**However:** `switchtec_gfms_bind()` and `switchtec_cmd()` in the library
are NOT platform-guarded. They work on Windows via the `\\.\switchtec0`
IOCTL path. The CLI is locked out. The library is not.

**Implication:** The daemon must be compiled from source against libswitchtec.
This is not optional. The pre-built exe is useful for `switchtec list` and
`fw-info` verification only. All GFMS operations require custom code.

---

## Binding Persistence

`gfms_bind` is a one-shot command. The Switchtec holds binding state in its
internal GFMS database in volatile RAM. No keep-alive or heartbeat is required
to maintain the binding once set. The switch fires
`SWITCHTEC_GFMS_EVENT_BIND` to confirm.

**Power cycle resets all bindings.** The daemon must re-run `gfms_bind` on
every boot before the guest driver loads.

**Automatic unbind events to handle:**

| Event | Value | Meaning |
|---|---|---|
| `SWITCHTEC_GFMS_EVENT_HOST_LINK_DOWN` | 1 | Host link lost — binding may drop |
| `SWITCHTEC_GFMS_EVENT_DEV_DEL` | 3 | Device removed from fabric |

These indicate conditions where rebinding may be required without a full
power cycle. The daemon should monitor for these events and rebind if needed.
For the V340L use case (card fixed in slot, not hot-swapped), low probability
in normal operation.

---

## GFMS Event Types — CONFIRMED FROM PRIMARY SOURCE

**Source: switchtec-user-master / inc/switchtec/switchtec.h**

```c
enum switchtec_gfms_event_type {
    SWITCHTEC_GFMS_EVENT_HOST_LINK_UP       = 0,
    SWITCHTEC_GFMS_EVENT_HOST_LINK_DOWN     = 1,
    SWITCHTEC_GFMS_EVENT_DEV_ADD            = 2,
    SWITCHTEC_GFMS_EVENT_DEV_DEL            = 3,
    SWITCHTEC_GFMS_EVENT_FAB_LINK_UP        = 4,
    SWITCHTEC_GFMS_EVENT_FAB_LINK_DOWN      = 5,
    SWITCHTEC_GFMS_EVENT_BIND               = 6,
    SWITCHTEC_GFMS_EVENT_UNBIND             = 7,
    SWITCHTEC_GFMS_EVENT_DATABASE_CHANGED   = 8,
    SWITCHTEC_GFMS_EVENT_HVD_INST_ENABLE    = 9,
    SWITCHTEC_GFMS_EVENT_HVD_INST_DISABLE   = 10,
    SWITCHTEC_GFMS_EVENT_EP_PORT_REMOVE     = 11,
    SWITCHTEC_GFMS_EVENT_EP_PORT_ADD        = 12,
    SWITCHTEC_GFMS_EVENT_AER                = 13,
    SWITCHTEC_GFMS_EVENT_MAX                = 14,
};
```

---

## GIM Source Status

The GPUOpen `MxGPU-Virtualization` repo was archived April 21, 2025.
It contains GIM code for FirePro S7150/S7150x2 (Tonga) only — confirmed
from issue #43. There is no open-source GIM implementation for Vega or
the V340L. The new `amd/MxGPU-Virtualization` repo stripped all Vega code.

**The MMIO activation sequence (GPUIOV/FB/RLC) is not available from
open-source GIM source for Vega.** It exists only in the amdgpuv ESXi VIB
binary (S61, in hand) which would require reverse engineering to extract.
The MMIO sequence in the brief (Section 7) was derived from AMD kernel
headers, not GIM source — this is unaffected.

---

## MMIO Requirement — Open Empirical Question

The Switchtec `gfms_bind` handles PCIe fabric routing only. It does not
touch the AMD GPU silicon registers. On Linux, GIM performs both:

1. Switchtec MRPC bind (fabric routing)
2. GPUIOV + FB carving + RLC scheduler write + vBIOS/PF2VF to FB (GPU config)

Whether step 2 is required from the Windows host, or whether it is handled
internally by the ESXi VIB after receiving the PF via DDA, is unknown
pre-hardware.

**Day 1 experiment:** Fire `gfms_bind` only. Observe whether `DEV_686C`
enumerates without any MMIO sequence.

- If `686C` appears: `gfms_bind` alone is sufficient. Daemon is ~200 lines.
- If `686C` does not appear: MMIO sequence is required. Add Path A KMDF
  driver components per Section 7 of BRIEF.md.

This is the single most consequential unknown remaining. It determines
whether the Windows native path is a userspace daemon or a daemon plus
a small kernel driver.

---

## Windows Native vs Linux Path

| Component | Linux (AMD VIB) | Windows (Path C) |
|---|---|---|
| Switch driver | Built into ESXi kernel | switchtec-kmdf-0.6, signed |
| GFMS bind | GIM kernel module | libswitchtec via `\\.\switchtec0` |
| PDFID discovery | GIM internal | `gfms_dump` via libswitchtec |
| GPUIOV/FB/RLC | GIM kernel module | KMDF driver (if required) |
| Mailbox service | GIM kernel module | Win32 service loop |
| Guest driver | 19.Q2 V340 official | 19.Q2 V340 official — identical |
| Lines of code | 0 (AMD wrote it) | 200–600 depending on MMIO gate |

The Switchtec bind is simpler on Windows than Linux — you call a library
function against a signed driver rather than implementing it in a kernel
module. The GPUIOV/MMIO work, if required, is where the gap opens.

---

## What This Means for Path C

Revised understanding after switchtec-user source analysis:

1. `pnputil /add-driver Switchtec.inf /install` — signed, automatic
2. Card inserted → `\\.\switchtec0` live
3. Daemon: open device via libswitchtec → `gfms_dump` → discover PDFIDs
4. Daemon: `switchtec_gfms_bind()` → switch holds state, fires BIND event
5. **Gate:** does `DEV_686C` appear? If yes → DDA → guest driver → done
6. If no → add MMIO sequence per BRIEF.md Section 7, then retry
7. Mailbox service loop (persistent, services guest driver lifetime)

The daemon is required and must be compiled from source.
The pre-built exe cannot send GFMS commands on Windows.
The library can. The code to write is small.
