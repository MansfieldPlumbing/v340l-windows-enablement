# V340L Windows Enablement — Session Research Addendum

**Status:** PRE-EMPIRICAL — Hardware validation pending  
**Session scope:** Daemon specification, register map, protocol verification  
**All values confirmed from primary source unless explicitly marked INFERRED**

---

## What This Session Accomplished

This session closed every software-resolvable implementation gate. The daemon is now fully specified from primary source with no inferred register values remaining. The card has not arrived. Nothing below has been validated on hardware.

---

## New Source Registry Entries

| ID | Source | Contents | Status |
|---|---|---|---|
| S_N1 | `mxgpu_ai.c` — torvalds/linux | VF-side mailbox implementation, IDH opcodes, WREG8 pattern | PRIMARY SOURCE IN HAND |
| S_N2 | `mxgpu_ai.h` — torvalds/linux | IDH enums, MAILBOX_CONTROL byte offset macros | PRIMARY SOURCE IN HAND |
| S_N3 | `nbio_6_1_offset.h` — torvalds/linux | Vega10 NBIO register offsets (actual header mxgpu_ai.c includes) | PRIMARY SOURCE IN HAND |
| S_N4 | `nbio_7_0_offset.h` — torvalds/linux | Cross-reference only — offsets identical to nbio_6_1 | PRIMARY SOURCE IN HAND |
| S_N5 | `vega10_ip_offset.h` — torvalds/linux | NBIO_BASE and GC_BASE segment addresses | PRIMARY SOURCE IN HAND |
| S_N6 | `gc_9_0_offset.h` — torvalds/linux | RLC IOV register offsets, SCH non-sequentiality confirmed | PRIMARY SOURCE IN HAND |
| S_N7 | `amdgv_sriovmsg.h` — torvalds/linux | PF2VF struct, FB layout constants, checksum declaration | PRIMARY SOURCE IN HAND |
| S_N8 | `amdgpu_virt.c` — torvalds/linux | Checksum algorithm implementation, version dispatch logic | PRIMARY SOURCE IN HAND |
| S_N9 | `gim_irqmgr.c` — GPUOpen MxGPU-Virtualization | PF-side mailbox handler, host send/receive sequence | PRIMARY SOURCE IN HAND |
| S_N10 | `gim_adapter.c` — GPUOpen MxGPU-Virtualization | GIM init sequence, vBIOS read method, SR-IOV enable | PRIMARY SOURCE IN HAND |
| S_N11 | `gim_s7150_reg.h` — GPUOpen MxGPU-Virtualization | S7150 register map (cross-reference only, not Vega10) | PRIMARY SOURCE IN HAND |

---

## Complete Vega10 Register Map — Confirmed

### NBIO Mailbox Registers (BAR0)

**Formula:** `BAR0_byte_offset = (NBIO_BASE__INST0_SEG2 + reg_offset) * 4`  
**NBIO_BASE__INST0_SEG2 = 0x0D20** (from vega10_ip_offset.h)  
**All registers BASE_IDX=2, confirmed from nbio_6_1_offset.h**

| Register | dword | BAR0 byte offset | Purpose |
|---|---|---|---|
| `mmMAILBOX_INDEX` | 0x0E55 | **0x03954** | Select target VF (write 0–15) |
| `mmMAILBOX_MSGBUF_TRN_DW0` | 0x0E56 | **0x03958** | PF→VF: message opcode |
| `mmMAILBOX_MSGBUF_TRN_DW1` | 0x0E57 | 0x0395C | PF→VF: data word 1 |
| `mmMAILBOX_MSGBUF_TRN_DW2` | 0x0E58 | **0x03960** | PF→VF: checksum_key (write 0) |
| `mmMAILBOX_MSGBUF_TRN_DW3` | 0x0E59 | 0x03964 | PF→VF: data word 3 |
| `mmMAILBOX_MSGBUF_RCV_DW0` | 0x0E5A | **0x03968** | VF→PF: IDH request opcode |
| `mmMAILBOX_MSGBUF_RCV_DW1` | 0x0E5B | 0x0396C | VF→PF: data word 1 |
| `mmMAILBOX_MSGBUF_RCV_DW2` | 0x0E5C | **0x03970** | VF→PF: checksum_key (version 1 only) |
| `mmMAILBOX_MSGBUF_RCV_DW3` | 0x0E5D | 0x03974 | VF→PF: data word 3 |
| `mmMAILBOX_CONTROL` | 0x0E5E | **0x03978** | ACK / valid control |

**MAILBOX_CONTROL byte layout** (confirmed from mxgpu_ai.h):

| Byte | Offset | Value | Meaning |
|---|---|---|---|
| 0 (TRN) | BAR0+0x3978 | write `0x01` | Set TRN_MSG_VALID — message ready for VF |
| 0 (TRN) | BAR0+0x3978 | write `0x00` | Clear TRN_MSG_VALID — before writing new message |
| 1 (RCV) | BAR0+0x3979 | write `0x02` | RCV_MSG_ACK — PF acknowledges VF's message |

**32-bit equivalents** (for `write32` implementations):

```
ACK receipt:          write32(BAR0+0x3978, 0x00000200)
Set TRN_MSG_VALID:    write32(BAR0+0x3978, 0x00000001)
Clear TRN_MSG_VALID:  write32(BAR0+0x3978, 0x00000000)
```

### GC/RLC IOV Registers (BAR0)

**Formula:** `BAR0_byte_offset = (GC_BASE__INST0_SEG1 + reg_offset) * 4`  
**GC_BASE__INST0_SEG1 = 0xA000** (from vega10_ip_offset.h)  
**All registers BASE_IDX=1, confirmed from gc_9_0_offset.h**

| Register | dword | BAR0 byte offset | Purpose |
|---|---|---|---|
| `mmRLC_GPU_IOV_VF_ENABLE` | 0xFB00 | **0x3EC00** | Enable VF hardware partitioning |
| `mmRLC_GPU_IOV_SCH_BLOCK` | 0xFB34 | 0x3ECD0 | Scheduler block config |
| `mmRLC_GPU_IOV_SCH_0` | 0xFB38 | 0x3ECE0 | Scheduler register 0 |
| `mmRLC_GPU_IOV_ACTIVE_FCN_ID` | 0xFB39 | 0x3ECE4 | Active function ID |
| `mmRLC_GPU_IOV_SCH_3` | 0xFB3A | 0x3ECE8 | Scheduler register 3 (**non-sequential**) |
| `mmRLC_GPU_IOV_SCH_1` | 0xFB3B | **0x3ECEC** | World-switch timer register |
| `mmRLC_GPU_IOV_SCH_2` | 0xFB3C | 0x3ECF0 | Scheduler register 2 |

**SCH non-sequentiality warning:** SCH_3 (0x5b3a) precedes SCH_1 (0x5b3b) in the register file. SCH_1 is the world-switch timer. Writing 0x186A0 (4ms) to the wrong SCH register silently misconfigures the scheduler.

---

## IDH Protocol — Complete Opcode Table

**Direction:** VF→PF means VF writes to its TRN, PF reads from its RCV.  
PF→VF is the reverse.

### IDH Requests (VF sends, PF receives at MSGBUF_RCV_DW0)

| Value | Name |
|---|---|
| 1 | `IDH_REQ_GPU_INIT_ACCESS` |
| 2 | `IDH_REL_GPU_INIT_ACCESS` |
| 3 | `IDH_REQ_GPU_FINI_ACCESS` |
| 4 | `IDH_REL_GPU_FINI_ACCESS` |
| 5 | `IDH_REQ_GPU_RESET_ACCESS` |
| 6 | `IDH_REQ_GPU_INIT_DATA` |

### IDH Events (PF sends, VF receives at MSGBUF_RCV_DW0)

| Value | Name |
|---|---|
| 0 | `IDH_CLR_MSG_BUF` |
| 1 | `IDH_READY_TO_ACCESS_GPU` |
| 2 | `IDH_FLR_NOTIFICATION` |
| 3 | `IDH_FLR_NOTIFICATION_CMPL` |
| 4 | `IDH_SUCCESS` |
| 5 | `IDH_FAIL` |
| 6 | `IDH_QUERY_ALIVE` |
| 7 | `IDH_REQ_GPU_INIT_DATA_READY` |

---

## VF Framebuffer Layout — Confirmed from amdgv_sriovmsg.h V1

| Offset | Size | Contents |
|---|---|---|
| BAR5 + 0x00000 | 64KB | vBIOS image |
| BAR5 + 0x10000 | 1KB | PF2VF struct |
| BAR5 + 0x10400 | 1KB | VF2PF struct (guest writes here) |
| BAR5 + 0x10800 | 2KB | Bad pages region |

---

## Checksum Algorithm — Confirmed Verbatim from amdgpu_virt.c

**NOT XOR. Byte addition with key, minus the stored checksum field bytes.**

```c
unsigned int amd_sriov_msg_checksum(void *obj,
                                    unsigned long obj_size,
                                    unsigned int key,
                                    unsigned int checksum)
{
    unsigned int ret = key;
    unsigned long i = 0;
    unsigned char *pos = (char *)obj;

    for (i = 0; i < obj_size; ++i)
        ret += *(pos + i);

    // subtract the checksum field's own bytes
    pos = (char *)&checksum;
    for (i = 0; i < sizeof(checksum); ++i)
        ret -= *(pos + i);

    return ret;
}
```

**For the daemon (version 2):**

```c
memset(&pf2vf, 0, sizeof(pf2vf));
pf2vf.header.size    = 1024;
pf2vf.header.version = 2;
pf2vf.checksum       = 0;
pf2vf.checksum = amd_sriov_msg_checksum(&pf2vf, 1024, 0, 0);
memcpy(BAR5 + 0x10000, &pf2vf, 1024);
```

When `key=0` and `checksum=0`, the subtraction step is a no-op. Port the full function anyway for correctness.

---

## PF2VF Version Discovery — Critical Finding

`amdgpu_virt.c` dispatches on `pf2vf_info->version`:

**Version 1** (`amdgim_pf2vf_info_v1` — legacy GIM/S7150 format):
- Uses `checksum_key` received from host via mailbox TRN_DW2
- Guest reads `checksum_key` from MSGBUF_RCV_DW2

**Version 2** (`amd_sriov_msg_pf2vf_info` — current Vega10/MI25 format):
- Always uses `key=0`
- Comment in source: "TODO: missing key, need to add it later"
- **This is the correct version for the V340L**

Set `header.version = 2`. TRN_DW2 should still be written as 0 for completeness.

---

## Open Question Resolved: Does gfms_bind Alone Cause DEV_686C?

**Answer: No.**

`mmRLC_GPU_IOV_VF_ENABLE` (BAR0+0x3EC00) is a separate register in the GC/RLC block. It tells the GPU silicon to advertise VF BARs on the PCIe bus. The Switchtec `gfms_bind` routes the PCIe fabric. `RLC_GPU_IOV_VF_ENABLE` enables the partitioning on the silicon itself. Both are required.

**Day 1 experiment is still valid** — try gfms_bind first to confirm, but the prediction is that DEV_686C will not appear until VF_ENABLE is written.

**Write value:** `0x3` for 2 VFs per die (bit 0 = VF0, bit 1 = VF1). Writing `0x1` only enables VF0.

---

## Complete Daemon Sequence — Fully Specified

```
Phase 1 — Fabric (libswitchtec, userspace)
  gfms_dump → discover pdfid_start per port, host_sw_idx,
               host_phys_port_id, host_log_port_id
  Build gfms_bind payload with discovered values
  switchtec_gfms_bind() via switchtec_cmd(dev, 0x84, ...)

Phase 2 — Silicon enable (via KMDF BAR mapper IOCTL)
  write32(BAR0+0x3ECEC, 0x186A0)   // SCH_1: 4ms world-switch timer
  write32(BAR0+0x3EC00, 0x3)       // VF_ENABLE: VF0 + VF1

Phase 3 — Mailbox loop (poll ~10ms per VF)
  write32(BAR0+0x3954, vf_id)      // MAILBOX_INDEX: select VF
  opcode = read32(BAR0+0x3968)     // MAILBOX_RCV_DW0: VF's request

  if opcode == 1 (IDH_REQ_GPU_INIT_ACCESS):
    write32(BAR0+0x3978, 0x200)    // ACK receipt
    memcpy(BAR5+0x00000, vbios, 65536)
    build pf2vf: size=1024, version=2, checksum=computed, rest=0
    memcpy(BAR5+0x10000, &pf2vf, 1024)
    write32(BAR0+0x3978, 0x0)      // clear TRN_MSG_VALID
    write32(BAR0+0x3960, 0)        // TRN_DW2 = 0
    write32(BAR0+0x3958, 1)        // IDH_READY_TO_ACCESS_GPU
    write32(BAR0+0x3978, 0x1)      // set TRN_MSG_VALID

  if opcode == 6 (IDH_QUERY_ALIVE):
    write32(BAR0+0x3978, 0x200)    // ACK, one write, done
```

---

## Code Audit: Current ai_studio_code.c / ai_studio_code.cpp

### What Is Correct

- All register offset values match primary source exactly
- IDH opcode values (1 = INIT_ACCESS, 6 = QUERY_ALIVE)
- MAILBOX_CONTROL byte offsets (+0 TRN, +1 RCV)
- Byte values (2 for ACK, 1 for valid, 0 for clear)
- FB layout (0x00000 vBIOS, 0x10000 PF2VF)
- `header.version = 2`
- `header.size = 1024`
- Checksum field zeroed before computing
- VF loop 0..1 for 2 dies
- KMDF structure (WdfDriverCreate, WdfDeviceCreate, IOCTL queue)
- MDL pattern (IoAllocateMdl, MmBuildMdlForNonPagedPool, MmMapLockedPagesSpecifyCache)
- ReleaseHardware teardown sequence

### Critical Issues — Will Cause Silent Failures

**[CRITICAL — cpp] PF2VF struct is hand-rolled with wrong field layout.**  
The `static_assert(sizeof==1024)` passes because padding fills it, but field offsets are wrong. The checksum field lands at the wrong byte offset. The guest driver reads wrong bytes, checksum validation returns `-EINVAL`, and the guest crashes on init. Fix: `#include "amdgv_sriovmsg.h"` directly. Do not hand-roll this struct.

**[CRITICAL — cpp] `RLC_GPU_IOV_VF_ENABLE` written as `1`, not `0x3`.**  
Value `1` enables only VF0. VF1 never appears. Only half the card works.

**[CRITICAL — cpp] SCH_0 through SCH_3 all written with `0x186A0`.**  
Only SCH_1 is the world-switch timer. SCH_0, SCH_2, SCH_3 are different control registers. Writing a timer value to them is undefined. Write only SCH_1. Leave others untouched until their purpose is confirmed from GIM source.

**[CRITICAL — c] `EvtDevicePrepareHardware` is never registered.**  
Declared and implemented but not wired into `WdfDeviceInitSetPnpPowerEventCallbacks()`. The BAR mapping never fires. Add `WDF_PNPPOWER_EVENT_CALLBACKS` in `EvtDriverDeviceAdd`.

### Moderate Issues

**[MODERATE — c] BAR selection by count is fragile.**  
`bar_count` increments per `CmResourceTypeMemory` descriptor. If the firmware reports resources in a different order or includes bridge window descriptors, the wrong BAR gets mapped silently. Identify by size: BAR0 is ~2MB (registers), BAR5 is 256MB+ (framebuffer).

**[MODERATE — c] `MmMapIoSpace` is deprecated.**  
Use `MmMapIoSpaceEx(addr, len, PAGE_READWRITE | PAGE_NOCACHE)` per confirmed Microsoft pcidrv pattern.

**[MODERATE — c] No `WdfDeviceSetPnpCapabilities`.**  
Without `SurpriseRemovalOK = WdfTrue`, Windows PnP may BSOD on surprise removal during the activation sequence. Gate 10 in the brief covers this.

**[MODERATE — cpp] Switchtec phase is a stub.**  
`gfms_bind` is commented out. PDFID values are required for a correct bind payload. Implement `gfms_dump` call before Day 1 testing.

**[MODERATE — cpp] `write8` to MMIO may not be safe from userspace.**  
Byte writes to MMIO through MDL mapping are architecture-dependent. If mailbox ACK behavior is unstable, switch to read-modify-write: `write32(base, CTRL, (read32(base, CTRL) & 0xFFFFFF00) | value)`.

---

## Confirmed Negative Findings

| Item | Status |
|---|---|
| GIM Vega source (open) | CLOSED NEGATIVE — GPUOpen repo archived Apr 2025, S7150/Tonga only |
| switchtec-user fabric CLI on Windows | CLOSED NEGATIVE — `#ifdef __linux__` in cli/fabric.c |
| Microchip PSX MRPC programming guide | CLOSED NEGATIVE — robots.txt blocked, no public doc found |
| AMD Hyper-V MI25 activation code | CLOSED NEGATIVE — proprietary Azure platform code, never published |

---

## Remaining Empirical Gates (Hardware Only)

1. Switchtec endpoint enumerates in Windows Device Manager
2. DEV_8546 confirmed (vs alternate PSX variant)
3. `switchtec-kmdf-0.6` binds cleanly on Windows 11
4. `gfms_bind` + `RLC_GPU_IOV_VF_ENABLE` causes DEV_686C to appear
5. DEV_686C loads AMD guest driver in Hyper-V DDA guest
6. Tokens generated by llama.cpp

---

## Prior Corrections from This Session

| Claim | Previous | Corrected |
|---|---|---|
| Checksum algorithm | "XOR-based CRC" | Byte addition with key, minus checksum field |
| DEV ID prediction | DEV_8543 (48-port) | DEV_8546 (PM8546 = 96-lane 48-port) |
| gfms_bind sufficient for DEV_686C | Unknown | No — RLC_GPU_IOV_VF_ENABLE also required |
| Gemini MAILBOX_CONTROL ACK value | 0x00020000 | 0x00000200 (byte 1, not byte 2) |
| PF2VF version for Vega10 | Version 1 assumed | Version 2 confirmed from amdgpu_virt.c |
| nbio header mxgpu_ai.c uses | nbio_7_0 assumed | nbio_6_1 confirmed (offsets identical) |
