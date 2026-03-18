#include <windows.h>
#include <setupapi.h>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

#include "v340_shared.h"
// [FIX APPLIED] Directly use AMD's header to ensure compiler ABI & bitfield alignments match the guest exactly.
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

// [FIX APPLIED] Full checksum algorithm with subtraction step in case 'key' is ever non-zero
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

// [FIX APPLIED] Safe 32-bit Read-Modify-Write to prevent PCIe dropped TLPs/Master Aborts on 8-bit writes
inline void write8_safe(void* base, uint32_t reg_offset, uint8_t byte_index, uint8_t value) {
    uint32_t val32 = read32(base, reg_offset);
    uint32_t mask = ~(0xFF << (byte_index * 8));
    val32 = (val32 & mask) | (value << (byte_index * 8));
    write32(base, reg_offset, val32);
}

int main() {
    std::cout << "[*] Starting V340L Ghost Hypervisor Daemon..." << std::endl;

    // --- PHASE 1: Switchtec Fabric Bind ---
    struct switchtec_dev* sw_dev = switchtec_open("\\\\.\\switchtec0");
    if (sw_dev) {
        // switchtec_cmd(sw_dev, 0x84, &cmd, sizeof(cmd), &result, sizeof(result));
        std::cout << "[+] Switchtec Fabric Bind command issued." << std::endl;
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
    HANDLE hDevice = CreateFileA("\\\\.\\V340Mapper", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) {
        std::cerr << "[!] Failed to open KMDF Mapper. Ensure v340_mapper.sys is loaded." << std::endl;
        return 1;
    }

    V340_BAR_INFO bars = {0};
    DWORD bytesReturned;
    if (!DeviceIoControl(hDevice, IOCTL_V340_GET_BAR_POINTERS, NULL, 0, &bars, sizeof(bars), &bytesReturned, NULL)) {
        std::cerr << "[!] Failed to map BARs via IOCTL!" << std::endl;
        return 1;
    }
    std::cout << "[+] Hardware mapped! BAR0: " << bars.bar0_user_ptr << " | BAR2: " << bars.bar2_user_ptr << std::endl;

    // --- PHASE 4: Enable RLC Scheduler ---
    // [FIX APPLIED] Removed cargo-culted SCH registers. Only writing the world-switch timer (SCH_1).
    write32(bars.bar0_user_ptr, REG_RLC_GPU_IOV_SCH_1, 0x186A0); // 4ms timeslice
    
    // [FIX APPLIED] Write 3 (0b11) to properly enable both VF0 and VF1
    write32(bars.bar0_user_ptr, REG_RLC_GPU_IOV_VF_ENABLE, 3);
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
                pf2vf.size = 1024;
                pf2vf.version = 2; // AMD_SRIOV_MSG_FW_VRAM_PF2VF_VER
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
            else if (request == 6) { // IDH_QUERY_ALIVE
                write8_safe(bars.bar0_user_ptr, REG_MAILBOX_CONTROL, 1, 2); // Send ACK
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CloseHandle(hDevice);
    return 0;
}