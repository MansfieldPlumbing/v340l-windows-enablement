# ADDENDUM 10 — DAY 1 RECONNAISSANCE
## AMD Radeon Pro V340L MxGPU Windows Activation Project
## Date: March 2026
## Status: Pre-activation. Hardware confirmed alive.

---

## CITATION PROTOCOL

All topology claims in this document are derived from a single
primary source: raw SurveyDDA.ps1 output captured live during
first hardware session. See SURVEYDDA_OUTPUT.txt. Verify there.

---

## GATE 4 — CLOSED

Dual-die IOMMU grouping confirmed via SurveyDDA.ps1.

Result is better than predicted. The research corpus predicted
both dies would share an IOMMU group per S7150x2 precedent.
Actual topology shows the two dies on SEPARATE downstream switch
ports. Independent IOMMU groups. Each die can be assigned
independently. No coupling constraint.

---

## CONFIRMED PCIe TOPOLOGY

```
PCIROOT(64)
└── 101:0.0  Switchtec PFX 48xG3 — Upstream Switch Port   [MRPC target]
    ├── 102:0.0  Downstream Switch Port
    │   └── 105:0.0  VEN_1002 DEV_6864 SUBSYS_0C001002 REV_05  [Die 0]
    └── 102:1.0  Downstream Switch Port
        └── 108:0.0  VEN_1002 DEV_6864 SUBSYS_0C001002 REV_05  [Die 1]
```

**Switchtec:** VEN_11F8 DEV_8533 SUBSYS_BEEF11F8 REV_00
- DEV_8533 = PFX 48xG3 (not DEV_8543 as previously estimated — no impact)
- SUBSYS_BEEF11F8 = Microsemi internal placeholder — factory firmware,
  unmodified. Confirmed.

**AMD dies:** VEN_1002 DEV_6864 SUBSYS_0C001002 REV_05
- SUBSYS_0C001002 = AMD branded variant confirmed (not Lenovo 17AA OEM)
- Both dies enumerate at Error status — expected, no driver installed
- Both are activation targets

**Location paths (locked):**
```
Die 0: PCIROOT(64)#PCI(0000)#PCI(0000)#PCI(0000)#PCI(0000)#PCI(0000)#PCI(0000)
Die 1: PCIROOT(64)#PCI(0000)#PCI(0000)#PCI(0100)#PCI(0000)#PCI(0000)#PCI(0000)
```

---

## ARCHITECTURE CORRECTION — SWITCHTEC ACCESS PATH

**Prior assumption:** Switchtec enumerates as a raw MRPC management
endpoint with no driver. Phase 5 accesses BAR0 directly via
MmMapIoSpaceEx.

**Actual state:** pci.sys claims the upstream port at enumeration as a
generic PCIe bridge. Status OK. It routes lanes correctly — both dies
enumerate behind it — but exposes no MRPC management surface to
userspace.

**Resolution:** Bind Intel switchtec-kmdf driver to upstream port
101:0.0 only. This creates \\.\switchtec0 character device. The
switchtec-user userspace library then handles Phase 5 MRPC 0x84/0x01
directly through that interface. No custom kernel code required for
the MRPC path.

Do not rebind the downstream ports at 102:0.0 and 102:1.0. Leave
those on pci.sys. Only the upstream port requires the switchtec driver.

---

## TOOLCHAIN UPDATE

v340ctl is a userspace daemon — a Windows service. Not a KMDF driver.

Phase 5 (Switchtec MRPC) routes through the switchtec-user library
API against \\.\switchtec0 once the Intel driver is bound.

Config space access for Phases 1-4 (GPUIOV capability walk, FB
carving, GPUIOV commands) is currently suspended pending a simpler
hypothesis: because the Switchtec ASIC exists out-of-band from the
OS rings, sending MRPC_GFMS_BIND to the switch may trigger the
silicon to expose DEV_686C VFs natively without host-level PCIe
capability carving. The daemon will test this first. Phases 1-4
remain available as fallback if the Switchtec bind alone is
insufficient.

---

## THERMAL INCIDENT

Session terminated by thermal shutdown. Card produced long POST beep
and system froze under PowerShell enumeration load with no forced
airflow on the passive heatsink.

**No hardware damage.** eNVM restored factory state on power cycle.
Both dies re-enumerated cleanly after reboot. Zero brick risk
guarantee held.

**Forced high-static-pressure airflow is mandatory before next
session.** The V340L passive heatsink is insufficient under any
load on an open bench without dedicated forced air. A 120mm fan
directly on the fin stack is the minimum requirement.

---

## GATE STATUS UPDATE

| Gate | Question | Previous | New |
|---|---|---|---|
| 4 | Dual-die IOMMU grouping | OPEN | **CLOSED — independent** |
| 5 | 686C on bare metal Win11 Pro | OPEN | OPEN |
| 7 | SCH register index | OPEN | OPEN |
| 10 | PnP surprise removal | OPEN | OPEN |

---

## NEXT SESSION PREREQUISITES

1. Forced airflow on V340L heatsink — mandatory, non-negotiable
2. Bind Intel switchtec-kmdf to upstream port 101:0.0 only
3. Confirm `switchtec list` returns `switchtec0 PFX 48xG3`
4. Resolve config space access shim for Phases 1-4
5. Disable both 6864 PF nodes in Device Manager (Gate 10 mitigation)
6. Fire daemon
7. Watch for DEV_686C in Device Manager with no Code 43
