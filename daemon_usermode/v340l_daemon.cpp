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

// Global handle for graceful exit
HANDLE g_hDevice = INVALID_HANDLE_VALUE;

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
BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        std::cout << "\n[!] Shutting down daemon safely... Unmapping BARs." << std::endl;
        if (g_hDevice != INVALID_HANDLE_VALUE) {
            CloseHandle(g_hDevice);
            g_hDevice = INVALID_HANDLE_VALUE;
        }
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

inline void write8_safe(void* base, uint32_t reg_offset, uint8_t byte_index, uint8_t value) {
    uint32_t val32 = read32(base, reg_offset);
    uint32_t mask = ~(0xFF << (byte_index * 8));
    val32 = (val32 & mask) | (value << (byte_index * 8));
    write32(base, reg_offset, val32);
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
    struct switchtec_dev* sw_dev = switchtec_open("\\\\.\\switchtec0");
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

        uint8_t result[4] = {0}; // Status response
        int ret = switchtec_cmd(sw_dev, 0x84 /* MRPC_GFMS_BIND_UNBIND */, &cmd, sizeof(cmd), &result, sizeof(result));

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
    fread(vbios_buffer.data(), 1, 65536, f);
    fclose(f);
    std::cout << "[+] WX 8200 vBIOS loaded into memory (64KB)." << std::endl;

    // --- PHASE 3: Map BARs ---
    g_hDevice = CreateFileA("\\\\.\\V340Mapper", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (g_hDevice == INVALID_HANDLE_VALUE) {
        std::cerr << "[!] Failed to open KMDF Mapper. Ensure v340_mapper.sys is loaded." << std::endl;
        return 1;
    }

    V340_BAR_INFO bars = {0};
    DWORD bytesReturned;
    if (!DeviceIoControl(g_hDevice, IOCTL_V340_GET_BAR_POINTERS, NULL, 0, &bars, sizeof(bars), &bytesReturned, NULL)) {
        std::cerr << "[!] Failed to map BARs via IOCTL!" << std::endl;
        return 1;
    }
    std::cout << "[+] Hardware mapped! BAR0: " << bars.bar0_user_ptr << " | FB BAR: " << bars.bar2_user_ptr << std::endl;

    // --- PHASE 4: Enable RLC Scheduler ---
    write32(bars.bar0_user_ptr, REG_RLC_GPU_IOV_SCH_1, 0x186A0); // 4ms timeslice
    write32(bars.bar0_user_ptr, REG_RLC_GPU_IOV_VF_ENABLE, 3);   // Enable VF0 and VF1
    std::cout << "[+] RunList Controller Scheduler Armed for VFs 0 and 1." << std::endl;

    // --- PHASE 5: Mailbox Loop ---
    std::cout << "[*] Entering Mailbox Watchdog Loop..." << std::endl;

    while (true) {
        for (int vf_id = 0; vf_id < 2; vf_id++) { 
            
            write32(bars.bar0_user_ptr, REG_MAILBOX_INDEX, vf_id);
            uint32_t request = read32(bars.bar0_user_ptr, REG_MSGBUF_RCV_DW0);

            if (request == 1) { // IDH_REQ_GPU_INIT_ACCESS
                std::cout << "[MAILBOX] Received INIT_ACCESS from VF " << vf_id << std::endl;

                // A. Acknowledge Receipt (Write 2 to RCV byte via safe RMW)
                write8_safe(bars.bar0_user_ptr, REG_MAILBOX_CONTROL, 1, 2); 

                // B. Write vBIOS (BAR2 + 0x00000)
                memcpy((uint8_t*)bars.bar2_user_ptr + 0x00000, vbios_buffer.data(), 65536);
                
                // C. Write PF2VF Capability Struct (BAR2 + 0x10000)
                amd_sriov_msg_pf2vf_info pf2vf;
                memset(&pf2vf, 0, sizeof(pf2vf));
                pf2vf.header.size = 1024;
                pf2vf.header.version = 2; // AMD_SRIOV_MSG_FW_VRAM_PF2VF_VER
                pf2vf.vf2pf_update_interval_ms = 2000;
                pf2vf.fcn_idx = vf_id;
                pf2vf.checksum = 0; 
                pf2vf.checksum = compute_checksum(&pf2vf, sizeof(pf2vf), 0);
                memcpy((uint8_t*)bars.bar2_user_ptr + 0x10000, &pf2vf, sizeof(pf2vf));
                
                // D. Send READY_TO_ACCESS
                write8_safe(bars.bar0_user_ptr, REG_MAILBOX_CONTROL, 0, 0); // Clear valid bit
                write32(bars.bar0_user_ptr, REG_MSGBUF_TRN_DW0, 1);         // READY_TO_ACCESS_GPU
                write32(bars.bar0_user_ptr, REG_MSGBUF_TRN_DW2, 0);         // Checksum key = 0
                write8_safe(bars.bar0_user_ptr, REG_MAILBOX_CONTROL, 0, 1); // TRN_MSG_VALID = 1
                
                std::cout << "[MAILBOX] Access Granted. GPU Silicon Unlocked." << std::endl;
            }
            else if (request == 6) { // IDH_REQ_GPU_INIT_DATA
                std::cout << "[MAILBOX] Received REQ_GPU_INIT_DATA from VF " << vf_id << ". Sending READY." << std::endl;

                write8_safe(bars.bar0_user_ptr, REG_MAILBOX_CONTROL, 1, 2); // ACK receipt
                write8_safe(bars.bar0_user_ptr, REG_MAILBOX_CONTROL, 0, 0); // clear TRN valid
                write32(bars.bar0_user_ptr, REG_MSGBUF_TRN_DW0, 7);         // IDH_REQ_GPU_INIT_DATA_READY
                write32(bars.bar0_user_ptr, REG_MSGBUF_TRN_DW2, 0);         // dw2 = 0
                write8_safe(bars.bar0_user_ptr, REG_MAILBOX_CONTROL, 0, 1); // set TRN_MSG_VALID
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Should never reach here due to Ctrl+C handler, but just in case
    CloseHandle(g_hDevice);
    return 0;
}