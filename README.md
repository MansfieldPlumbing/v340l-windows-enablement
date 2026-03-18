# v340l-windows-enablement

**Status: PRE-EMPIRICAL — Hardware validation pending.**
All paths fully specified from primary sources. No community precedent exists. Day 1 testing imminent.

| | |
|---|---|
| **Subject** | AMD Radeon Pro V340L — 16GB HBM2, dual Vega10, 484 GB/s per die |
| **Host** | Lenovo ThinkStation P520 |
| **CPU** | Intel Xeon W-2145 |
| **RAM** | 160GB DDR4 ECC |
| **Primary GPU** | RTX 3090 (host display) |
| **OS** | Windows 11 Pro |

---

The V340L is a $50 enterprise GPU decommissioned from Google Stadia in January 2023. Thousands of units on the secondary market. Nobody has activated one on Windows.

Not because it can't be done. Because nobody identified the correct layer.

---

## The Problem

The card boots as `DEV_6864` (physical function). It only becomes `DEV_686C` (virtual functions, usable for inference) after a software activation sequence targeting volatile SRAM. Power cycle resets it. The sequence must run on every boot.

On ESXi and Linux, AMD ships the activation mechanism — the `amdgpuv` VIB and the GIM kernel module respectively. On Windows: nothing. The community spent years fighting Code 43 with INF edits and vBIOS flashes. They were at the wrong layer.

The correct layer is the Microsemi Switchtec PSX Gen3 PCIe fabric switch embedded in the V340L. On Linux and ESXi, AMD's drivers handle this switch invisibly. On Windows, it requires an explicit driver and an MRPC activation command before the GPU silicon will respond to anything.

This repo documents that layer and three theoretical paths to activation. **The hardware has not been validated yet. Everything here is pre-empirical.**

---

## Status

**PRE-EMPIRICAL.** Card in transit. All three paths are fully specified from
primary sources. No activation has been attempted. No community precedent exists
for any of these paths on Windows. Hardware validation pending.

---

## The Key Finding — Theoretical

The V340L contains a Microsemi Switchtec PSX Gen3 PCIe fabric switch.
Activating the card on Windows requires a driver for this switch before
anything else can proceed.

That driver was found hiding in Intel's firmware package for the
AXXP3SWX08040 — a NVMe storage expansion AIC that uses the same Switchtec
silicon. The driver (`switchtec-kmdf-0.6`) is WHQL-signed, covers PSX Gen3
management endpoints via `VEN_11F8&DEV_854x&CC_058000`, and has a confirmed
fix removing the NVMe-present requirement for driver load.

This is a significant pre-hardware finding. Whether it actually works with
the V340L is an empirical question that Day 1 will answer.

The finding went undiscovered by the community for years because:

- Nobody identified the Switchtec as a required Windows driver target
- The driver is indexed under Intel storage AIC terms, not GPU terms
- The search namespaces — GPU virtualization vs NVMe fabric management —
  have zero overlap

It was found by stepping outside the conventional GPU search namespace,
identifying the switch vendor from teardown documentation, and searching
for Windows driver support for that specific silicon family. The driver
was then verified against primary source character-for-character before
any claims were made about it.

**This is a theory, not a result. The card is in transit.**

---

## Three Paths

**Path A — v340ctl KMDF**
Windows-native KMDF kernel driver replicates AMD's GIM activation sequence
from primary source headers. `DEV_686C` VFs passed into a Windows guest via
Hyper-V DDA. Fully specified from AMD kernel source headers. No community
precedent. No hardware validation.

**Path B — ESXi nested via Hyper-V DDA**
Windows host passes raw `6864` PFs to a nested ESXi guest via DDA. AMD's
native `amdgpuv` VIB handles activation inside ESXi. The `amdgpuv` VIB
descriptor explicitly lists Lenovo subsystem ID `17AA0C00` — the P520 is
AMD and Lenovo's co-certified reference platform for this card. Highest
prior probability of success given AMD co-certified this exact hardware
combination. Still unvalidated on this system.

**Path C — Switchtec userspace daemon**
`switchtec-kmdf-0.6` (confirmed in Intel AIC package) binds to the
Switchtec management endpoint. A daemon compiled against libswitchtec calls
`switchtec_gfms_bind()` to open fabric routing, then services the GPU
mailbox protocol. Full `GFMS_BIND` payload structure confirmed from
`switchtec-user` source. The `fabric` CLI subcommand group is Linux-only —
the daemon must be compiled from source.

**Critical unknown for Path C:** whether `gfms_bind` alone triggers
`DEV_686C` enumeration, or whether the GPUIOV/MMIO sequence from Path A
is also required from the Windows host. Day 1 determines this.

---

## What the Research Has Established

Confirmed from primary source before hardware arrival:

- Switchtec management endpoint DEV IDs: `VEN_11F8&DEV_854x&CC_058000`
  covered by `switchtec-kmdf-0.6` INF — confirmed verbatim
- NVMe load dependency fixed in firmware B08C — confirmed verbatim from
  release notes
- `GFMS_BIND` wire format confirmed from `lib/switchtec.c`
- PDFID discovery method confirmed — `gfms_dump` reports per-port ranges
- `fabric` CLI group confirmed Linux-only — daemon required on Windows
- GPUIOV register offsets, FB encoding, mailbox opcodes, PF2VF checksum
  algorithm — all confirmed from AMD kernel source headers
- Official V340 Windows guest driver exists: AMD Radeon Pro Software for
  Enterprise 19.Q2 — native `686C` target, no INF edit required
- P520 is AMD/Lenovo co-certified reference platform per VIB descriptor
- Zero brick risk — activation targets volatile SRAM only

Open empirical gates — only hardware can close these:

- Does the Switchtec endpoint enumerate in Windows Device Manager?
- Is the DEV ID in the INF coverage range?
- Does `switchtec-kmdf-0.6` bind cleanly on Windows 11?
- Does `gfms_bind` alone trigger `DEV_686C`, or is MMIO also required?
- Does `DEV_686C` load in a Hyper-V DDA guest?
- Does llama.cpp generate tokens?

---

## Day 1

Three PCIe devices must enumerate: two `DEV_6864` PFs and one Switchtec
management endpoint. If fewer than three enumerate, the riser or power
supply is the problem, not the activation sequence.

From there the decision tree is in `BRIEF.md`.

---

## Files

| File | Description |
|---|---|
| `BRIEF.md` | Complete technical record — register values, gate table, implementation sequence, source registry |
| `SWITCHTEC.md` | Switchtec driver and protocol findings — Intel AIC package analysis, GFMS_BIND payload, platform constraints |

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
fabrication log in `BRIEF.md` Section 12 records where AI-generated
values were wrong and why the methodology caught them.

The card is in transit. Day 1 will tell us if the theory holds.
