#include <windows.h>
#include <setupapi.h>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

#include "v340_shared.h"
#include "amdgv_sriovmsg.h"

extern "C" {
    #include "switchtec/switchtec.h"
}

// Immutable Vega10 BAR0 Byte Offsets
#define REG_MAILBOX_INDEX         0x3954
#define REG_MSGBUF_TRN_DW0        0x3958
#define REG_MSGBUF_TRN_DW2        0x3960
#define REG_MSGBUF_RCV_DW0        0x3968
#define REG_MAILBOX_CONTROL       0x3978

#define REG_RLC_GPU_IOV_VF_ENABLE 0x3EC00
#define REG_RLC_GPU_IOV_SCH_1     0x3ECEC

// Global struct to hold handles and mapping for all discovered dies
struct GpuDie {
    HANDLE hDevice;
    V340_BAR_INFO bars;
    int die_index;
};

std::vector<GpuDie> g_mapped_dies;

// --- SWITCHTEC BIND STRUCT ---
#pragma pack(push, 1)
struct gfms_bind_cmd {
    uint8_t  subcmd;              // MRPC_GFMS_BIND = 0x01
    uint8_t  host_sw_idx;
    uint8_t  host_phys_port_id;
    uint8_t  host_log_port_id;
    struct {
        uint16_t pdfid;           // Physical Device Function ID
        uint8_t  next_valid;      // 1 if another function follows, else 0
        uint8_t  reserved;        // zero
    } function[8];
};
#pragma pack(pop)

// --- GRACEFUL EXIT HANDLER ---
// Iterates all mapped dies so both handles are closed cleanly on Ctrl+C.
// CloseHandle triggers EvtFileCleanup in the KMDF driver, which unmaps
// the UserMode MDL pages in the correct process context.
BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        std::cout << "\n[!] Shutting down daemon safely... Unmapping BARs." << std::endl;
        for (auto& die : g_mapped_dies) {
            if (die.hDevice != INVALID_HANDLE_VALUE) {
                CloseHandle(die.hDevice);
            }
        }
        g_mapped_dies.clear();
        ExitProcess(0);
    }
    return TRUE;
}

unsigned int compute_checksum(void *obj, unsigned long obj_size, unsigned int key) {
    unsigned int ret = key;
    unsigned char *pos = (unsigned char *)obj;
    for (unsigned long i = 0; i < obj_size; ++i) {
        ret += *(pos + i);
    }
    return ret; 
}

inline void write32(void* base, uint32_t offset, uint32_t value) { 
    *(volatile uint32_t*)((uint8_t*)base + offset) = value; 
}

inline uint32_t read32(void* base, uint32_t offset) { 
    return *(volatile uint32_t*)((uint8_t*)base + offset); 
}

// Safe 32-bit Read-Modify-Write for MAILBOX_CONTROL byte fields.
// Raw byte writes to MMIO are not guaranteed safe on PCIe -- RMW prevents
// clobbering adjacent control bytes in the same DWORD.
inline void write8_safe(void* base, uint32_t reg_offset, uint8_t byte_index, uint8_t value) {
    uint32_t val32 = read32(base, reg_offset);
    uint32_t mask = ~(0xFF << (byte_index * 8));
    val32 = (val32 & mask) | (value << (byte_index * 8));
    write32(base, reg_offset, val32);
}

// Enumerate all loaded v340_mapper.sys instances via GUID and map their BARs.
// The mapper uses GUID-only device interfaces (no hardcoded symlink) so both
// die instances load cleanly. Each die gets its own handle and BAR mapping.
std::vector<GpuDie> map_all_dies() {
    std::vector<GpuDie> dies;

    HDEVINFO hDevInfo = SetupDiGetClassDevs(
        &GUID_DEVINTERFACE_V340_MAPPER, NULL, NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE) return dies;

    SP_DEVICE_INTERFACE_DATA devInterfaceData;
    devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0;
         SetupDiEnumDeviceInterfaces(hDevInfo, NULL,
             &GUID_DEVINTERFACE_V340_MAPPER, i, &devInterfaceData);
         i++)
    {
        DWORD reqSize = 0;
        SetupDiGetDeviceInterfaceDetailA(hDevInfo, &devInterfaceData,
                                         NULL, 0, &reqSize, NULL);

        std::vector<BYTE> detailBuffer(reqSize);
        PSP_DEVICE_INTERFACE_DETAIL_DATA_A detailData =
            (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)detailBuffer.data();
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

        if (!SetupDiGetDeviceInterfaceDetailA(hDevInfo, &devInterfaceData,
                detailData, reqSize, NULL, NULL)) {
            std::cerr << "[!] SetupDiGetDeviceInterfaceDetailA failed for index "
                      << i << std::endl;
            continue;
        }

        HANDLE hDevice = CreateFileA(detailData->DevicePath,
            GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hDevice == INVALID_HANDLE_VALUE) {
            std::cerr << "[!] CreateFileA failed for die " << i
                      << " error: " << GetLastError() << std::endl;
            continue;
        }

        V340_BAR_INFO bars = {0};
        DWORD bytesRet = 0;
        if (!DeviceIoControl(hDevice, IOCTL_V340_GET_BAR_POINTERS,
                NULL, 0, &bars, sizeof(bars), &bytesRet, NULL)) {
            std::cerr << "[!] IOCTL failed for die " << i
                      << " error: " << GetLastError() << std::endl;
            CloseHandle(hDevice);
            continue;
        }

        // Guard: IOCTL can return TRUE while kernel-side MDL mapping silently
        // failed. NULL pointers here would cause a deref crash in the mailbox
        // loop with no diagnostic output -- catch it here instead.
        if (bars.bar0_user_ptr == nullptr || bars.bar2_user_ptr == nullptr) {
            std::cerr << "[!] Die " << i << ": IOCTL succeeded but kernel returned NULL pointers." << std::endl;
            std::cerr << "    Check EvtDevicePrepareHardware -- MDL mapping may have failed." << std::endl;
            CloseHandle(hDevice);
            continue;
        }

        dies.push_back({hDevice, bars, (int)i});
        std::cout << "[+] Mapped Die " << i
                  << " | BAR0: " << bars.bar0_user_ptr
                  << " | BAR2: " << bars.bar2_user_ptr << std::endl;
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);

    // Day 1 diagnostic: if only one die mapped, the second driver instance
    // likely failed with STATUS_OBJECT_NAME_COLLISION or Code 10.
    // Check Device Manager for a second V340Mapper entry with an error.
    if (dies.size() == 1) {
        std::cerr << "[!] WARNING: Only one die mapped. Second die may have failed to load." << std::endl;
        std::cerr << "    Check Device Manager for Code 10 on the second V340Mapper instance." << std::endl;
    }

    return dies;
}

int main() {
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    std::cout << "[*] Starting V340L Ghost Hypervisor Daemon..." << std::endl;

    // =========================================================================
    // TODO: Update these variables from your `gfms_dump` CLI output on Day 1!
    // =========================================================================
    uint8_t  discovered_sw_idx       = 0x00; 
    uint8_t  discovered_phys_port    = 0x00; 
    uint8_t  discovered_log_port     = 0x00; 
    uint16_t discovered_pdfid_vf0    = 0x0000;
    uint16_t discovered_pdfid_vf1    = 0x0001; 
    // =========================================================================

    // --- PHASE 1: Switchtec Fabric Bind ---
    // "switchtec0" is parsed by switchtec_open() in switchtec.c via
    // sscanf("switchtec%d") -> switchtec_open_by_index(0)
    // -> SetupDiGetClassDevs enumeration against SWITCHTEC_INTERFACE_GUID
    // -> get_path() retrieves the real hardware device path from Windows
    // -> switchtec_open_by_path() appends the GUID and calls CreateFile. Correct path.
    // Do NOT use "\\\\.\\switchtec0" -- that bypasses the dispatch and calls
    // switchtec_open_by_path() directly with a raw string, which appends the
    // GUID to "\\\\.\\switchtec0" and produces an invalid CreateFile path.
    struct switchtec_dev* sw_dev = switchtec_open("switchtec0");
    if (sw_dev) {
        gfms_bind_cmd cmd = {0};
        cmd.subcmd            = 1; // MRPC_GFMS_BIND
        cmd.host_sw_idx       = discovered_sw_idx;
        cmd.host_phys_port_id = discovered_phys_port;
        cmd.host_log_port_id  = discovered_log_port;

        // Map Die 0
        cmd.function[0].pdfid      = discovered_pdfid_vf0;
        cmd.function[0].next_valid = 1; 

        // Map Die 1
        cmd.function[1].pdfid      = discovered_pdfid_vf1;
        cmd.function[1].next_valid = 0; // Terminate list

        uint8_t result[4] = {0};
        int ret = switchtec_cmd(sw_dev, 0x84 /* MRPC_GFMS_BIND_UNBIND */,
                                &cmd, sizeof(cmd), &result, sizeof(result));

        if (ret == 0 && result[0] == 0) {
            std::cout << "[+] Switchtec Fabric Bind successful. Routing opened." << std::endl;
        } else {
            std::cerr << "[!] Switchtec MRPC returned error! Status: " << (int)result[0] << std::endl;
        }
        switchtec_close(sw_dev);
    } else {
        std::cerr << "[!] Skipping Switchtec (Is driver loaded?)" << std::endl;
    }

    // --- PHASE 2: Load vBIOS ---
    FILE* f = fopen("wx8200.rom", "rb");
    if (!f) {
        std::cerr << "[!] CRITICAL: Could not find wx8200.rom in working directory!" << std::endl;
        return 1;
    }
    std::vector<uint8_t> vbios_buffer(65536, 0);
    size_t bytes_read = fread(vbios_buffer.data(), 1, 65536, f);
    fclose(f);
    std::cout << "[+] WX 8200 vBIOS loaded: " << bytes_read << " / 65536 bytes." << std::endl;

    // --- PHASE 3: Map BARs across all independent dies via SetupAPI ---
    g_mapped_dies = map_all_dies();
    if (g_mapped_dies.empty()) {
        std::cerr << "[!] Failed to map any GPU dies. Ensure v340_mapper.sys is loaded." << std::endl;
        return 1;
    }

    // --- PHASE 4: Enable RLC Scheduler ---
    // RLC_GPU_IOV_VF_ENABLE is a per-die register in the GC/RLC block.
    // Writing 0x3 to a single die tells that die's RLC to partition its 8GB
    // into two 4GB VFs. Writing 0x1 enables VF0 only -- one 8GB VF per die.
    // With two dies: two independent 8GB VFs. Correct topology.
    for (auto& die : g_mapped_dies) {
        write32(die.bars.bar0_user_ptr, REG_RLC_GPU_IOV_SCH_1, 0x186A0); // 4ms world-switch timeslice
        write32(die.bars.bar0_user_ptr, REG_RLC_GPU_IOV_VF_ENABLE, 1);   // One 8GB VF from this die
        std::cout << "[+] RLC Scheduler Armed for Die " << die.die_index
                  << " (1x 8GB VF active)" << std::endl;
    }

    // --- PHASE 5: Mailbox Loop ---
    std::cout << "[*] Entering Mailbox Watchdog Loop..." << std::endl;

    while (true) {
        for (auto& die : g_mapped_dies) {
            int vf_id = 0; // One VF per die, index is always 0

            write32(die.bars.bar0_user_ptr, REG_MAILBOX_INDEX, vf_id);

            // Guard: only process RCV_DW0 if RCV_MSG_VALID is set (bit 8 of CONTROL).
            // Reading DW0 without this check risks acting on stale register state.
            // mxgpu_ai.c documents this explicitly -- peek_msg is only safe when
            // the valid bit is already asserted by hardware.
            uint32_t control = read32(die.bars.bar0_user_ptr, REG_MAILBOX_CONTROL);
            if ((control & 0x100) == 0)
                continue;

            uint32_t request = read32(die.bars.bar0_user_ptr, REG_MSGBUF_RCV_DW0);

            if (request == 1) { // IDH_REQ_GPU_INIT_ACCESS
                std::cout << "[MAILBOX] Die " << die.die_index
                          << " VF0: Received INIT_ACCESS" << std::endl;

                // A. Acknowledge receipt
                write8_safe(die.bars.bar0_user_ptr, REG_MAILBOX_CONTROL, 1, 2);

                // B. Write vBIOS into VF framebuffer window (BAR2 + 0x00000)
                memcpy((uint8_t*)die.bars.bar2_user_ptr + 0x00000,
                       vbios_buffer.data(), 65536);

                // C. Build and write PF2VF capability struct (BAR2 + 0x10000)
                amd_sriov_msg_pf2vf_info pf2vf;
                memset(&pf2vf, 0, sizeof(pf2vf));
                pf2vf.header.size              = 1024;
                pf2vf.header.version           = 2;    // AMD_SRIOV_MSG_FW_VRAM_PF2VF_VER
                pf2vf.vf2pf_update_interval_ms = 2000;
                pf2vf.fcn_idx                  = vf_id;
                pf2vf.checksum                 = 0;
                pf2vf.checksum = compute_checksum(&pf2vf, sizeof(pf2vf), 0);
                memcpy((uint8_t*)die.bars.bar2_user_ptr + 0x10000,
                       &pf2vf, sizeof(pf2vf));

                // D. Send IDH_READY_TO_ACCESS_GPU
                write8_safe(die.bars.bar0_user_ptr, REG_MAILBOX_CONTROL, 0, 0); // Clear TRN_MSG_VALID
                write32(die.bars.bar0_user_ptr, REG_MSGBUF_TRN_DW0, 1);         // IDH_READY_TO_ACCESS_GPU
                write32(die.bars.bar0_user_ptr, REG_MSGBUF_TRN_DW2, 0);         // checksum key = 0 (version 2)
                write8_safe(die.bars.bar0_user_ptr, REG_MAILBOX_CONTROL, 0, 1); // Set TRN_MSG_VALID

                std::cout << "[MAILBOX] Access Granted. GPU Silicon Unlocked." << std::endl;
            }
            else if (request == 6) { // IDH_REQ_GPU_INIT_DATA
                std::cout << "[MAILBOX] Die " << die.die_index
                          << " VF0: Received REQ_GPU_INIT_DATA. Sending READY." << std::endl;

                write8_safe(die.bars.bar0_user_ptr, REG_MAILBOX_CONTROL, 1, 2); // ACK receipt
                write8_safe(die.bars.bar0_user_ptr, REG_MAILBOX_CONTROL, 0, 0); // Clear TRN_MSG_VALID
                write32(die.bars.bar0_user_ptr, REG_MSGBUF_TRN_DW0, 7);         // IDH_REQ_GPU_INIT_DATA_READY
                write32(die.bars.bar0_user_ptr, REG_MSGBUF_TRN_DW2, 0);         // dw2 = 0
                write8_safe(die.bars.bar0_user_ptr, REG_MAILBOX_CONTROL, 0, 1); // Set TRN_MSG_VALID
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0; // Unreachable -- handled by ConsoleHandler
}
