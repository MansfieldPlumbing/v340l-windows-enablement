# v340l-windows-enablement

## It's three characters.

The AMD Radeon Pro V340L — the $50 ex-Google Stadia dual-die Vega 10 GPU —
works on bare metal Windows with a single INF edit. No hypervisor. No SR-IOV
activation. No Switchtec management. No vBIOS flash.

**Change `REV_03` to `REV_05` in the AMD driver INF. Install. Done.**

Each V340L card has two independent Vega 10 dies. Each die: 3584 shaders,
8 GB HBM2, 483.8 GB/s bandwidth, OpenCL 2.0, Vulkan, DirectX 12.
Two cards = four dies = 14,336 shaders, 32 GB HBM2, ~1.9 TB/s aggregate.

---

## Quick Start (5 minutes)

### 1. Disable driver signature enforcement

The INF edit invalidates the catalog signature. Requires elevated PowerShell:

```powershell
bcdedit /set testsigning on
bcdedit /set nointegritychecks on
shutdown /r /t 0
```

### 2. Download the AMD V340 driver

From AMD's official download page:
[Radeon Pro V340 Drivers](https://www.amd.com/en/support/downloads/drivers.html/graphics/radeon-pro/radeon-pro-v-series/radeon-pro-v340.html)

Select: **Radeon Pro Software Enterprise 19.Q2 for V340** (Windows 10 64-bit, under the ESXi 6.7 section).

Direct link (as of April 2026):
```
https://drivers.amd.com/drivers/firepro/win10-64bit-radeon-pro-software-enterprise-19.q2-for-v340-june18.exe
```

Extract the installer.

### 3. Edit one line in the INF

The display driver INF is at `Packages\Drivers\Display\WT6A_INF\C0343610.inf`.
Edit both `C0343610.inf` and `U0343610.inf` (the non-DCH variant, present on
older Windows installs) — same change in both:

```powershell
foreach ($inf in @("C0343610.inf", "U0343610.inf")) {
    $path = "path\to\Packages\Drivers\Display\WT6A_INF\$inf"
    if (Test-Path $path) {
        (Get-Content $path -Raw) -replace 'DEV_6864&REV_03','DEV_6864&REV_05' | Set-Content $path -NoNewline
    }
}
```

### 4. Install via Device Manager

1. Right-click the **Video Controller** (yellow bang) → **Update driver**
2. **Browse my computer** → **Let me pick from a list** → **Have Disk**
3. Browse to the `WT6A_INF` folder → select `C0343610.inf`
4. Select **"Radeon Pro V340"** (not "MxGPU") → Next → Accept warning
5. Repeat for the second die

Both dies appear under **Display adapters** as "Radeon Pro V340". Status: OK.
No Code 43. No Code 10.

---

## Confirmed Working

| | |
|---|---|
| **Card** | AMD Radeon Pro V340L (ex-Google Stadia, SUBSYS_0C001002, REV_05) |
| **Host** | Lenovo ThinkStation P520 |
| **CPU** | Intel Xeon W-2145 |
| **OS** | Windows 11 Pro 24H2 (Build 26100) |
| **Driver** | Radeon Pro Software Enterprise 19.Q2 |
| **Cards tested** | 2 simultaneously (4 dies, all operational) |
| **Coexistence** | NVIDIA Quadro P2000 as primary display — no conflict |

![Four dies operational — GPU-Z confirmation](gpuz-four-dies.png)

### Per Die
- 56 Compute Units / 3584 Shaders
- 8192 MB HBM2 @ 945 MHz (483.8 GB/s)
- 1500 MHz GPU Clock
- OpenCL 2.0, Vulkan, OpenGL 4.6, DirectX 12 (FL 12_1), DirectML
- PCIe x16 Gen3

### OpenCL Verified
```
Number of devices:    4
Name:                 gfx901
Max compute units:    56
Max clock frequency:  1500Mhz
Global memory size:   8573157376
Available:            Yes
Compiler available:   Yes
```

---

## Why This Was Missed

The community spent years fighting Code 43 through:
- vBIOS flashes (Vega 56/64 ROMs)
- Switchtec PCIe switch reverse engineering
- GIM kernel module SR-IOV activation sequences
- Nested ESXi passthrough

All of this assumed DEV_6864 (the physical function) was inert and needed
to be converted to DEV_686C (virtual function) through a software activation
sequence. This assumption was wrong.

**DEV_6864 is a fully functional GPU.** AMD's driver INF has install sections
for both 6864 and 686C. The 6864 sections just didn't match the silicon
revision of the ex-Stadia cards (REV_05 vs the original V340's REV_03).

Windows reports "no compatible driver" identically for both missing drivers
and revision mismatches. There is no UI indication that a driver exists for
your exact device ID but targets a different revision. The error was
invisible at the layer everyone was looking.

---

## What About SR-IOV / MxGPU / Virtual Functions?

The SR-IOV activation path documented in this repo (BRIEF.md Sections 7–8)
is still valid and required if you want to:

- Split each die into multiple virtual functions (DEV_686C)
- Run multiple isolated VMs per die
- Use MxGPU time-sliced GPU scheduling

For using each die as a single, undivided 8 GB GPU — which is what you want
for local inference, compute, or rendering — the INF edit is all you need.

---

## The Switchtec

The Microsemi Switchtec PFX 48xG3 on the V340L is a transparent PCIe bridge.
Windows' inbox pci.sys handles it automatically. It routes lanes to the two
downstream dies. It does not gate, lock, or manage GPU access in any way.

AMD suppressed the Switchtec management endpoint in firmware. There is no
host-visible MRPC surface. The switch is a patch panel, not a gatekeeper.

---

## Files

| File | Description |
|---|---|
| **ADDENDUM12.md** | **Day 3 — Bare metal activation achieved. Full technical record.** |
| BRIEF.md | SR-IOV activation research — register values, gate table, GPUIOV sequence |
| SWITCHTEC.md | Switchtec driver and MRPC findings (valid but unnecessary for PF usage) |
| ADDENDUM10.md | Day 1 — Hardware reconnaissance, topology confirmed |
| ADDENDUM11.md | Day 2 — Switchtec driver bind, Code 10 resolved |
| SURVEYDDA_OUTPUT.txt | Raw SurveyDDA output — PCIe topology |

---

## Community Prior Art

User **kowmangler** on the Proxmox forum independently achieved the same
result on Debian 13 / Proxmox in Nov 2025, passing raw DEV_6864 PFs to
VMs via VFIO. The Windows guest driver required the same INF revision edit.
Linux guests used standard amdgpu with no modification.

Thread: `forum.proxmox.com/threads/shared-gpu-for-vdi-radeon-pro-v340.170416/`

---

## License

MIT
