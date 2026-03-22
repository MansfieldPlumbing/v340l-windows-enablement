# v340l-windows-enablement

**Status: DAY 2 COMPLETE — Switchtec driver bound. Code 10. One registry key from MRPC surface.**
The Switchtec service is confirmed running and attached to the upstream port at 101:0.0.
Device is at Code 10 — KMDF WDF registry parameters missing from sc.exe service creation.
Fix is staged. Day 3 starts with one PowerShell command.
See ADDENDUM11.md for Day 2 findings. See ADDENDUM10.md for Day 1 topology and thermal warnings.

| | |
|---|---|
| **Subject** | AMD Radeon Pro V340L — 16GB HBM2, dual Vega10, 484 GB/s per die |
| **Host** | Lenovo ThinkStation P520 |
| **CPU** | Intel Xeon W-2145 |
| **RAM** | 160GB DDR4 ECC |
| **Primary GPU** | RTX 3090 (host display) |
| **OS** | Windows 11 Pro 24H2 (Build 26100) |

---

The V340L is a $50 enterprise GPU decommissioned from Google Stadia in January
2023. Thousands of units on the secondary market. Nobody has activated one on
Windows.

Not because it can't be done. Because nobody identified the correct layer.

---

## The Problem

The card boots as DEV_6864 (physical function). It only becomes DEV_686C
(virtual functions, usable for inference) after a software activation sequence
targeting volatile SRAM. Power cycle resets it. The sequence must run on every
boot.

On ESXi and Linux, AMD ships the activation mechanism — the amdgpuv VIB and
the GIM kernel module respectively. On Windows: nothing. The community spent
years fighting Code 43 with INF edits and vBIOS flashes. They were at the
wrong layer.

The correct layer is the Microsemi Switchtec PFX Gen3 PCIe fabric switch
embedded in the V340L. On Linux and ESXi AMD's drivers handle this switch
invisibly. On Windows it requires an explicit driver and an MRPC activation
command before the GPU silicon will respond to anything.

This repo documents that layer and the path to activation.

"Protocol verified against AMD's open source GIM driver (mxgpu_ai.c). Message IDs are identical. amdgv_sriovmsg.h is the extended version of the same spec with additional RAS telemetry support. The Windows daemon implements the identical handshake the Linux kernel guest driver uses."

---

## Status

**SWITCHTEC DRIVER BOUND.** The pre-empirical phase is complete. The card is in the
system, the PCIe topology perfectly matches the structural predictions, and
the Switchtec fabric is routing correctly. The Switchtec service is bound to
the upstream port. We are one registry key from an active MRPC surface.

---

## Day 1 — Reconnaissance Complete

Three PCIe devices enumerated exactly as predicted:

    101:0.0  Switchtec PFX 48xG3 Upstream Switch Port   [MRPC target]
    102:0.0  Switchtec Downstream Switch Port            [to Die 0]
    102:1.0  Switchtec Downstream Switch Port            [to Die 1]
    105:0.0  AMD DEV_6864 Die 0  (Error, no driver, expected)
    108:0.0  AMD DEV_6864 Die 1  (Error, no driver, expected)

SUBSYS_0C001002 confirmed — AMD branded variant, not Lenovo OEM.
SUBSYS_BEEF11F8 confirmed — factory Switchtec firmware, unmodified.

Gate 4 closed: dies are on separate downstream switch ports.
Independent IOMMU groups. Better than predicted — no coupling constraint.

Session terminated by thermal shutdown before activation attempt.
Passive heatsink requires forced high-static-pressure airflow on open bench.
No hardware damage — eNVM restored factory state on power cycle.
Zero brick risk guarantee held on first contact.

Full reconnaissance data: ADDENDUM10.md and SURVEYDDA_OUTPUT.txt

---

## Day 2 — Driver Bind Achieved

Both dies confirmed alive via GPU-Z before any driver work:
Vega10 GLXT SERVER, PCIe x16 Gen3, HBM2 2048-bit, 3584 shaders per die.
No clock or temp readings without driver — expected.

Windows 11 24H2 closed two previously viable bind paths:
- GUI "Have Disk" + uncheck "Show compatible hardware" — dead on Build 26100
- pnputil /scan-devices auto-bind — pci.sys outranks 3rd party at CC_060400

Successful bind achieved via registry injection:
- Switchtec.sys copied to System32\drivers
- Service created via sc.exe → STATE: RUNNING
- Class subkey manually created at HKLM\...\Control\Class\{1DED99DE...}\0000
- Device Enum key force-patched: Service, ClassGUID, Class, Driver
- DEVPKEY_Device_Service confirmed returning "Switchtec"

Code 10 on device restart. Root cause identified: sc.exe bypasses INF install,
so KMDF WDF parameters were never written. Missing key:

    HKLM\SYSTEM\CurrentControlSet\Services\Switchtec\Parameters\Wdf
      KmdfLibraryVersion = "1.15"

Day 3 starts by writing this key and restarting the device.
Switchtec CLI confirmed at: C:\Program Files\Switchtec\switchtec.exe

Full Day 2 findings: ADDENDUM11.md

---

## The Key Finding

The V340L contains a Microsemi Switchtec PFX 48xG3 PCIe fabric switch
(confirmed DEV_8533). Activating the card on Windows requires a driver for
this switch before anything else can proceed.

That driver was found hiding in Intel's firmware package for the
AXXP3SWX08040 — a NVMe storage expansion AIC that uses the same Switchtec
silicon. The driver (switchtec-kmdf-0.6) is WHQL-signed, covers PFX Gen3
management endpoints via VEN_11F8 DEV_8533, and has a confirmed fix
removing the NVMe-present requirement for driver load.

The finding went undiscovered by the community for years because:

- Nobody identified the Switchtec as a required Windows driver target
- The driver is indexed under Intel storage AIC terms, not GPU terms
- The search namespaces — GPU virtualization vs NVMe fabric management —
  have zero overlap

It was found by stepping outside the conventional GPU search namespace,
identifying the switch vendor from teardown documentation, and searching
for Windows driver support for that specific silicon family.

---

## The Activation Path

Path C — Switchtec userspace daemon (active)

The Switchtec upstream port at 101:0.0 is claimed by pci.sys at enumeration
as a generic bridge. It routes lanes correctly but exposes no MRPC management
surface. Binding the Switchtec driver to 101:0.0 via registry injection creates
the device interface. A userspace daemon compiled against libswitchtec then calls
MRPC_GFMS_BIND (opcode 0x84, sub-cmd 0x01) to open SR-IOV VF routing paths
in the Switchtec fabric.

Key hypothesis: Because the Switchtec ASIC exists out-of-band from the OS
rings, sending MRPC_GFMS_BIND to the switch may trigger the silicon to expose
DEV_686C VFs natively without host-level PCIe capability carving. The daemon
will test this directly.

Path A — v340ctl KMDF (specified, not yet attempted)
Windows-native KMDF kernel driver replicates AMD's GIM activation sequence
from primary source headers. Fully specified. Available as fallback if Path C
alone is insufficient.

Path B — ESXi nested via Hyper-V DDA (available)
Windows host passes raw 6864 PFs to a nested ESXi guest via DDA. AMD's native
amdgpuv VIB handles activation inside ESXi. The VIB descriptor explicitly
lists Lenovo subsystem ID 17AA0C00 — the P520 is AMD and Lenovo's
co-certified reference platform for this card.

---

## What the Research Has Established

Confirmed from primary source:

- Switchtec PFX 48xG3 DEV_8533 enumerated and confirmed covered by
  switchtec-kmdf-0.6 INF
- Both AMD DEV_6864 PFs enumerated at 105:0.0 and 108:0.0
- Independent IOMMU groups — dies on separate downstream switch ports
- SUBSYS_0C001002 — AMD branded variant confirmed
- SUBSYS_BEEF11F8 — factory Switchtec firmware unmodified
- GPUIOV register offsets, FB encoding, mailbox opcodes, PF2VF checksum
  algorithm — all confirmed from AMD kernel source headers
- Official V340 Windows guest driver: AMD Radeon Pro Software 19.Q2
  native 686C target, no INF edit required
- Zero brick risk — activation targets volatile SRAM only
- Switchtec service successfully bound to upstream port via registry injection
- Windows 11 24H2 GUI and auto-bind paths confirmed non-viable

Open empirical gates:

- CLOSED: Switchtec enumerates in Windows — YES at 101:0.0
- CLOSED: DEV_8533 in INF coverage range — YES confirmed verbatim
- CLOSED: Switchtec service binds to upstream port — YES via registry injection
- Does Switchtec driver initialize cleanly? (Code 10 — WDF key fix pending)
- Does switchtec.exe list return switchtec0?
- Does MRPC_GFMS_BIND alone trigger DEV_686C enumeration?
- Does DEV_686C load on bare metal Windows 11 Pro with no Code 43?
- Does llama.cpp generate tokens?

---

## Platform Constraints Confirmed (Day 2)

- Windows 11 24H2 Build 26100: GUI driver override path non-functional
- pci.sys inbox driver wins all automatic bind races at CC_060400
- Registry injection is the only viable driver bind mechanism on this platform
- Switchtec management endpoint (Function 1, CC_058000) absent from PnP
  enumeration — eNVM suppression vs ARI miss unresolved, RWEverything scan pending
- Switchtec CLI: C:\Program Files\Switchtec\switchtec.exe

---

## Files

| File | Description |
|---|---|
| BRIEF.md | Complete technical record — register values, gate table, implementation sequence, source registry |
| SWITCHTEC.md | Switchtec driver and protocol findings — Intel AIC package analysis, GFMS_BIND payload, platform constraints |
| ADDENDUM10.md | Day 1 hardware reconnaissance — confirmed topology, Gate 4 closed, thermal incident |
| ADDENDUM11.md | Day 2 — Switchtec driver bind achieved, Code 10 root cause identified, Day 3 starting point |
| SURVEYDDA_OUTPUT.txt | Raw SurveyDDA.ps1 output — primary source for all Day 1 topology claims |

---

## Methodology Note

This repo was built using a primary-source-first discipline throughout.
Every hex value, register offset, and opcode is either confirmed from a
fetched primary source or explicitly labelled INFERRED. AI-assisted
analysis was used extensively for source retrieval and cross-referencing.
Human-directed search identified the Intel AIC package lead. AI verified
it against primary source.

The distinction matters: AI did not discover the Intel driver. A search
outside the conventional GPU namespace found it. AI confirmed it was real
and traced the implications through to the activation sequence. The
fabrication log in BRIEF.md Section 12 records where AI-generated values
were wrong and why the methodology caught them.

Day 1 told us the topology is sound. Day 2 bound the driver. Day 3 fires the daemon.
