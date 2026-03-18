#pragma once
/*
 * amdgv_sriovmsg.h — Windows/MSVC port
 *
 * Ported from drivers/gpu/drm/amd/amdgpu/amdgv_sriovmsg.h (torvalds/linux).
 * Changes from upstream:
 *   - __packed removed (redundant with #pragma pack(push,1) below; MSVC does not support it)
 *   - _Static_assert / #ifdef __linux__ block replaced with C++11 static_assert
 *   - vf2pf struct and V1 layout omitted (daemon only writes the V2 pf2vf struct)
 */

#include <stdint.h>

#define AMD_SRIOV_MSG_SIZE_KB               1
#define AMD_SRIOV_MSG_FW_VRAM_PF2VF_VER     2
#define AMD_SRIOV_MSG_FW_VRAM_VF2PF_VER     3
#define AMD_SRIOV_MSG_RESERVE_UCODE         24
#define AMD_SRIOV_MSG_RESERVE_VCN_INST      4

#pragma pack(push, 1)

union amd_sriov_msg_feature_flags {
    struct {
        uint32_t error_log_collect    : 1;
        uint32_t host_load_ucodes     : 1;
        uint32_t host_flr_vramlost    : 1;
        uint32_t mm_bw_management     : 1;
        uint32_t pp_one_vf_mode       : 1;
        uint32_t reg_indirect_acc     : 1;
        uint32_t av1_support          : 1;
        uint32_t vcn_rb_decouple      : 1;
        uint32_t mes_info_dump_enable : 1;
        uint32_t ras_caps             : 1;
        uint32_t ras_telemetry        : 1;
        uint32_t ras_cper             : 1;
        uint32_t xgmi_ta_ext_peer_link: 1;
        uint32_t reserved             : 19;
    } flags;
    uint32_t all;
};

union amd_sriov_reg_access_flags {
    struct {
        uint32_t vf_reg_access_ih          : 1;
        uint32_t vf_reg_access_mmhub       : 1;
        uint32_t vf_reg_access_gc          : 1;
        uint32_t vf_reg_access_l1_tlb_cntl : 1;
        uint32_t vf_reg_access_sq_config   : 1;
        uint32_t reserved                  : 27;
    } flags;
    uint32_t all;
};

union amd_sriov_ras_caps {
    struct {
        uint64_t block_umc               : 1;
        uint64_t block_sdma              : 1;
        uint64_t block_gfx               : 1;
        uint64_t block_mmhub             : 1;
        uint64_t block_athub             : 1;
        uint64_t block_pcie_bif          : 1;
        uint64_t block_hdp               : 1;
        uint64_t block_xgmi_wafl         : 1;
        uint64_t block_df                : 1;
        uint64_t block_smn               : 1;
        uint64_t block_sem               : 1;
        uint64_t block_mp0               : 1;
        uint64_t block_mp1               : 1;
        uint64_t block_fuse              : 1;
        uint64_t block_mca               : 1;
        uint64_t block_vcn               : 1;
        uint64_t block_jpeg              : 1;
        uint64_t block_ih                : 1;
        uint64_t block_mpio              : 1;
        uint64_t block_mmsch             : 1;
        uint64_t poison_propogation_mode : 1;
        uint64_t uniras_supported        : 1;
        uint64_t reserved                : 42;
    } bits;
    uint64_t all;
};

struct amd_sriov_msg_uuid_info {
    union {
        struct {
            uint32_t did    : 16;
            uint32_t fcn    : 8;
            uint32_t asic_7 : 8;
        };
        uint32_t time_low;
    };
    struct {
        uint32_t time_mid  : 16;
        uint32_t time_high : 12;
        uint32_t version   : 4;
    };
    struct {
        struct {
            uint8_t clk_seq_hi : 6;
            uint8_t variant    : 2;
        };
        union {
            uint8_t clk_seq_low;
            uint8_t asic_6;
        };
        uint16_t asic_4;
    };
    uint32_t asic_0;
};

struct amd_sriov_msg_pf2vf_info_header {
    uint32_t size;
    uint32_t version;
    uint32_t reserved[2];
};

#define AMD_SRIOV_MSG_PF2VF_INFO_FILLED_SIZE (55)

struct amd_sriov_msg_pf2vf_info {
    struct amd_sriov_msg_pf2vf_info_header header;
    uint32_t checksum;
    union amd_sriov_msg_feature_flags feature_flags;
    uint32_t hevc_enc_max_mb_per_second;
    uint32_t hevc_enc_max_mb_per_frame;
    uint32_t avc_enc_max_mb_per_second;
    uint32_t avc_enc_max_mb_per_frame;
    uint64_t mecfw_offset;
    uint32_t mecfw_size;
    uint64_t uvdfw_offset;
    uint32_t uvdfw_size;
    uint64_t vcefw_offset;
    uint32_t vcefw_size;
    uint32_t bp_block_offset_low;
    uint32_t bp_block_offset_high;
    uint32_t bp_block_size;
    uint32_t vf2pf_update_interval_ms;
    uint64_t uuid;
    uint32_t fcn_idx;
    union amd_sriov_reg_access_flags reg_access_flags;
    struct {
        uint32_t decode_max_dimension_pixels;
        uint32_t decode_max_frame_pixels;
        uint32_t encode_max_dimension_pixels;
        uint32_t encode_max_frame_pixels;
    } mm_bw_management[AMD_SRIOV_MSG_RESERVE_VCN_INST];
    struct amd_sriov_msg_uuid_info uuid_info;
    uint32_t pcie_atomic_ops_support_flags;
    uint32_t gpu_capacity;
    uint32_t bdf_on_host;
    uint32_t more_bp;
    union amd_sriov_ras_caps ras_en_caps;
    union amd_sriov_ras_caps ras_telemetry_en_caps;
    uint32_t reserved[256 - AMD_SRIOV_MSG_PF2VF_INFO_FILLED_SIZE];
};

#pragma pack(pop)

static_assert(sizeof(amd_sriov_msg_pf2vf_info) == 1024,
    "amd_sriov_msg_pf2vf_info must be exactly 1024 bytes — check struct layout against upstream");
static_assert(AMD_SRIOV_MSG_PF2VF_INFO_FILLED_SIZE == 55,
    "Filled size mismatch");
