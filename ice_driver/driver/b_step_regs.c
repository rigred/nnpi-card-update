/********************************************
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 ********************************************/


#include "sph_device_regs.h"
#include "b_step/axi2idi_regs_regs.h"
#include "b_step/cve_delphi_cfg_regs.h"
#include "b_step/TLC_command_formats_values_no_ifdef.h"
#include "b_step/cve_cbbid.h"
#include "b_step/CVG_MMU_1_system_map_regs.h"
#include "b_step/map_GeCoE_Core_regs.h"
#include "b_step/cve_dse_regs.h"
#include "b_step/gpsb_x1_regs_regs.h"
#include "b_step/tlc_hi_regs.h"
#include "b_step/ice_mmu_inner_regs.h"
#include "b_step/tlc_regs.h"
#include "b_step/cve_hw_forSw.h"
#include "b_step/ice_obs_dtf_obs_enc_regs_regs.h" /*TODO:enable with coral release */
#include "b_step/mmio_semaphore_regs.h"
#include "b_step/idc_regs_regs.h"
#include "b_step/debugCbbId_regs.h"
#include "b_step/mmio_hub_regs.h"


/*TODO: These macros to be taken from coral once available */
#define ICE_TLC_HI_TLC_DEBUG_REG_MMOFFSET 0x13C
#define ICE_TLC_HI_TLC_GENERATE_CONTROL_UCMD_REG_MMOFFSET 0x138
#define ICE_TLC_LOW_BASE 0x2000

const struct config cfg_b = {
	.ice_gecoe_dec_partial_access_count_offset	= CVE_GECOE_GECOE_DEC_PARTIAL_ACCESS_COUNT_MMOFFSET,
	.ice_gecoe_enc_partial_access_count_offset	= CVE_GECOE_GECOE_ENC_PARTIAL_ACCESS_COUNT_MMOFFSET,
	.ice_gecoe_dec_meta_miss_count_offset		= CVE_GECOE_GECOE_DEC_META_MISS_COUNT_MMOFFSET,
	.ice_gecoe_enc_uncom_mode_count_offset		= CVE_GECOE_GECOE_ENC_UNCOM_MODE_COUNT_MMOFFSET,
	.ice_gecoe_enc_null_mode_count_offset		= CVE_GECOE_GECOE_ENC_NULL_MODE_COUNT_MMOFFSET,
	.ice_gecoe_enc_sm_mode_count_offset		= CVE_GECOE_GECOE_ENC_SM_MODE_COUNT_MMOFFSET,
	.ice_delphi_base				= CVE_DELPHI_BASE,
	.ice_delphi_dbg_perf_cnt_1_reg_offset		= CVE_DELPHI_DELPHI_DBG_PERF_CNT_1_REG_MMOFFSET,
	.ice_delphi_dbg_perf_cnt_2_reg_offset		= CVE_DELPHI_DELPHI_DBG_PERF_CNT_2_REG_MMOFFSET,
	.ice_delphi_dbg_perf_status_reg_offset		= CVE_DELPHI_DELPHI_DBG_PERF_STATUS_REG_MMOFFSET,
	.ice_delphi_gemm_cnn_startup_counter_offset		= CVE_DELPHI_CORE_STATUS_1_MMOFFSET,
	.ice_delphi_gemm_compute_cycle_offset		= CVE_DELPHI_CORE_STATUS_2_MMOFFSET,
	.ice_delphi_gemm_output_write_cycle_offset		= CVE_DELPHI_CORE_STATUS_3_MMOFFSET,
	.ice_delphi_cnn_compute_cycles_offset		= CVE_DELPHI_CORE_STATUS_4_MMOFFSET,
	.ice_delphi_cnn_output_write_cycles_offset		= CVE_DELPHI_CORE_STATUS_5_MMOFFSET,
	.ice_delphi_credit_cfg_latency_offset		= CVE_DELPHI_CORE_STATUS_6_MMOFFSET,
	.ice_delphi_perf_cnt_ovr_flw_indication_offset		= CVE_DELPHI_CORE_STATUS_7_MMOFFSET,
	.ice_mmu_1_system_map_mem_invalidate_offset	= CVG_MMU_1_SYSTEM_MAP_MEM_INVALIDATE_OFFSET,
	.ice_mmu_1_system_map_mem_pt_base_addr_offset	= CVG_MMU_1_SYSTEM_MAP_MEM_PAGE_TABLE_BASE_ADDRESS_OFFSET,
	.ice_mmu_1_system_map_stream_id_l1_0            = ICE_MMU_ATU_STREAM_ID_REG_IDX_L1_0_MMOFFSET,
	.ice_mmu_1_system_map_stream_id_l1_1            = ICE_MMU_ATU_STREAM_ID_REG_IDX_L1_1_MMOFFSET,
	.ice_mmu_1_system_map_stream_id_l1_2            = ICE_MMU_ATU_STREAM_ID_REG_IDX_L1_2_MMOFFSET,
	.ice_mmu_1_system_map_stream_id_l1_3            = ICE_MMU_ATU_STREAM_ID_REG_IDX_L1_3_MMOFFSET,
	.ice_mmu_1_system_map_stream_id_l1_4            = ICE_MMU_ATU_STREAM_ID_REG_IDX_L1_4_MMOFFSET,
	.ice_mmu_1_system_map_stream_id_l1_5            = ICE_MMU_ATU_STREAM_ID_REG_IDX_L1_5_MMOFFSET,
	.ice_mmu_1_system_map_stream_id_l1_6            = ICE_MMU_ATU_STREAM_ID_REG_IDX_L1_6_MMOFFSET,
	.ice_mmu_1_system_map_stream_id_l1_7            = ICE_MMU_ATU_STREAM_ID_REG_IDX_L1_7_MMOFFSET,
	.ice_mmu_1_system_map_stream_id_l2_0            = ICE_MMU_ATU_STREAM_ID_REG_IDX_L2_0_MMOFFSET,
	.ice_mmu_1_system_map_stream_id_l2_1            = ICE_MMU_ATU_STREAM_ID_REG_IDX_L2_1_MMOFFSET,
	.ice_mmu_1_system_map_stream_id_l2_2            = ICE_MMU_ATU_STREAM_ID_REG_IDX_L2_2_MMOFFSET,
	.ice_mmu_1_system_map_stream_id_l2_3            = ICE_MMU_ATU_STREAM_ID_REG_IDX_L2_3_MMOFFSET,
	.ice_mmu_1_system_map_stream_id_l2_4            = ICE_MMU_ATU_STREAM_ID_REG_IDX_L2_4_MMOFFSET,
	.ice_mmu_1_system_map_stream_id_l2_5            = ICE_MMU_ATU_STREAM_ID_REG_IDX_L2_5_MMOFFSET,
	.ice_mmu_1_system_map_stream_id_l2_6            = ICE_MMU_ATU_STREAM_ID_REG_IDX_L2_6_MMOFFSET,
	.ice_mmu_1_system_map_stream_id_l2_7            = ICE_MMU_ATU_STREAM_ID_REG_IDX_L2_7_MMOFFSET,
	.ice_sem_base					= CVE_SEMAPHORE_BASE,
	.ice_sem_mmio_general_offset			= CVE_SEMAPHORE_MMIO_CVE_SEM_GENERAL_MMOFFSET,
	.ice_sem_mmio_demon_enable_offset		= CVE_SEMAPHORE_MMIO_CVE_REGISTER_DEMON_ENABLE_MMOFFSET,
	.ice_sem_mmio_demon_control_offset		= CVE_SEMAPHORE_MMIO_CVE_REGISTER_DEMON_CONTROL_MMOFFSET,
	.ice_sem_mmio_demon_table_offset		= CVE_SEMAPHORE_MMIO_CVE_REGISTER_DEMON_TABLE_MMOFFSET,
	.ice_dso_dtf_encoder_config_reg_offset		= ICE_OBS_ENC_REGS_ICE_OBS_DSO_DTF_ENC_MEM_REGS_DSO_DTF_ENCODER_CONFIG_REG_MMOFFSET,
	.ice_dso_cfg_dtf_src_cfg_reg_offset		= ICE_OBS_ENC_REGS_ICE_OBS_DSO_DTF_OBS_MEM_REGS_DSO_CFG_DTF_SRC_CONFIG_REG_MMOFFSET,
	.ice_dso_cfg_ptype_filter_ch0_reg_offset	= ICE_OBS_ENC_REGS_ICE_OBS_DSO_DTF_OBS_MEM_REGS_DSO_CFG_PTYPE_FILTER_CH0_REG_MMOFFSET,
	.ice_dso_filter_match_low_ch0_reg_offset	= ICE_OBS_ENC_REGS_ICE_OBS_DSO_DTF_OBS_MEM_REGS_DSO_FILTER_MATCH_LOW_CH0_REG_MMOFFSET,
	.ice_dso_filter_match_high_ch0_reg_offset	= ICE_OBS_ENC_REGS_ICE_OBS_DSO_DTF_OBS_MEM_REGS_DSO_FILTER_MATCH_HIGH_CH0_REG_MMOFFSET,
	.ice_dso_filter_mask_low_ch0_reg_offset		= ICE_OBS_ENC_REGS_ICE_OBS_DSO_DTF_OBS_MEM_REGS_DSO_FILTER_MASK_LOW_CH0_REG_MMOFFSET,
	.ice_dso_filter_mask_high_ch0_reg_offset	= ICE_OBS_ENC_REGS_ICE_OBS_DSO_DTF_OBS_MEM_REGS_DSO_FILTER_MASK_HIGH_CH0_REG_MMOFFSET,
	.ice_dso_filter_inv_ch0_reg_offset		= ICE_OBS_ENC_REGS_ICE_OBS_DSO_DTF_OBS_MEM_REGS_DSO_FILTER_INV_CH0_REG_MMOFFSET,
	.ice_dso_cfg_ptype_filter_ch1_reg_offset	= ICE_OBS_ENC_REGS_ICE_OBS_DSO_DTF_OBS_MEM_REGS_DSO_CFG_PTYPE_FILTER_CH1_REG_MMOFFSET,
	.ice_dso_filter_match_low_ch1_reg_offset	= ICE_OBS_ENC_REGS_ICE_OBS_DSO_DTF_OBS_MEM_REGS_DSO_FILTER_MATCH_LOW_CH1_REG_MMOFFSET,
	.ice_dso_filter_match_high_ch1_reg_offset	= ICE_OBS_ENC_REGS_ICE_OBS_DSO_DTF_OBS_MEM_REGS_DSO_FILTER_MATCH_HIGH_CH1_REG_MMOFFSET,
	.ice_dso_filter_mask_low_ch1_reg_offset		= ICE_OBS_ENC_REGS_ICE_OBS_DSO_DTF_OBS_MEM_REGS_DSO_FILTER_MASK_LOW_CH1_REG_MMOFFSET,
	.ice_dso_filter_mask_high_ch1_reg_offset	= ICE_OBS_ENC_REGS_ICE_OBS_DSO_DTF_OBS_MEM_REGS_DSO_FILTER_MASK_HIGH_CH1_REG_MMOFFSET,
	.ice_dso_filter_inv_ch1_reg_offset		= ICE_OBS_ENC_REGS_ICE_OBS_DSO_DTF_OBS_MEM_REGS_DSO_FILTER_INV_CH1_REG_MMOFFSET,
	.ice_dso_cfg_dtf_src_cfg_src_en_mask		= ICE_OBS_DTF_OBS_ENC_REGS_DSO_CFG_DTF_SRC_CONFIG_REG_SRC_EN_MASK,
	.ice_dso_cfg_dtf_src_cfg_ch_en_mask		= ICE_OBS_DTF_OBS_ENC_REGS_DSO_CFG_DTF_SRC_CONFIG_REG_CHANNEL_ENABLE_MASK,
	.ice_dbg_cbbid_base				= CVE_DEBUGCBBID_BASE,
	.ice_dbg_cbbid_cfg_offset	 		= CVE_DEBUGCBBID_DEBUG_CBBID_CFG_REG_MMOFFSET,
	.ice_prog_cores_ctrl_offset			= CVE_MMIO_HUB_PROG_CORES_CONTROL_MMOFFSET,
	.ice_dse_base					= CVE_DSE_BASE,
	.ice_axi_max_inflight_offset			= CVE_DSE_AXI_MAX_INFLIGHT_MMOFFSET,
	.ice_tlc_low_base				= ICE_TLC_LOW_BASE,
	.ice_tlc_hi_base				= CVE_TLC_HI_BASE,
	.ice_tlc_hi_tlc_debug_reg_offset		= ICE_TLC_HI_TLC_DEBUG_REG_MMOFFSET,
	.ice_tlc_hi_tlc_control_ucmd_reg_offset		= ICE_TLC_HI_TLC_GENERATE_CONTROL_UCMD_REG_MMOFFSET,
	.ice_tlc_base					= CVE_TLC_BASE,
	.ice_tlc_hi_dump_control_offset			= CVE_TLC_HI_TLC_DUMP_CONTROL_REG_MMOFFSET,
	.ice_tlc_hi_dump_buf_offset			= CVE_TLC_HI_TLC_DUMP_BUFFER_CONFIG_REG_MMOFFSET,
	.ice_tlc_hi_mailbox_doorbell_offset		= CVE_TLC_HI_TLC_MAILBOX_DOORBELL_MMOFFSET,
	.ice_tlc_barrier_watch_cfg_offset		= CVE_TLC_TLC_BARRIER_WATCH_CONFIG_REG_MMOFFSET,
	.ice_dump_never					= DUMP_CVE_NEVER,
	.ice_dump_now					= DUMP_CVE_NOW,
	.ice_dump_on_error				= DUMP_CVE_ON_ERROR,
	.ice_dump_on_marker				= DUMP_CVE_ON_MARKER,
	.ice_dump_all_marker				= DUMP_CVE_ALL_MARKER,
	.icedc_intr_bit_ilgacc				= IDC_REGS_IDCINTST_ILGACC_LSB,
	.icedc_intr_bit_icererr				= IDC_REGS_IDCINTST_ICERERR_LSB,
	.icedc_intr_bit_icewerr				= IDC_REGS_IDCINTST_ICEWERR_LSB,
	.icedc_intr_bit_asf_ice1_err			= IDC_REGS_IDCINTST_ASF_ICE1_ERR_LSB,
	.icedc_intr_bit_asf_ice0_err			= IDC_REGS_IDCINTST_ASF_ICE0_ERR_LSB,
	.icedc_intr_bit_icecnerr			= IDC_REGS_IDCINTST_ICECNERR_LSB,
	.icedc_intr_bit_iceseerr			= IDC_REGS_IDCINTST_ICESEERR_LSB,
	.icedc_intr_bit_icearerr			= IDC_REGS_IDCINTST_ICEARERR_LSB,
	.icedc_intr_bit_ctrovferr			= IDC_REGS_IDCINTST_CTROVFERR_LSB,
	.icedc_intr_bit_iacntnot			= IDC_REGS_IDCINTST_IACNTNOT_LSB,
	.icedc_intr_bit_semfree				= IDC_REGS_IDCINTST_SEMFREE_LSB,
	.bar0_mem_icerst_offset				= IDC_REGS_IDC_MMIO_BAR0_MEM_ICERST_MMOFFSET,
	.bar0_mem_icerdy_offset				= IDC_REGS_IDC_MMIO_BAR0_MEM_ICERDY_MMOFFSET,
	.bar0_mem_icenote_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_ICENOTE_MMOFFSET,
	.bar0_mem_icepe_offset				= IDC_REGS_IDC_MMIO_BAR0_MEM_ICEPE_MMOFFSET,
	.bar0_mem_iceinten_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_ICEINTEN_MMOFFSET,
	.bar0_mem_iceintst_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_ICEINTST_MMOFFSET,
	.bar0_mem_idcspare_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_IDCSPARE_MMOFFSET,
	.bar0_mem_idcintst_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_IDCINTST_MMOFFSET,
	.bar0_mem_icepool0_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_ICEPOOL0_MMOFFSET,
	.bar0_mem_icepool1_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_ICEPOOL1_MMOFFSET,
	.bar0_mem_icepool2_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_ICEPOOL2_MMOFFSET,
	.bar0_mem_icepool3_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_ICEPOOL3_MMOFFSET,
	.bar0_mem_icepool4_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_ICEPOOL4_MMOFFSET,
	.bar0_mem_icepool5_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_ICEPOOL5_MMOFFSET,
	.bar0_mem_icenota0_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_ICENOTA0_MMOFFSET,
	.bar0_mem_icenota1_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_ICENOTA1_MMOFFSET,
	.bar0_mem_icenota2_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_ICENOTA2_MMOFFSET,
	.bar0_mem_icenota3_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_ICENOTA3_MMOFFSET,
	.bar0_mem_evctprot0_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT0_MMOFFSET,
	.bar0_mem_evctprot1_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT1_MMOFFSET,
	.bar0_mem_evctprot2_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT2_MMOFFSET,
	.bar0_mem_evctprot3_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT3_MMOFFSET,
	.bar0_mem_evctprot4_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT4_MMOFFSET,
	.bar0_mem_evctprot5_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT5_MMOFFSET,
	.bar0_mem_evctprot6_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT6_MMOFFSET,
	.bar0_mem_evctprot7_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT7_MMOFFSET,
	.bar0_mem_evctprot8_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT8_MMOFFSET,
	.bar0_mem_evctprot9_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT9_MMOFFSET,
	.bar0_mem_evctprot10_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT10_MMOFFSET,
	.bar0_mem_evctprot11_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT11_MMOFFSET,
	.bar0_mem_evctprot12_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT12_MMOFFSET,
	.bar0_mem_evctprot13_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT13_MMOFFSET,
	.bar0_mem_evctprot14_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT14_MMOFFSET,
	.bar0_mem_evctprot15_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT15_MMOFFSET,
	.bar0_mem_evctprot16_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT16_MMOFFSET,
	.bar0_mem_evctprot17_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT17_MMOFFSET,
	.bar0_mem_evctprot18_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT18_MMOFFSET,
	.bar0_mem_evctprot19_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT19_MMOFFSET,
	.bar0_mem_evctprot20_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT20_MMOFFSET,
	.bar0_mem_evctprot21_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT21_MMOFFSET,
	.bar0_mem_evctprot22_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT22_MMOFFSET,
	.bar0_mem_evctprot23_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT23_MMOFFSET,
	.bar0_mem_evctprot24_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT24_MMOFFSET,
	.bar0_mem_evctprot25_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT25_MMOFFSET,
	.bar0_mem_evctprot26_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT26_MMOFFSET,
	.bar0_mem_evctprot27_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT27_MMOFFSET,
	.bar0_mem_evctprot28_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT28_MMOFFSET,
	.bar0_mem_evctprot29_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT29_MMOFFSET,
	.bar0_mem_evctprot30_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT30_MMOFFSET,
	.bar0_mem_evctprot31_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_EVCTPROT31_MMOFFSET,
	.bar0_mem_idcinten_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_IDCINTEN_MMOFFSET,
	.bar0_mem_icemasksts_offset			= IDC_REGS_IDC_MMIO_BAR0_MEM_ICEMASKSTS_MMOFFSET,
	.bar0_mem_evctice0_offset			= IDC_REGS_IDC_MMIO_BAR1_MEM_EVCTICE0_MMOFFSET,
	.mmio_cb_doorbell_offset			= CVE_MMIO_HUB_NEW_COMMAND_BUFFER_DOOR_BELL_MMOFFSET,
	.mmio_cbd_base_addr_offset			= CVE_MMIO_HUB_COMMAND_BUFFER_DESCRIPTORS_BASE_ADDRESS_MMOFFSET,
	.mmio_cbd_entries_nr_offset			= CVE_MMIO_HUB_COMMAND_BUFFER_DESCRIPTORS_ENTRIES_NR_MMOFFSET,
	.mmio_intr_mask_offset				= CVE_MMIO_HUB_INTERRUPT_MASK_MMOFFSET,
	.mmio_pre_idle_delay_cnt_offset			= CVE_MMIO_HUB_PRE_IDLE_DELAY_COUNT_MMOFFSET,
	.mmio_cfg_idle_enable_mask			= MMIO_HUB_MEM_CVE_CONFIG_CVE_IDLE_ENABLE_MASK,
	.mmio_intr_status_offset			= CVE_MMIO_HUB_INTERRUPT_STATUS_MMOFFSET,
	.mmio_hw_revision_offset			= INVALID_OFFSET,
	.mmio_wd_intr_mask				= MMIO_HUB_MEM_INTERRUPT_STATUS_INTERNAL_CVE_WATCHDOG_INTERRUPT_MASK,
	.mmio_btrs_wd_intr_mask				= MMIO_HUB_MEM_INTERRUPT_STATUS_BTRS_CVE_WATCHDOG_INTERRUPT_MASK,
	.mmio_sec_wd_intr_mask				= MMIO_HUB_MEM_INTERRUPT_STATUS_INTERNAL_CVE_SECONDARY_WATCHDOG_INTERRUPT_MASK,
	.mmio_cnc_wd_intr_mask				= MMIO_HUB_MEM_INTERRUPT_STATUS_INTERNAL_CVE_CNC_WATCHDOG_INTERRUPT_MASK,
	.mmio_asip2host_intr_mask			= MMIO_HUB_MEM_INTERRUPT_STATUS_ASIP2HOST_INT_MASK,
	.mmio_ivp2host_intr_mask			= MMIO_HUB_MEM_INTERRUPT_STATUS_IVP2HOST_INT_MASK,
	.mmio_dtf_ctrl_offset				= CVE_MMIO_HUB_DTF_CONTROL_MMOFFSET,
	.mmio_ecc_serrcount_offset			= CVE_MMIO_HUB_ECC_SERRCOUNT_MMOFFSET,
	.mmio_ecc_derrcount_offset			= CVE_MMIO_HUB_ECC_DERRCOUNT_MMOFFSET,
	.mmio_parity_low_err_offset                     = CVE_MMIO_HUB_PARITY_LOW_ERR_MMOFFSET,
	.mmio_parity_high_err_offset                    = CVE_MMIO_HUB_PARITY_HIGH_ERR_MMOFFSET,
	.mmio_parity_low_err_mask                       = CVE_MMIO_HUB_PARITY_LOW_ERR_MASK_MMOFFSET,
	.mmio_parity_high_err_mask                      = CVE_MMIO_HUB_PARITY_HIGH_ERR_MASK_MMOFFSET,
	.mmio_parity_errcount_offset			= CVE_MMIO_HUB_PARITY_ERRCOUNT_MMOFFSET,
	.mmio_unmapped_err_id_offset			= CVE_MMIO_HUB_UNMAPPED_ERR_ID_MMOFFSET,
	.mmio_cbb_err_code_offset			= CVE_MMIO_HUB_CBB_ERROR_CODE_MMOFFSET,
	.mmio_cbb_error_info_offset			= CVE_MMIO_HUB_CBB_ERROR_INFO_MMOFFSET,
	.mmio_tlc_info_offset				= CVE_MMIO_HUB_TLC_INFO_MMOFFSET,
	.mmio_gp_regs_offset				= CVE_MMIO_HUB_GENERAL_PURPOSE_REGS_MMOFFSET,
	.mmio_hub_mem_gp_regs_reset			= MMIO_HUB_MEM_GENERAL_PURPOSE_REGS_RESET,
	.mmio_cve_config_offset				= CVE_MMIO_HUB_CVE_CONFIG_MMOFFSET,
	.mmio_wd_enable_mask				= MMIO_HUB_MEM_CVE_CONFIG_CVE_WATCHDOG_ENABLE_MASK,
	.mmio_wd_init_offset				= CVE_MMIO_HUB_CVE_WATCHDOG_INIT_MMOFFSET,
	.mmio_tlc_pulse_offset				= CVE_MMIO_HUB_TLC_WR_PULSE_MMOFFSET,
	.mmio_tlc_wd_petting_mask			= MMIO_HUB_MEM_TLC_WR_PULSE_CVE_WATCHDOG_PETTING_MASK,
	.mmio_dsram_single_err_intr_mask		= MMIO_HUB_MEM_INTERRUPT_STATUS_DSRAM_SINGLE_ERR_INTERRUPT_MASK,
	.mmio_dsram_double_err_intr_mask		= MMIO_HUB_MEM_INTERRUPT_STATUS_DSRAM_DOUBLE_ERR_INTERRUPT_MASK,
	.mmio_sram_parity_err_intr_mask			= MMIO_HUB_MEM_INTERRUPT_MASK_SRAM_PARITY_ERR_INTERRUPT_MASK,
	.mmio_dsram_unmapped_addr_intr_mask		= MMIO_HUB_MEM_INTERRUPT_MASK_DSRAM_UNMAPPED_ADDR_INTERRUPT_MASK,
	.mmio_intr_status_tlc_reserved_mask		= MMIO_HUB_MEM_INTERRUPT_STATUS_TLC_RESERVED_MASK,
	.mmio_intr_status_tlc_panic_mask		= MMIO_HUB_MEM_INTERRUPT_STATUS_TLC_PANIC_MASK,
	.mmio_intr_status_dump_completed_mask		= MMIO_HUB_MEM_INTERRUPT_STATUS_DUMP_COMPLETED_MASK,
	.mmio_intr_status_tlc_cb_completed_mask		= MMIO_HUB_MEM_INTERRUPT_STATUS_TLC_CB_COMPLETED_MASK,
	.mmio_intr_status_tlc_fifo_empty_mask		= MMIO_HUB_MEM_INTERRUPT_STATUS_TLC_FIFO_EMPTY_MASK,
	.mmio_intr_status_tlc_err_mask			= MMIO_HUB_MEM_INTERRUPT_STATUS_TLC_ERROR_MASK,
	.mmio_intr_status_mmu_err_mask			= MMIO_HUB_MEM_INTERRUPT_STATUS_MMU_ERROR_MASK,
	.mmio_intr_status_mmu_page_no_write_perm_mask	= MMIO_HUB_MEM_INTERRUPT_STATUS_MMU_PAGE_NO_WRITE_PERMISSION_MASK,
	.mmio_intr_status_mmu_page_no_read_perm_mask	= MMIO_HUB_MEM_INTERRUPT_STATUS_MMU_PAGE_NO_READ_PERMISSION_MASK,
	.mmio_intr_status_mmu_page_no_exe_perm_mask	= MMIO_HUB_MEM_INTERRUPT_STATUS_MMU_PAGE_NO_EXECUTE_PERMISSION_MASK,
	.mmio_intr_status_mmu_page_none_perm_mask	= MMIO_HUB_MEM_INTERRUPT_STATUS_MMU_PAGE_NONE_PERMISSION_MASK,
	.mmio_intr_status_mmu_soc_bus_err_mask		= MMIO_HUB_MEM_INTERRUPT_STATUS_MMU_SOC_BUS_ERROR_MASK,
	.mmio_intr_status_btrs_wd_intr_mask		= MMIO_HUB_MEM_INTERRUPT_STATUS_BTRS_CVE_WATCHDOG_INTERRUPT_MASK,
	.mmu_atu0_base					= ICE_MMU_ATU0_BASE,
	.mmu_atu1_base					= ICE_MMU_ATU1_BASE,
	.mmu_atu2_base					= ICE_MMU_ATU2_BASE,
	.mmu_atu3_base					= ICE_MMU_ATU3_BASE,
	.mmu_base					= ICE_MMU_BASE,
	.mmu_atu_misses_offset				= ICE_MMU_ATU_MISSES_MMOFFSET,
	.mmu_atu_transactions_offset			= ICE_MMU_ATU_TRANSACTIONS_MMOFFSET,
	.mmu_read_issued_offset				= ICE_MMU_READ_ISSUED_MMOFFSET,
	.mmu_write_issued_offset			= ICE_MMU_WRITE_ISSUED_MMOFFSET,
	.mmu_atu_pt_base_addr_offset			= ICE_MMU_ATU_PAGE_TABLE_BASE_ADDRESS_MMOFFSET,
	.mmu_cfg_offset					= ICE_MMU_MMU_CONFIG_MMOFFSET,
	.mmu_tlc_ivp_stream_mapping_offset		= ICE_MMU_TLC_DSP_STREAM_MAPPING_MMOFFSET,
	.mmu_dse_surf_0_3_stream_mapping_offset		= ICE_MMU_DSE_SURFACE_0_3_STREAM_MAPPING_MMOFFSET,
	.mmu_dse_surf_4_7_stream_mapping_offset		= ICE_MMU_DSE_SURFACE_4_7_STREAM_MAPPING_MMOFFSET,
	.mmu_dse_surf_8_11_stream_mapping_offset	= ICE_MMU_DSE_SURFACE_8_11_STREAM_MAPPING_MMOFFSET,
	.mmu_dse_surf_12_15_stream_mapping_offset	= ICE_MMU_DSE_SURFACE_12_15_STREAM_MAPPING_MMOFFSET,
	.mmu_dse_surf_16_19_stream_mapping_offset	= ICE_MMU_DSE_SURFACE_16_19_STREAM_MAPPING_MMOFFSET,
	.mmu_dse_surf_20_23_stream_mapping_offset	= ICE_MMU_DSE_SURFACE_20_23_STREAM_MAPPING_MMOFFSET,
	.mmu_dse_surf_24_27_stream_mapping_offset	= ICE_MMU_DSE_SURFACE_24_27_STREAM_MAPPING_MMOFFSET,
	.mmu_dse_surf_28_31_stream_mapping_offset	= ICE_MMU_DSE_SURFACE_28_31_STREAM_MAPPING_MMOFFSET,
	.mmu_delphi_stream_mapping_offset		= ICE_MMU_DELPHI_STREAM_MAPPING_MMOFFSET,
	.mmu_pt_idx_bits_table_bit0_lsb			= ICE_MMU_INNER_MEM_AXI_TABLE_PT_INDEX_BITS_TABLE_INDEX_BIT0_LSB,
	.mmu_pt_idx_bits_table_bit1_lsb			= ICE_MMU_INNER_MEM_AXI_TABLE_PT_INDEX_BITS_TABLE_INDEX_BIT1_LSB,
	.mmu_pt_idx_bits_table_bit2_lsb			= ICE_MMU_INNER_MEM_AXI_TABLE_PT_INDEX_BITS_TABLE_INDEX_BIT2_LSB,
	.mmu_pt_idx_bits_table_bit3_lsb			= ICE_MMU_INNER_MEM_AXI_TABLE_PT_INDEX_BITS_TABLE_INDEX_BIT3_LSB,
	.mmu_axi_tbl_pt_idx_bits_offset			= ICE_MMU_AXI_TABLE_PT_INDEX_BITS_MMOFFSET,
	.mmu_tlc_axi_attri_offset			= ICE_MMU_TLC_AXI_ATTRIBUTES_MMOFFSET,
	.mmu_asip_axi_attri_offset			= ICE_MMU_ASIP_AXI_ATTRIBUTES_MMOFFSET,
	.mmu_dsp_axi_attri_offset			= ICE_MMU_DSP_AXI_ATTRIBUTES_MMOFFSET,
	.mmu_page_walk_axi_attri_offset			= ICE_MMU_PAGE_WALK_AXI_ATTRIBUTES_MMOFFSET,
	.mmu_fault_details_offset			= ICE_MMU_MMU_FAULT_DETAILS_MMOFFSET,
	.mmu_fault_linear_addr_offset			= ICE_MMU_FAULT_LINEAR_ADDRESS_MMOFFSET,
	.mmu_fault_physical_addr_offset			= ICE_MMU_FAULT_PHYSICAL_ADDRESS_MMOFFSET,
	.mmu_chicken_bits_offset			= ICE_MMU_MMU_CHICKEN_BITS_MMOFFSET,
	.mmu_page_sizes_offset				= ICE_MMU_PAGE_SIZES_MMOFFSET,
	.gpsb_x1_regs_clk_gate_ctl_offset		= GPSB_X1_REGS_CLK_GATE_CTL_MMOFFSET,
	.gpsb_x1_regs_iccp_config2_offset		= GPSB_X1_REGS_ICCP_CONFIG2_MMOFFSET,
	.gpsb_x1_regs_iccp_config3_offset		= GPSB_X1_REGS_ICCP_CONFIG3_MMOFFSET,
	.mem_clk_gate_ctl_dont_squash_iceclk_lsb	= MEM_CLK_GATE_CTL_DONT_SQUASH_ICECLK_LSB,
	.cbbid_tlc_offset				= CBBID_TLC_OFFSET,
	.axi_shared_read_status_offset			= AXI_SHARED_READ_STATUS_OFFSET,
	.tlc_dram_size					= TLC_DRAM_SIZE,
	.computecluster_sp_size_in_kb			= COMPUTECLUSTER_SP_SIZE_IN_KB,
	.tlc_trax_mem_size				= TLC_TRAX_MEM_SIZE,
	.trax_header_size_in_bytes			= TRAX_HEADER_SIZE_IN_BYTES,
	.cnc_cr_number_of_bids				= CNC_CR_NUMBER_OF_BIDS,
	.cnc_cr_num_of_regs_per_bid			= CNC_CR_NUM_OF_REGS_PER_BID,
	.cnc_cr_num_of_regs				= CNC_CR_NUM_OF_REGS,
	.credit_acc_reg_size				= CREDIT_ACC_REG_SIZE,
	.stop_all_barriers				= STOP_ALL_BARRIERS,
	.stop_on_section_id				= STOP_ON_SECTION_ID,
	.resume						= RESUME,
	.block_incoming_cnc_messages			= BLOCK_INCOMING_CNC_MESSAGES,
	.serve_incoming_cnc_messages			= SERVE_INCOMING_CNC_MESSAGES,
	.axi_shared_read_cfg_offset			= AXI_SHARED_READ_CFG_OFFSET,
        .cve_status_empty				= CVE_STATUS_EMPTY,
        .cve_status_pending				= CVE_STATUS_PENDING,
        .cve_status_dispatched				= CVE_STATUS_DISPATCHED,
        .cve_status_running				= CVE_STATUS_RUNNING,
        .cve_status_completed				= CVE_STATUS_COMPLETED,
        .cve_status_aborted				= CVE_STATUS_ABORTED,
        .cve_status_loaded				= CVE_STATUS_LOADED,
	.bar1_msg_evctice0_msgregaddr			= IDC_REGS_IDC_MMIO_BAR1_MSG_EVCTICE0_MSGREGADDR,
	.bar1_msg_evctincice0_msgregaddr		= IDC_REGS_IDC_MMIO_BAR1_MSG_EVCTINCICE0_MSGREGADDR,
	.mmio_hw_revision_major_rev_mask		= INVALID_OFFSET,
	.mmio_hw_revision_minor_rev_mask		= INVALID_OFFSET,
	.cbbid_gecoe_offset				= CBBID_GECOE_OFFSET,
	.mmio_dpcg_control				= CVE_MMIO_HUB_CVE_DPCG_CONTROL_REG_MMOFFSET,
	.a2i_icebo_pmon_global_offset			= MEM_A2I_ICEBAR_ICEBO_PMON_GLOBAL_MMOFFSET,
	.a2i_icebo_pmon_event_0_offset			= MEM_A2I_ICEBAR_ICEBO_PMON_EVENT_0_MMOFFSET,
	.a2i_icebo_pmon_event_1_offset			= MEM_A2I_ICEBAR_ICEBO_PMON_EVENT_1_MMOFFSET,
	.a2i_icebo_pmon_event_2_offset			= MEM_A2I_ICEBAR_ICEBO_PMON_EVENT_2_MMOFFSET,
	.a2i_icebo_pmon_event_3_offset			= MEM_A2I_ICEBAR_ICEBO_PMON_EVENT_3_MMOFFSET,
	.a2i_icebo_pmon_counter_0_offset		= MEM_A2I_ICEBAR_ICEBO_PMON_COUNTER_0_MMOFFSET,
	.a2i_icebo_pmon_counter_1_offset		= MEM_A2I_ICEBAR_ICEBO_PMON_COUNTER_1_MMOFFSET,
	.a2i_icebo_pmon_counter_2_offset		= MEM_A2I_ICEBAR_ICEBO_PMON_COUNTER_2_MMOFFSET,
	.a2i_icebo_pmon_counter_3_offset		= MEM_A2I_ICEBAR_ICEBO_PMON_COUNTER_3_MMOFFSET,
	.a2i_icebo_pmon_status_offset			= MEM_A2I_ICEBAR_ICEBO_PMON_STATUS_MMOFFSET,
	.ice_delphi_dbg_perf_status_total_cyc_cnt_saturated_mask = CVE_DELPHI_CFG_MEM_DELPHI_DBG_PERF_STATUS_REG_TOTAL_CYC_CNT_SATURATED_MASK,
	.ice_delphi_bdg_perf_status_per_lyr_cyc_cnt_saturated_mask = CVE_DELPHI_CFG_MEM_DELPHI_DBG_PERF_STATUS_REG_TOTAL_CYC_CNT_SATURATED_MASK,
	.mmio_hub_p_wait_mode_offset                    = CVE_MMIO_HUB_P_WAIT_MODE_MMOFFSET,
	.mmio_prog_cores_control_tlc_runstall_mask = MMIO_HUB_MEM_PROG_CORES_CONTROL_TLC_RUNSTALL_MASK,
	.mmio_prog_cores_control_ivp_runstall_mask = MMIO_HUB_MEM_PROG_CORES_CONTROL_IVP_RUNSTALL_MASK,
	.mmio_prog_cores_control_asip0_runstall_mask = MMIO_HUB_MEM_PROG_CORES_CONTROL_ASIP0_RUNSTALL_MASK,
	.mmio_prog_cores_control_asip1_runstall_mask = MMIO_HUB_MEM_PROG_CORES_CONTROL_ASIP1_RUNSTALL_MASK,
	.gecoe_base = CVE_GECOE_BASE,
	.gecoe_dbg_reg_offset = CVE_GECOE_GECOE_DBG_REG_MMOFFSET,
};
