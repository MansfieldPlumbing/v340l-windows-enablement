#include <ntddk.h>
#include <wdf.h>
#include "v340_shared.h"

typedef struct _DEVICE_CONTEXT {
    PVOID  bar0_kernel_ptr;
    PMDL   bar0_mdl;
    PVOID  bar0_user_ptr;       // valid only inside daemon process context
    size_t bar0_size;

    PVOID  bar2_kernel_ptr;
    PMDL   bar2_mdl;
    PVOID  bar2_user_ptr;       // valid only inside daemon process context
    size_t bar2_size;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

EVT_WDF_DRIVER_DEVICE_ADD      EvtDriverDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE EvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE EvtDeviceReleaseHardware;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoDeviceControl;
EVT_WDF_FILE_CLEANUP            EvtFileCleanup;

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, EvtDriverDeviceAdd);
    return WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
}

NTSTATUS EvtDriverDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit) {
    UNREFERENCED_PARAMETER(Driver);

    // NO hardcoded device name or symbolic link.
    // The V340L has two independent Vega10 dies. PnP loads this driver once
    // per die. A hardcoded \DosDevices\V340Mapper name causes
    // STATUS_OBJECT_NAME_COLLISION on the second instance — Die 1 gets
    // Code 10 before the daemon starts. GUID-based interface is the correct
    // multi-instance pattern: each instance gets a unique \\?\... path,
    // the daemon enumerates both via SetupDiGetClassDevs.

    // Register EvtFileCleanup so WDF calls it in the closing process's
    // thread context — the only safe place to call MmUnmapLockedPages
    // on a UserMode mapping.
    WDF_FILEOBJECT_CONFIG fileConfig;
    WDF_FILEOBJECT_CONFIG_INIT(&fileConfig,
        WDF_NO_EVENT_CALLBACK,   // EvtFileCreate
        WDF_NO_EVENT_CALLBACK,   // EvtFileClose
        EvtFileCleanup);         // EvtFileCleanup — runs in daemon process context
    WdfDeviceInitSetFileObjectConfig(DeviceInit, &fileConfig, WDF_NO_OBJECT_ATTRIBUTES);

    WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);
    pnpCallbacks.EvtDevicePrepareHardware = EvtDevicePrepareHardware;
    pnpCallbacks.EvtDeviceReleaseHardware = EvtDeviceReleaseHardware;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpCallbacks);

    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

    WDFDEVICE device;
    NTSTATUS status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);
    if (!NT_SUCCESS(status)) return status;

    // Expose via GUID interface only — daemon discovers both instances
    // using SetupDiGetClassDevs against GUID_DEVINTERFACE_V340_MAPPER.
    status = WdfDeviceCreateDeviceInterface(device, &GUID_DEVINTERFACE_V340_MAPPER, NULL);
    if (!NT_SUCCESS(status)) return status;

    WDF_IO_QUEUE_CONFIG queueConfig;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
    queueConfig.EvtIoDeviceControl = EvtIoDeviceControl;

    WDFQUEUE queue;
    return WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
}

// PHASE 1 — Kernel-side only.
// Map BAR physical addresses into kernel VA space, build MDLs.
// No UserMode mapping here — this runs in the PnP manager (System process).
// UserMode mapping in System process context would produce VAs valid only
// in PID 4, causing an access violation the moment the daemon dereferences them.
NTSTATUS EvtDevicePrepareHardware(WDFDEVICE Device, WDFCMRESLIST ResourcesRaw, WDFCMRESLIST ResourcesTranslated) {
    UNREFERENCED_PARAMETER(ResourcesRaw);
    PDEVICE_CONTEXT ctx = GetDeviceContext(Device);

    for (ULONG i = 0; i < WdfCmResourceListGetCount(ResourcesTranslated); i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR desc = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);

        if (desc->Type == CmResourceTypeMemory) {

            PVOID kernel_ptr = MmMapIoSpaceEx(desc->u.Memory.Start,
                                              desc->u.Memory.Length,
                                              PAGE_READWRITE | PAGE_NOCACHE);
            if (!kernel_ptr) continue;

            PMDL mdl = IoAllocateMdl(kernel_ptr, (ULONG)desc->u.Memory.Length,
                                     FALSE, FALSE, NULL);
            if (!mdl) {
                MmUnmapIoSpace(kernel_ptr, desc->u.Memory.Length);
                continue;
            }
            MmBuildMdlForNonPagedPool(mdl);

            // BAR selection by size:
            // < 16MB  -> MMIO register BAR (BAR0)
            // 256MB+  -> Framebuffer BAR (BAR2)
            if (desc->u.Memory.Length < (16 * 1024 * 1024)) {
                ctx->bar0_kernel_ptr = kernel_ptr;
                ctx->bar0_mdl        = mdl;
                ctx->bar0_size       = desc->u.Memory.Length;
                // bar0_user_ptr intentionally left NULL until IOCTL
            } else {
                ctx->bar2_kernel_ptr = kernel_ptr;
                ctx->bar2_mdl        = mdl;
                ctx->bar2_size       = desc->u.Memory.Length;
                // bar2_user_ptr intentionally left NULL until IOCTL
            }
        }
    }
    return STATUS_SUCCESS;
}

// PHASE 2 — UserMode mapping, on first IOCTL call.
// WDF routes standard IOCTLs in the calling thread's context, which is the
// daemon thread. The UserMode VAs produced here are valid in the daemon's
// address space and nowhere else.
VOID EvtIoDeviceControl(WDFQUEUE Queue, WDFREQUEST Request,
                        size_t OutputBufferLength, size_t InputBufferLength,
                        ULONG IoControlCode) {
    UNREFERENCED_PARAMETER(InputBufferLength);
    PDEVICE_CONTEXT ctx = GetDeviceContext(WdfIoQueueGetDevice(Queue));
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    size_t bytesReturned = 0;

    if (IoControlCode == IOCTL_V340_GET_BAR_POINTERS) {
        if (OutputBufferLength >= sizeof(V340_BAR_INFO)) {
            PV340_BAR_INFO info;
            status = WdfRequestRetrieveOutputBuffer(Request, sizeof(V340_BAR_INFO),
                                                    (PVOID*)&info, NULL);
            if (NT_SUCCESS(status)) {

                // Map BAR0 UserMode VA on first call
                if (ctx->bar0_mdl && ctx->bar0_user_ptr == NULL) {
                    __try {
                        ctx->bar0_user_ptr = MmMapLockedPagesSpecifyCache(
                            ctx->bar0_mdl, UserMode, MmNonCached,
                            NULL, FALSE, NormalPagePriority);
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        ctx->bar0_user_ptr = NULL;
                    }
                }

                // Map BAR2 UserMode VA on first call
                if (ctx->bar2_mdl && ctx->bar2_user_ptr == NULL) {
                    __try {
                        ctx->bar2_user_ptr = MmMapLockedPagesSpecifyCache(
                            ctx->bar2_mdl, UserMode, MmNonCached,
                            NULL, FALSE, NormalPagePriority);
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        ctx->bar2_user_ptr = NULL;
                    }
                }

                info->bar0_user_ptr = ctx->bar0_user_ptr;
                info->bar0_size     = ctx->bar0_size;
                info->bar2_user_ptr = ctx->bar2_user_ptr;
                info->bar2_size     = ctx->bar2_size;
                bytesReturned = sizeof(V340_BAR_INFO);
            }
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
    }
    WdfRequestCompleteWithInformation(Request, status, bytesReturned);
}

// PHASE 3 — UserMode cleanup, in daemon process context.
// Called by WDF when the daemon calls CloseHandle() or exits.
// MmUnmapLockedPages MUST run in the same process context that created
// the UserMode mapping. If this ran in ReleaseHardware (System process),
// Windows would bugcheck with PROCESS_HAS_LOCKED_PAGES.
VOID EvtFileCleanup(WDFFILEOBJECT FileObject) {
    PDEVICE_CONTEXT ctx = GetDeviceContext(WdfFileObjectGetDevice(FileObject));

    if (ctx->bar0_user_ptr && ctx->bar0_mdl) {
        MmUnmapLockedPages(ctx->bar0_user_ptr, ctx->bar0_mdl);
        ctx->bar0_user_ptr = NULL;
    }
    if (ctx->bar2_user_ptr && ctx->bar2_mdl) {
        MmUnmapLockedPages(ctx->bar2_user_ptr, ctx->bar2_mdl);
        ctx->bar2_user_ptr = NULL;
    }
}

// PHASE 4 — Kernel cleanup.
// Free MDLs and unmap kernel-side IO space.
// UserMode pointers are already NULL by the time this runs
// (EvtFileCleanup fires before device removal).
NTSTATUS EvtDeviceReleaseHardware(WDFDEVICE Device, WDFCMRESLIST ResourcesTranslated) {
    UNREFERENCED_PARAMETER(ResourcesTranslated);
    PDEVICE_CONTEXT ctx = GetDeviceContext(Device);

    if (ctx->bar0_mdl) {
        IoFreeMdl(ctx->bar0_mdl);
        ctx->bar0_mdl = NULL;
    }
    if (ctx->bar0_kernel_ptr) {
        MmUnmapIoSpace(ctx->bar0_kernel_ptr, ctx->bar0_size);
        ctx->bar0_kernel_ptr = NULL;
    }
    if (ctx->bar2_mdl) {
        IoFreeMdl(ctx->bar2_mdl);
        ctx->bar2_mdl = NULL;
    }
    if (ctx->bar2_kernel_ptr) {
        MmUnmapIoSpace(ctx->bar2_kernel_ptr, ctx->bar2_size);
        ctx->bar2_kernel_ptr = NULL;
    }
    return STATUS_SUCCESS;
}
