# ADDENDUM 13 — DAY 4: WEBGPU INFERENCE ACHIEVED

## AMD Radeon Pro V340L MxGPU Windows Activation Project
## Date: April 10, 2026
## Status: SUCCESS — Native Windows inference operational via WebGPU/Dawn.

---

## Summary

Native LLM inference on the V340L under Windows 10 IoT LTSC with the stock
19.Q2 driver is operational. The API translation chain
`WebGPU → Google Dawn → D3D12 → 19.Q2 driver` targets the card's DirectX 12
Feature Level 12_1 implementation directly, bypassing the Vulkan 1.2 extension
gap that blocked the llama.cpp Vulkan backend.

**First empirical baseline — Qwen3.5-9B-Q3_K_S, single die:**

| Test | Result |
|---|---|
| Prompt Processing (pp512) | **20.40 t/s** |
| Token Generation (tg128) | **6.70 t/s** |

Hardware: Lenovo ThinkStation P520, Intel Xeon W-2145, AMD Radeon Pro V340L
(gfx901, D3D12 FL 12_1, driver 26.20.11016.1), NVIDIA Quadro P2000 (display).

> **Note:** These numbers were obtained while Dawn fell back to the legacy FXC
> shader compiler due to the DXC preload bug described below. With DXC active,
> prompt processing will be higher.

---

## The Adapter Selection Trap

Dawn's `RequestAdapter` defaults to the `DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE`
adapter. On the P520 reference platform, this is the NVIDIA Quadro P2000
(primary display). Pascal lacks native `ShaderF16` support, causing llama.cpp's
WebGPU backend to assert on initialization:

```text
GGML_ASSERT(ctx->webgpu_global_ctx->adapter.HasFeature(wgpu::FeatureName::ShaderF16)) failed
```

The V340L exposes Vega 10's Rapid Packed Math as `ShaderF16` cleanly through the
19.Q2 D3D12 driver. Dawn just wasn't asking it first.

**The fix** enumerates DXGI adapters after the failed default selection and
binds to the first adapter that passes `HasFeature(ShaderF16)`. No hardcoded
device names. The patch was submitted to upstream llama.cpp as PR #21744.

The cleaned, deduplicated LUID fallback (the version in the PR):

```cpp
#ifdef _WIN32
    if (!ctx->webgpu_global_ctx->adapter.HasFeature(wgpu::FeatureName::ShaderF16)) {
        IDXGIFactory6* f6 = nullptr;
        if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory6), (void**)&f6))) {
            IDXGIAdapter1* dxgi_adapter = nullptr;
            for (UINT i = 0; f6->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                    __uuidof(IDXGIAdapter1), (void**)&dxgi_adapter) != DXGI_ERROR_NOT_FOUND; i++) {
                DXGI_ADAPTER_DESC1 desc;
                if (SUCCEEDED(dxgi_adapter->GetDesc1(&desc))) {
                    struct LUIDOpts : wgpu::ChainedStruct { ::LUID adapterLUID; };
                    LUIDOpts lo{};
                    lo.sType = static_cast<wgpu::SType>(0x0005000C);
                    lo.nextInChain = nullptr;
                    lo.adapterLUID = desc.AdapterLuid;
                    wgpu::RequestAdapterOptions luid_opts;
                    luid_opts.backendType = wgpu::BackendType::D3D12;
                    luid_opts.nextInChain = &lo;
                    wgpu::Adapter candidate = nullptr;
                    ctx->webgpu_global_ctx->instance.WaitAny(
                        ctx->webgpu_global_ctx->instance.RequestAdapter(
                            &luid_opts, wgpu::CallbackMode::AllowSpontaneous,
                            [&candidate](wgpu::RequestAdapterStatus s, wgpu::Adapter a, const char*) {
                                if (s == wgpu::RequestAdapterStatus::Success)
                                    candidate = std::move(a);
                            }),
                        UINT64_MAX);
                    if (candidate && candidate.HasFeature(wgpu::FeatureName::ShaderF16)) {
                        char s[256]{}; size_t n = 0;
                        wcstombs_s(&n, s, desc.Description, _TRUNCATE);
                        GGML_LOG_INFO("ggml_webgpu: default adapter lacks ShaderF16 - falling back to %s\n", s);
                        ctx->webgpu_global_ctx->adapter = std::move(candidate);
                        dxgi_adapter->Release();
                        break;
                    }
                }
                dxgi_adapter->Release();
            }
            f6->Release();
        }
    }
#endif
```

---

## The DXC Preload Bug (Error 87)

Console output included:

```text
Error: DynamicLib.Open: d3dcompiler_47.dll Windows Error: 87
    at EnsureFXC (PlatformFunctions.cpp:117)
```

This is Dawn probing for the legacy FXC compiler after failing to find DXC.
FXC tops out at Shader Model 5.1 and cannot optimize 16-bit compute shaders.

**Root cause:** Dawn's internal `LoadLibraryEx` uses
`LOAD_LIBRARY_SEARCH_DEFAULT_DIRS`, which excludes the application's working
directory. A relative-path `LoadLibraryA("dxcompiler.dll")` call does nothing
useful before Dawn's search runs.

**Fix** — resolve from executable directory before `wgpu::CreateInstance`:

```cpp
#ifdef _WIN32
    {
        char exe_path[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
        std::string dir(exe_path);
        size_t last_slash = dir.find_last_of("\\/");
        if (last_slash != std::string::npos) {
            dir = dir.substr(0, last_slash + 1);
            LoadLibraryA((dir + "dxil.dll").c_str());       // must precede dxcompiler
            LoadLibraryA((dir + "dxcompiler.dll").c_str());
        }
    }
#endif
```

The `Error 87` warning on `d3dcompiler_47.dll` is harmless noise — it fires
because Dawn probes the legacy path after DXC is already loaded. If ShaderF16
is working, DXC is active regardless of the warning.

---

## SR-IOV Topology Revealed

Four `llama-cli.exe` processes launched simultaneously all bound to the same
DXGI adapter (GPU 0), exhausted its memory, and caused `DXGI_ERROR_DEVICE_HUNG`.

PowerShell interrogation of the PCI device tree via `Get-PnpDevice` and
`Get-PnpDeviceProperty` mapped the four dies to distinct PCI buses:

| Device | Bus | Card | Die |
|---|---|---|---|
| `v340-slot6-b` | 108 | Card A | Die 0 |
| `v340-slot6-a` | 105 | Card A | Die 1 |
| `v340-slot1-b` | 186 | Card B | Die 0 |
| `v340-slot1-a` | 183 | Card B | Die 1 |

The Switchtec PFX fabric switch is presenting each die as an independent SR-IOV
virtual function on a separate PCI bus. Standard DXGI enumeration collapses
these into `GPU 0` through `GPU 3`, but all four map to distinct LUIDs.

To distribute inference load across all four dies, processes must be pinned to
specific LUIDs — which is exactly what the DirectPort adapter selection logic
provides.

---

## A Note on Driver Versions

The REV_03 → REV_05 INF edit has only been validated against the 19.Q2 driver.
Testing with a later AMD driver package applied the same edit but the driver
crashed at device polling during initialization. The root cause is likely a
changed polling contract between the newer driver and the hardware — the 19.Q2
driver was written for this exact silicon. Enabling a later driver would require
a KMDF shim to intercept and satisfy the polling sequence. That work is deferred.

**For now: 19.Q2 is the production path. Do not attempt this with a newer driver.**

---

## Next Phase: DirectPort D3D12 Pipeline Parallelism

D3D12 is confirmed as the viable compute backend via WebGPU/Dawn. The project
now moves to multi-die scaling.

Because each die is an independent PCIe virtual function isolated by the
Switchtec crossbar, standard in-process tensor parallelism will bottleneck on
PCIe bandwidth and OS-mediated synchronization. The solution is pipeline
parallelism across independent processes, one per die, connected by the
DirectPort SDK:

1. Each die receives an independent `llama-cli` process, selected by DXGI LUID.
2. DirectPort opens `D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER` NT Handles between processes.
3. Activations (~4 MB for a 9B model) are passed die-to-die via `dp12_signal_fence` / `dp12_queue_wait`.
4. Execution pipelines speculatively through the Switchtec PCIe 3.0 x16 fabric.

Gate 19 (D3D12 `CrossNodeSharingTier` check) is the immediate next empirical step.