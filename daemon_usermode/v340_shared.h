#pragma once
#include <devioctl.h>

// {8B35A0F5-46D9-4EAA-91EA-A1521BA90DB0}
DEFINE_GUID(GUID_DEVINTERFACE_V340_MAPPER, 
0x8b35a0f5, 0x46d9, 0x4eaa, 0x91, 0xea, 0xa1, 0x52, 0x1b, 0xa9, 0xd, 0xb0);

#define IOCTL_V340_GET_BAR_POINTERS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _V340_BAR_INFO {
    PVOID bar0_user_ptr;
    size_t bar0_size;
    PVOID bar2_user_ptr;
    size_t bar2_size;
} V340_BAR_INFO, *PV340_BAR_INFO;