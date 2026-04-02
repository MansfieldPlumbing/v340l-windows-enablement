# ADDENDUM 12 — DAY 3: BARE METAL ACTIVATION ACHIEVED

## AMD Radeon Pro V340L MxGPU Windows Activation Project
## Date: April 2, 2026
## Status: COMPLETE — Four dies operational on bare metal Windows 11 Pro. No hypervisor. No activation daemon. No Switchtec MRPC.

---

## Summary

The V340L does not require SR-IOV activation, Switchtec management commands,
GPUIOV register writes, or any hypervisor layer to function as a GPU on Windows.

The DEV_6864 physical function is a fully operational Vega 10 GPU. AMD's official
19.Q2 V340 Windows driver natively supports DEV_6864 as a direct install target.
The only barrier to installation on ex-Google Stadia V340L cards is a silicon
revision mismatch in the driver INF: the driver specifies `REV_03` (original V340),
while Stadia-era cards report `REV_05` (later manufacturing run, same die).

**The fix is a three-character INF edit.** Change `REV_03` to `REV_05`. Install
via Device Manager "Have Disk". Both dies activate immediately. No Code 43.
No Code 10. Full OpenCL, Vulkan, DirectCompute, DirectML. Confirmed on two
cards (four dies) simultaneously.

---

## What Was Done

### Prerequisites

1. Disable driver signature enforcement (required because the INF edit
   invalidates the catalog signature):
   ```
   bcdedit /set testsigning on
   bcdedit /set nointegritychecks on
   ```
   Reboot. "Test Mode" watermark will appear — this is expected.

### Driver Preparation

2. Download the official AMD V340 guest driver from:
   ```
   https://www.amd.com/en/support/downloads/drivers.html/graphics/radeon-pro/radeon-pro-v-series/radeon-pro-v340.html
   ```
   Select: **Radeon Pro Software Enterprise 19.Q2 for V340** (Windows 10 64-bit)
   Direct link:
   ```
   https://drivers.amd.com/drivers/firepro/win10-64bit-radeon-pro-software-enterprise-19.q2-for-v340-june18.exe
   ```

3. Run the installer to extract (or extract manually). The display driver INF is at:
   ```
   Packages\Drivers\Display\WT6A_INF\C0343610.inf
   ```

4. Edit `C0343610.inf` — replace all instances of the revision string:
   ```
   Find:    DEV_6864&REV_03
   Replace: DEV_6864&REV_05
   ```
   There are two lines to change (one per install section):
   ```
   "%AMD6864.1%" = ati2mtag_R7500, PCI\VEN_1002&DEV_6864&REV_05
   "%AMD6864.1%" = ati2mtag_R7500DS, PCI\VEN_1002&DEV_6864&REV_05
   ```

   PowerShell one-liner:
   ```powershell
   $inf = "path\to\C0343610.inf"
   (Get-Content $inf -Raw) -replace 'DEV_6864&REV_03','DEV_6864&REV_05' | Set-Content $inf -NoNewline
   ```

### Driver Installation

5. Open Device Manager. The V340L dies appear under **Other devices** as
   "Video Controller" with yellow warning icons.

6. Right-click a Video Controller → **Update driver** → **Browse my computer
   for drivers** → **Let me pick from a list** → **Have Disk** → browse to the
   `WT6A_INF` folder → select `C0343610.inf`.

7. Select **"Radeon Pro V340"** from the list (not "Radeon Pro V340 MxGPU" —
   that is the 686C VF driver). Click Next. Accept the unsigned driver warning.

8. Repeat for the second die.

9. Both dies appear under **Display adapters** as "Radeon Pro V340" with no
   error codes. Status: OK.

---

## Confirmed Working Configuration

| Component | Detail |
|-----------|--------|
| Card | AMD Radeon Pro V340L (ex-Google Stadia, SUBSYS_0C001002, REV_05) |
| Host | Lenovo ThinkStation P520 |
| CPU | Intel Xeon W-2145 |
| OS | Windows 11 Pro 24H2 (Build 26100) |
| Driver | Radeon Pro Software Enterprise 19.Q2 (26.20.11016.1) |
| Cards installed | 2 (4 dies total) |
| Primary display GPU | NVIDIA Quadro P2000 (coexists without conflict) |

---

## Confirmed Capabilities (Per Die)

| Property | Value |
|----------|-------|
| GPU | Vega 10 (gfx901) |
| Shaders | 3584 Unified |
| Compute Units | 56 |
| Max Clock | 1500 MHz |
| Memory | 8192 MB HBM2 (Hynix) |
| Memory Clock | 945 MHz |
| Memory Bandwidth | 483.8 GB/s |
| Bus Width | 2048-bit |
| Bus Interface | PCIe x16 Gen3 |
| OpenCL | 2.0 (AMD-APP 2841.5) |
| Vulkan | Yes (version TBD — 19.Q2 driver) |
| OpenGL | 4.6 |
| DirectCompute | Yes |
| DirectML | Yes |
| DirectX | 12 (Feature Level 12_1) |
| Resizable BAR | Enabled |

**Aggregate (4 dies):** 14,336 shaders, 32 GB HBM2, ~1.9 TB/s memory bandwidth.

---

## Why This Was Missed

The V340L community spent years attempting to activate the card through:

- vBIOS flashes (Vega 56/64 ROMs to bypass device ID checks)
- Switchtec PCIe fabric switch management (MRPC protocol reverse engineering)
- GIM kernel module replication (SR-IOV GPUIOV register sequences)
- Nested ESXi via Hyper-V DDA (passing PFs to VMware for native VIB activation)

All of these approaches assumed the DEV_6864 physical function was inert — a
locked management endpoint that required an activation sequence to produce
usable DEV_686C virtual functions. This assumption was wrong.

**The 6864 PF is a fully functional GPU.** AMD's own driver has install sections
for both 6864 (PF, "Radeon Pro V340") and 686C (VF, "Radeon Pro V340 MxGPU").
The 6864 sections were always in the INF. They simply didn't match the hardware
revision of the ex-Stadia cards.

The community searched for the answer in GPU virtualization, PCIe fabric
management, and hypervisor activation layers. The answer was in an INF file
that was already on AMD's download page.

**Contributing factors to the misdirection:**

1. **Revision mismatch was invisible.** Windows reports "no compatible driver"
   for both missing drivers and revision mismatches. There is no UI indication
   that a driver exists for your device ID but not your revision.

2. **The Stadia provenance created false assumptions.** Because the cards came
   from a cloud GPU virtualization platform, the community assumed
   virtualization was required to use them.

3. **DEV_6864 vs DEV_686C confusion.** The existence of two device IDs implied
   a locked/unlocked state. In reality, 6864 is the full physical GPU and
   686C is a virtual partition of it. Both are independently usable.

4. **Windows 11 24H2 GUI changes.** Build 26100 broke the "Have Disk" +
   "uncheck Show compatible hardware" flow that worked on earlier builds.
   This closed a path that might have led to accidental discovery.

---

## What the Switchtec Research Established

The Switchtec PFX 48xG3 fabric switch on the V340L operates in transparent
bridge mode. Windows' inbox pci.sys driver correctly bridges PCIe traffic
through the switch to both downstream dies without any management intervention.

The Switchtec management endpoint (Function 1, CC_058000) is suppressed in
the card's eNVM configuration. AMD configured the switch for transparent
pass-through only — no host-visible management surface.

**The Switchtec is a patch panel, not a gatekeeper.** It routes PCIe lanes.
It does not participate in GPU initialization, driver loading, or compute
operations. The driver communicates directly with each Vega 10 die through
standard PCIe BAR-mapped register access, routed transparently through the
PFX bridge.

The Switchtec driver bind achieved on Day 2 (ADDENDUM11) was technically
successful — the driver loaded and the service ran — but the management
endpoint it needed was never exposed by the hardware. This path was a dead
end not because the driver was wrong, but because the hardware was configured
to not need management.

---

## What the SR-IOV Research Remains Valid For

The GPUIOV register maps, PF2VF mailbox protocol, and activation sequence
documented in BRIEF.md Sections 7–8 are correct and verified against AMD's
kernel source headers. They are required for:

- Splitting each die into multiple virtual functions (DEV_686C)
- Running multiple VMs per die with isolated GPU partitions
- MxGPU time-sliced scheduling across VFs

For using each die as a single, undivided GPU — which provides the full
8 GB HBM2 and all 3584 shaders per die — none of this is needed. The
INF edit is sufficient.

---

## Proxmox Community Confirmation

Independent confirmation from the Proxmox community (forum thread:
`proxmox.com/threads/shared-gpu-for-vdi-radeon-pro-v340.170416/`):

User **kowmangler** (Nov 2025 — Jan 2026) achieved the same result on
Proxmox/Debian 13 — passing raw DEV_6864 PFs to VMs via VFIO without
SR-IOV activation. Each die functions as a complete GPU in the guest OS.
The Windows guest driver required the same INF revision edit. Linux guests
used the standard amdgpu driver with no modifications.

Key findings from their testing:
- Stock V340L BIOS required (Vega 56 vBIOS flash breaks FLR, causes hangs)
- Server board with SR-IOV, IOMMU, and Above 4G Addressing recommended
- FLR (Function Level Reset) support required for clean VM restart cycles
- 6840 TimeSpy score per die in Windows VM

---

## Next Steps

- [ ] Check Vulkan version exposed by 19.Q2 driver (need 1.2+ for llama.cpp Vulkan backend)
- [ ] Test OpenCL compute with llama.cpp CLBlast backend
- [ ] Test DirectPort crossbar latency between dies on separate cards
- [ ] Investigate newer community AMD drivers for updated Vulkan/OpenCL support
- [ ] Document thermal management for multi-card passive cooling configurations
- [ ] Update README.md with simplified activation instructions

---

## Gate Status — Final

| Gate | Question | Status |
|------|----------|--------|
| 1 | Switchtec enumerates | CLOSED — Yes |
| 2 | DEV_8533 in INF range | CLOSED — Yes |
| 3 | Switchtec service binds | CLOSED — Yes (but irrelevant) |
| 4 | Dual-die IOMMU groups | CLOSED — Independent |
| 5 | 686C on bare metal Win11 | **SUPERSEDED — 6864 works directly** |
| 11 | Switchtec driver bound | CLOSED — Yes (but no management endpoint) |
| 12 | Switchtec initializes | CLOSED — Code 10 resolved, but no switchtec0 device |
| **13** | **6864 PF usable as GPU** | **CLOSED — YES. INF edit only.** |
| **14** | **OpenCL compute live** | **CLOSED — 4 devices, gfx901, 1500 MHz, compiler available** |
| **15** | **Multi-card operation** | **CLOSED — 2 cards, 4 dies, all Status OK** |

---

## Methodology Note

This finding was reached by stepping outside the project's own assumptions.
After exhausting the Switchtec management path, a web search for community
V340L passthrough experiences on Proxmox revealed that direct PF usage was
already working on Linux — without any activation sequence. The Proxmox
user's Windows driver installation, which required the same revision-string
INF edit, directly informed the bare-metal Windows test that succeeded here.

The prior research was not wasted. The PCIe topology analysis, thermal
characterization, and Switchtec driver work established the hardware was
sound. The SR-IOV protocol documentation remains the only public reference
for V340L virtual function activation on Windows. But the simplest use case —
one GPU per die — required none of it.

Sometimes the answer is three characters in a text file.
