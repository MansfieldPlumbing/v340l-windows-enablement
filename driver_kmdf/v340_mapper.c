#include <ntddk.h>
#include <wdf.h>
#include "v340_shared.h"

typedef struct _DEVICE_CONTEXT {
    PVOID bar0_kernel_ptr;
    PMDL  bar0_mdl;
    PVOID bar0_user_ptr;
    size_t bar0_size;

    PVOID bar2_kernel_ptr;
    PMDL  bar2_mdl;
    PVOID bar2_user_ptr;
    size_t bar2_size;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

EVT_WDF_DRIVER_DEVICE_ADD EvtDriverDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE EvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE EvtDeviceReleaseHardware;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoDeviceControl;

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, EvtDriverDeviceAdd);
    return WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
}

NTSTATUS EvtDriverDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit) {
    WdfDeviceInitAssignName(DeviceInit, NULL); 
    
    // [FIX APPLIED] Register PnP Power Event Callbacks so hardware preparation actually runs!
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

    status = WdfDeviceCreateDeviceInterface(device, &GUID_DEVINTERFACE_V340_MAPPER, NULL);
    if (!NT_SUCCESS(status)) return status;

    WDF_IO_QUEUE_CONFIG queueConfig;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
    queueConfig.EvtIoDeviceControl = EvtIoDeviceControl;

    WDFQUEUE queue;
    return WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
}

NTSTATUS EvtDevicePrepareHardware(WDFDEVICE Device, WDFCMRESLIST ResourcesRaw, WDFCMRESLIST ResourcesTranslated) {
    UNREFERENCED_PARAMETER(ResourcesRaw);
    PDEVICE_CONTEXT ctx = GetDeviceContext(Device);
    int bar_count = 0;

    for (ULONG i = 0; i < WdfCmResourceListGetCount(ResourcesTranslated); i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR desc = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
        
        if (desc->Type == CmResourceTypeMemory) {
            PVOID mapped_ptr = MmMapIoSpace(desc->u.Memory.Start, desc->u.Memory.Length, MmNonCached);
            if (!mapped_ptr) continue;

            PMDL mdl = IoAllocateMdl(mapped_ptr, (ULONG)desc->u.Memory.Length, FALSE, FALSE, NULL);
            MmBuildMdlForNonPagedPool(mdl);
            
            PVOID user_ptr = NULL;
            __try {
                user_ptr = MmMapLockedPagesSpecifyCache(mdl, UserMode, MmNonCached, NULL, FALSE, NormalPagePriority);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                IoFreeMdl(mdl);
                MmUnmapIoSpace(mapped_ptr, desc->u.Memory.Length);
                continue;
            }

            if (bar_count == 0) { // BAR0 (Registers)
                ctx->bar0_kernel_ptr = mapped_ptr;
                ctx->bar0_mdl = mdl;
                ctx->bar0_user_ptr = user_ptr;
                ctx->bar0_size = desc->u.Memory.Length;
            } else if (bar_count == 2) { // BAR2/5 (Framebuffer)
                ctx->bar2_kernel_ptr = mapped_ptr;
                ctx->bar2_mdl = mdl;
                ctx->bar2_user_ptr = user_ptr;
                ctx->bar2_size = desc->u.Memory.Length;
            }
            bar_count++;
        }
    }
    return STATUS_SUCCESS;
}

NTSTATUS EvtDeviceReleaseHardware(WDFDEVICE Device, WDFCMRESLIST ResourcesTranslated) {
    UNREFERENCED_PARAMETER(ResourcesTranslated);
    PDEVICE_CONTEXT ctx = GetDeviceContext(Device);

    if (ctx->bar0_mdl) {
        MmUnmapLockedPages(ctx->bar0_user_ptr, ctx->bar0_mdl);
        IoFreeMdl(ctx->bar0_mdl);
        MmUnmapIoSpace(ctx->bar0_kernel_ptr, ctx->bar0_size);
    }
    if (ctx->bar2_mdl) {
        MmUnmapLockedPages(ctx->bar2_user_ptr, ctx->bar2_mdl);
        IoFreeMdl(ctx->bar2_mdl);
        MmUnmapIoSpace(ctx->bar2_kernel_ptr, ctx->bar2_size);
    }
    return STATUS_SUCCESS;
}

void EvtIoDeviceControl(WDFQUEUE Queue, WDFREQUEST Request, size_t OutputBufferLength, size_t InputBufferLength, ULONG IoControlCode) {
    UNREFERENCED_PARAMETER(InputBufferLength);
    PDEVICE_CONTEXT ctx = GetDeviceContext(WdfIoQueueGetDevice(Queue));
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    size_t bytesReturned = 0;

    if (IoControlCode == IOCTL_V340_GET_BAR_POINTERS) {
        if (OutputBufferLength >= sizeof(V340_BAR_INFO)) {
            PV340_BAR_INFO info;
            status = WdfRequestRetrieveOutputBuffer(Request, sizeof(V340_BAR_INFO), (PVOID*)&info, NULL);
            if (NT_SUCCESS(status)) {
                info->bar0_user_ptr = ctx->bar0_user_ptr;
                info->bar0_size = ctx->bar0_size;
                info->bar2_user_ptr = ctx->bar2_user_ptr;
                info->bar2_size = ctx->bar2_size;
                bytesReturned = sizeof(V340_BAR_INFO);
            }
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
    }
    WdfRequestCompleteWithInformation(Request, status, bytesReturned);
}