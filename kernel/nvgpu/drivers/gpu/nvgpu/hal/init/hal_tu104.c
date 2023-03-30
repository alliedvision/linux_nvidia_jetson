/*
 * TU104 Tegra HAL interface
 *
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <nvgpu/preempt.h>
#include <nvgpu/errata.h>

#include "hal/mm/mm_gm20b.h"
#include "hal/mm/mm_gp10b.h"
#include "hal/mm/mm_gv11b.h"
#include "hal/mm/mm_tu104.h"
#include "hal/mm/cache/flush_gk20a.h"
#include "hal/mm/cache/flush_gv11b.h"
#include "hal/mm/gmmu/gmmu_gm20b.h"
#include "hal/mm/gmmu/gmmu_gp10b.h"
#include "hal/mm/gmmu/gmmu_gv11b.h"
#include "hal/mm/mmu_fault/mmu_fault_gv11b.h"
#include "hal/mc/mc_gm20b.h"
#include "hal/mc/mc_gp10b.h"
#include "hal/mc/mc_gv11b.h"
#include "hal/mc/mc_gv100.h"
#include "hal/mc/mc_tu104.h"
#include "hal/bus/bus_gk20a.h"
#include "hal/bus/bus_gv100.h"
#include "hal/bus/bus_gv11b.h"
#include "hal/bus/bus_tu104.h"
#include "hal/ce/ce_gp10b.h"
#include "hal/ce/ce_gv11b.h"
#include "hal/ce/ce_tu104.h"
#include "hal/class/class_tu104.h"
#include "hal/priv_ring/priv_ring_gm20b.h"
#include "hal/priv_ring/priv_ring_gp10b.h"
#include "hal/priv_ring/priv_ring_gv11b.h"
#include "hal/power_features/cg/tu104_gating_reglist.h"
#include "hal/cbc/cbc_gv11b.h"
#include "hal/cbc/cbc_tu104.h"
#include "hal/therm/therm_gm20b.h"
#include "hal/therm/therm_tu104.h"
#include "hal/therm/therm_gv11b.h"
#include "hal/ltc/ltc_gm20b.h"
#include "hal/ltc/ltc_gp10b.h"
#include "hal/ltc/ltc_gv11b.h"
#include "hal/ltc/ltc_tu104.h"
#include "hal/ltc/intr/ltc_intr_gv11b.h"
#include "hal/fb/fb_gm20b.h"
#include "hal/fb/fb_gp10b.h"
#include "hal/fb/fb_gp106.h"
#include "hal/fb/fb_gv11b.h"
#include "hal/fb/fb_gv100.h"
#include "hal/fb/fb_tu104.h"
#include "hal/fb/fb_mmu_fault_gv11b.h"
#include "hal/fb/fb_mmu_fault_tu104.h"
#include "hal/fb/ecc/fb_ecc_gv11b.h"
#include "hal/fb/intr/fb_intr_ecc_gv11b.h"
#include "hal/fb/intr/fb_intr_tu104.h"
#include "hal/ptimer/ptimer_gk20a.h"
#include "hal/ptimer/ptimer_gp10b.h"
#include "hal/ptimer/ptimer_gv11b.h"
#include "hal/regops/regops_tu104.h"
#include "hal/regops/allowlist_tu104.h"
#include "hal/func/func_tu104.h"
#include "hal/fuse/fuse_gm20b.h"
#include "hal/fuse/fuse_gp10b.h"
#include "hal/fuse/fuse_gp106.h"
#include "hal/fuse/fuse_gv11b.h"
#include "hal/fuse/fuse_tu104.h"
#ifdef CONFIG_NVGPU_RECOVERY
#include "hal/rc/rc_gv11b.h"
#endif
#include "hal/fifo/fifo_gk20a.h"
#include "hal/fifo/fifo_gv11b.h"
#include "hal/fifo/fifo_tu104.h"
#include "hal/fifo/preempt_gv11b.h"
#include "hal/fifo/usermode_gv11b.h"
#include "hal/fifo/usermode_tu104.h"
#include "hal/fifo/pbdma_gm20b.h"
#include "hal/fifo/pbdma_gp10b.h"
#include "hal/fifo/pbdma_gv11b.h"
#include "hal/fifo/pbdma_tu104.h"
#include "hal/fifo/engines_gp10b.h"
#include "hal/fifo/engines_gv11b.h"
#include "hal/fifo/ramfc_gp10b.h"
#include "hal/fifo/ramfc_gv11b.h"
#include "hal/fifo/ramfc_tu104.h"
#include "hal/fifo/ramin_gk20a.h"
#include "hal/fifo/ramin_gm20b.h"
#include "hal/fifo/ramin_gv11b.h"
#include "hal/fifo/ramin_tu104.h"
#include "hal/fifo/runlist_ram_gk20a.h"
#include "hal/fifo/runlist_ram_gv11b.h"
#include "hal/fifo/runlist_ram_tu104.h"
#include "hal/fifo/runlist_fifo_gk20a.h"
#include "hal/fifo/runlist_fifo_gv11b.h"
#include "hal/fifo/runlist_fifo_tu104.h"
#include "hal/fifo/tsg_gk20a.h"
#include "hal/fifo/tsg_gv11b.h"
#include "hal/fifo/userd_gk20a.h"
#include "hal/fifo/userd_gv11b.h"
#include "hal/fifo/fifo_intr_gk20a.h"
#include "hal/fifo/fifo_intr_gv100.h"
#include "hal/fifo/fifo_intr_gv11b.h"
#include "hal/fifo/engine_status_gv100.h"
#include "hal/fifo/pbdma_status_gm20b.h"
#include "hal/fifo/ctxsw_timeout_gv11b.h"
#include "hal/gr/ecc/ecc_gv11b.h"
#ifdef CONFIG_NVGPU_FECS_TRACE
#include "hal/gr/fecs_trace/fecs_trace_gm20b.h"
#include "hal/gr/fecs_trace/fecs_trace_gv11b.h"
#endif
#include "hal/gr/falcon/gr_falcon_gm20b.h"
#include "hal/gr/falcon/gr_falcon_gp10b.h"
#include "hal/gr/falcon/gr_falcon_gv11b.h"
#include "hal/gr/falcon/gr_falcon_tu104.h"
#include "hal/gr/config/gr_config_gm20b.h"
#include "hal/gr/config/gr_config_gv11b.h"
#include "hal/gr/config/gr_config_gv100.h"
#ifdef CONFIG_NVGPU_GRAPHICS
#include "hal/gr/zbc/zbc_gm20b.h"
#include "hal/gr/zbc/zbc_gp10b.h"
#include "hal/gr/zbc/zbc_gv11b.h"
#include "hal/gr/zcull/zcull_gm20b.h"
#include "hal/gr/zcull/zcull_gv11b.h"
#endif
#include "hal/gr/init/gr_init_gm20b.h"
#include "hal/gr/init/gr_init_gp10b.h"
#include "hal/gr/init/gr_init_gv11b.h"
#include "hal/gr/init/gr_init_tu104.h"
#include "hal/gr/intr/gr_intr_gm20b.h"
#include "hal/gr/intr/gr_intr_gp10b.h"
#include "hal/gr/intr/gr_intr_gv11b.h"
#include "hal/gr/intr/gr_intr_tu104.h"
#ifdef CONFIG_NVGPU_DEBUGGER
#include "hal/gr/hwpm_map/hwpm_map_gv100.h"
#endif
#include "hal/gr/ctxsw_prog/ctxsw_prog_gm20b.h"
#include "hal/gr/ctxsw_prog/ctxsw_prog_gp10b.h"
#include "hal/gr/ctxsw_prog/ctxsw_prog_gv11b.h"
#ifdef CONFIG_NVGPU_DEBUGGER
#include "hal/gr/gr/gr_gk20a.h"
#include "hal/gr/gr/gr_gm20b.h"
#include "hal/gr/gr/gr_gp10b.h"
#include "hal/gr/gr/gr_gv11b.h"
#include "hal/gr/gr/gr_gv100.h"
#include "hal/gr/gr/gr_tu104.h"
#endif
#include "hal/pmu/pmu_gk20a.h"
#ifdef CONFIG_NVGPU_LS_PMU
#include "hal/pmu/pmu_gm20b.h"
#endif
#include "hal/pmu/pmu_gp10b.h"
#include "hal/pmu/pmu_gv11b.h"
#include "hal/pmu/pmu_tu104.h"
#include "hal/falcon/falcon_gk20a.h"
#include "hal/nvdec/nvdec_tu104.h"
#include "hal/gsp/gsp_tu104.h"
#ifdef CONFIG_NVGPU_DEBUGGER
#include "hal/perf/perf_gv11b.h"
#include "hal/perf/perf_tu104.h"
#endif
#ifdef CONFIG_NVGPU_DGPU
#include "hal/sec2/sec2_tu104.h"
#endif
#include "hal/sync/syncpt_cmdbuf_gv11b.h"
#include "hal/sync/sema_cmdbuf_gv11b.h"
#include "hal/netlist/netlist_tu104.h"
#include "hal/top/top_gm20b.h"
#include "hal/top/top_gp10b.h"
#include "hal/top/top_gv100.h"
#include "hal/top/top_gv11b.h"
#include "hal/bios/bios_tu104.h"
#include "hal/pramin/pramin_init.h"
#include "hal/xve/xve_gp106.h"
#include "hal/xve/xve_tu104.h"

#include "common/nvlink/init/device_reginit_gv100.h"
#include "hal/nvlink/intr_and_err_handling_tu104.h"
#include "hal/nvlink/minion_gv100.h"
#include "hal/nvlink/minion_tu104.h"
#include "hal/nvlink/link_mode_transitions_gv100.h"
#include "hal/nvlink/link_mode_transitions_tu104.h"
#include "hal/nvlink/nvlink_gv100.h"
#include "hal/nvlink/nvlink_tu104.h"
#include "hal/fifo/channel_gk20a.h"
#include "hal/fifo/channel_gm20b.h"
#include "hal/fifo/channel_gv11b.h"
#include "hal/fifo/channel_gv100.h"
#ifdef CONFIG_NVGPU_CLK_ARB
#include "common/clk_arb/clk_arb_gv100.h"
#endif

#include "hal/clk/clk_tu104.h"
#include "hal/clk/clk_mon_tu104.h"
#include "hal/fbpa/fbpa_tu104.h"
#include "hal_tu104.h"
#include "hal_tu104_litter.h"

#include <nvgpu/ptimer.h>
#include <nvgpu/error_notifier.h>
#include <nvgpu/acr.h>
#include <nvgpu/ce.h>
#include <nvgpu/ce_app.h>
#include <nvgpu/pmu.h>
#ifdef CONFIG_NVGPU_LS_PMU
#include <nvgpu/pmu/pmu_pstate.h>
#endif
#include <nvgpu/therm.h>
#include <nvgpu/clk_arb.h>
#include <nvgpu/class.h>
#include <nvgpu/debugger.h>
#include <nvgpu/pm_reservation.h>
#include <nvgpu/pbdma.h>
#include <nvgpu/runlist.h>
#include <nvgpu/fifo/userd.h>
#include <nvgpu/perfbuf.h>
#include <nvgpu/cyclestats_snapshot.h>
#include <nvgpu/regops.h>
#ifdef CONFIG_NVGPU_GRAPHICS
#include <nvgpu/gr/zbc.h>
#endif
#include <nvgpu/gr/setup.h>
#include <nvgpu/gr/fecs_trace.h>
#include <nvgpu/pmu/clk/clk.h>
#include <nvgpu/pmu/perf.h>
#include <nvgpu/gr/gr_falcon.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/gr_intr.h>
#include <nvgpu/pmu/pmu_perfmon.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/sbr.h>
#include <nvgpu/nvhost.h>
#include <nvgpu/nvlink.h>
#include <nvgpu/clk_mon.h>
#include <nvgpu/grmgr.h>
#include <nvgpu/profiler.h>

#include <nvgpu/hw/tu104/hw_pwr_tu104.h>

static int tu104_init_gpu_characteristics(struct gk20a *g)
{
	int err;

	err = nvgpu_init_gpu_characteristics(g);
	if (err != 0) {
		nvgpu_err(g, "failed to init GPU characteristics");
		return err;
	}

	nvgpu_set_enabled(g, NVGPU_SUPPORT_TSG_SUBCONTEXTS, true);
#ifdef CONFIG_NVGPU_GRAPHICS
	nvgpu_set_enabled(g, NVGPU_SUPPORT_SCG, true);
#endif
	nvgpu_set_enabled(g, NVGPU_SUPPORT_GET_TEMPERATURE, true);
	if (nvgpu_has_syncpoints(g)) {
		nvgpu_set_enabled(g, NVGPU_SUPPORT_SYNCPOINT_ADDRESS, true);
		nvgpu_set_enabled(g, NVGPU_SUPPORT_USER_SYNCPOINT, true);
	}
	nvgpu_set_enabled(g, NVGPU_SUPPORT_USERMODE_SUBMIT, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_DEVICE_EVENTS, true);

	return 0;
}

static const struct gops_acr tu104_ops_acr = {
	.acr_init = nvgpu_acr_init,
	.acr_construct_execute = nvgpu_acr_construct_execute,
};

static const struct gops_sbr tu104_ops_sbr = {
	.sbr_pub_load_and_execute = nvgpu_sbr_pub_load_and_execute,
};

static const struct gops_func tu104_ops_func = {
	.get_full_phys_offset = tu104_func_get_full_phys_offset,
};

static const struct gops_bios tu104_ops_bios = {
#ifdef CONFIG_NVGPU_DGPU
	.bios_sw_init = nvgpu_bios_sw_init,
	.bios_sw_deinit = nvgpu_bios_sw_deinit,
	.wait_for_bios_init_done = nvgpu_bios_wait_for_init_done,
#endif /* CONFIG_NVGPU_DGPU */
	.get_aon_secure_scratch_reg = tu104_get_aon_secure_scratch_reg,
};

static const struct gops_ecc tu104_ops_ecc = {
	.ecc_init_support = nvgpu_ecc_init_support,
	.ecc_finalize_support = nvgpu_ecc_finalize_support,
	.ecc_remove_support = nvgpu_ecc_remove_support,
};

static const struct gops_ltc_intr tu104_ops_ltc_intr = {
	.configure = gv11b_ltc_intr_configure,
	.isr = gv11b_ltc_intr_isr,
#ifdef CONFIG_NVGPU_NON_FUSA
	.en_illegal_compstat = gv11b_ltc_intr_en_illegal_compstat,
#endif
};

static const struct gops_ltc tu104_ops_ltc = {
	.ecc_init = gv11b_lts_ecc_init,
	.init_ltc_support = nvgpu_init_ltc_support,
	.ltc_remove_support = nvgpu_ltc_remove_support,
	.determine_L2_size_bytes = gp10b_determine_L2_size_bytes,
	.init_fs_state = ltc_tu104_init_fs_state,
	.flush = gm20b_flush_ltc,
#if defined(CONFIG_NVGPU_NON_FUSA) || defined(CONFIG_NVGPU_KERNEL_MODE_SUBMIT)
	.set_enabled = gp10b_ltc_set_enabled,
#endif
#ifdef CONFIG_NVGPU_GRAPHICS
	.set_zbc_s_entry = gv11b_ltc_set_zbc_stencil_entry,
	.set_zbc_color_entry = gm20b_ltc_set_zbc_color_entry,
	.set_zbc_depth_entry = gm20b_ltc_set_zbc_depth_entry,
#endif /* CONFIG_NVGPU_GRAPHICS */
#ifdef CONFIG_NVGPU_DEBUGGER
	.pri_is_ltc_addr = gm20b_ltc_pri_is_ltc_addr,
	.is_ltcs_ltss_addr = gm20b_ltc_is_ltcs_ltss_addr,
	.is_ltcn_ltss_addr = gm20b_ltc_is_ltcn_ltss_addr,
	.split_lts_broadcast_addr = gm20b_ltc_split_lts_broadcast_addr,
	.split_ltc_broadcast_addr = gm20b_ltc_split_ltc_broadcast_addr,
	.pri_is_lts_tstg_addr = tu104_ltc_pri_is_lts_tstg_addr,
	.set_l2_sector_promotion = tu104_set_l2_sector_promotion,
#endif /* CONFIG_NVGPU_DEBUGGER */
};

#ifdef CONFIG_NVGPU_COMPRESSION
static const struct gops_cbc tu104_ops_cbc = {
	.cbc_init_support = nvgpu_cbc_init_support,
	.cbc_remove_support = nvgpu_cbc_remove_support,
	.init = gv11b_cbc_init,
	.alloc_comptags = tu104_cbc_alloc_comptags,
	.ctrl = tu104_cbc_ctrl,
	.fix_config = NULL,
};
#endif

static const struct gops_ce tu104_ops_ce = {
	.ce_init_support = nvgpu_ce_init_support,
#ifdef CONFIG_NVGPU_DGPU
	.ce_app_init_support = nvgpu_ce_app_init_support,
	.ce_app_suspend = nvgpu_ce_app_suspend,
	.ce_app_destroy = nvgpu_ce_app_destroy,
#endif
	.set_pce2lce_mapping = tu104_ce_set_pce2lce_mapping,
	.isr_stall = gv11b_ce_stall_isr,
#ifdef CONFIG_NVGPU_NONSTALL_INTR
	.isr_nonstall = NULL,
#endif
	.get_num_pce = gv11b_ce_get_num_pce,
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	.mthd_buffer_fault_in_bar2_fault = gv11b_ce_mthd_buffer_fault_in_bar2_fault,
#endif
	.init_prod_values = gv11b_ce_init_prod_values,
	.halt_engine = gv11b_ce_halt_engine,
	.request_idle = NULL,
	.get_inst_ptr_from_lce = gv11b_ce_get_inst_ptr_from_lce,
};

static const struct gops_gr_ecc tu104_ops_gr_ecc = {
	.detect = NULL,
	.gpc_tpc_ecc_init = gv11b_gr_gpc_tpc_ecc_init,
	.fecs_ecc_init = gv11b_gr_fecs_ecc_init,
	.gpc_tpc_ecc_deinit = gv11b_gr_gpc_tpc_ecc_deinit,
	.fecs_ecc_deinit = gv11b_gr_fecs_ecc_deinit,
};

static const struct gops_gr_ctxsw_prog tu104_ops_gr_ctxsw_prog = {
	.hw_get_fecs_header_size = gm20b_ctxsw_prog_hw_get_fecs_header_size,
	.get_patch_count = gm20b_ctxsw_prog_get_patch_count,
	.set_patch_count = gm20b_ctxsw_prog_set_patch_count,
	.set_patch_addr = gm20b_ctxsw_prog_set_patch_addr,
	.set_compute_preemption_mode_cta = gp10b_ctxsw_prog_set_compute_preemption_mode_cta,
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	.init_ctxsw_hdr_data = gp10b_ctxsw_prog_init_ctxsw_hdr_data,
	.disable_verif_features = gm20b_ctxsw_prog_disable_verif_features,
#endif
#ifdef CONFIG_NVGPU_SET_FALCON_ACCESS_MAP
	.set_priv_access_map_config_mode = gm20b_ctxsw_prog_set_config_mode_priv_access_map,
	.set_priv_access_map_addr = gm20b_ctxsw_prog_set_addr_priv_access_map,
#endif
	.set_context_buffer_ptr = gv11b_ctxsw_prog_set_context_buffer_ptr,
	.set_type_per_veid_header = gv11b_ctxsw_prog_set_type_per_veid_header,
#ifdef CONFIG_NVGPU_GRAPHICS
	.set_zcull_ptr = gv11b_ctxsw_prog_set_zcull_ptr,
	.set_zcull = gm20b_ctxsw_prog_set_zcull,
	.set_zcull_mode_no_ctxsw = gm20b_ctxsw_prog_set_zcull_mode_no_ctxsw,
	.is_zcull_mode_separate_buffer = gm20b_ctxsw_prog_is_zcull_mode_separate_buffer,
#endif /* CONFIG_NVGPU_GRAPHICS */
#ifdef CONFIG_NVGPU_GFXP
	.set_graphics_preemption_mode_gfxp = gp10b_ctxsw_prog_set_graphics_preemption_mode_gfxp,
	.set_full_preemption_ptr = gv11b_ctxsw_prog_set_full_preemption_ptr,
	.set_full_preemption_ptr_veid0 = gv11b_ctxsw_prog_set_full_preemption_ptr_veid0,
#endif /* CONFIG_NVGPU_GFXP */
#ifdef CONFIG_NVGPU_CILP
	.set_compute_preemption_mode_cilp = gp10b_ctxsw_prog_set_compute_preemption_mode_cilp,
#endif
#ifdef CONFIG_NVGPU_DEBUGGER
	.hw_get_gpccs_header_size = gm20b_ctxsw_prog_hw_get_gpccs_header_size,
	.hw_get_extended_buffer_segments_size_in_bytes = gm20b_ctxsw_prog_hw_get_extended_buffer_segments_size_in_bytes,
	.hw_extended_marker_size_in_bytes = gm20b_ctxsw_prog_hw_extended_marker_size_in_bytes,
	.hw_get_perf_counter_control_register_stride = gm20b_ctxsw_prog_hw_get_perf_counter_control_register_stride,
	.get_main_image_ctx_id = gm20b_ctxsw_prog_get_main_image_ctx_id,
	.set_pm_ptr = gv11b_ctxsw_prog_set_pm_ptr,
	.set_pm_mode = gm20b_ctxsw_prog_set_pm_mode,
	.set_pm_smpc_mode = gm20b_ctxsw_prog_set_pm_smpc_mode,
	.hw_get_pm_mode_no_ctxsw = gm20b_ctxsw_prog_hw_get_pm_mode_no_ctxsw,
	.hw_get_pm_mode_ctxsw = gm20b_ctxsw_prog_hw_get_pm_mode_ctxsw,
	.hw_get_pm_mode_stream_out_ctxsw = gv11b_ctxsw_prog_hw_get_pm_mode_stream_out_ctxsw,
	.set_cde_enabled = gm20b_ctxsw_prog_set_cde_enabled,
	.set_pc_sampling = NULL,
	.check_main_image_header_magic = gm20b_ctxsw_prog_check_main_image_header_magic,
	.check_local_header_magic = gm20b_ctxsw_prog_check_local_header_magic,
	.get_num_gpcs = gm20b_ctxsw_prog_get_num_gpcs,
	.get_num_tpcs = gm20b_ctxsw_prog_get_num_tpcs,
	.get_extended_buffer_size_offset = gm20b_ctxsw_prog_get_extended_buffer_size_offset,
	.get_ppc_info = gm20b_ctxsw_prog_get_ppc_info,
	.get_local_priv_register_ctl_offset = gm20b_ctxsw_prog_get_local_priv_register_ctl_offset,
	.set_pmu_options_boost_clock_frequencies = gp10b_ctxsw_prog_set_pmu_options_boost_clock_frequencies,
	.hw_get_perf_counter_register_stride = gv11b_ctxsw_prog_hw_get_perf_counter_register_stride,
	.hw_get_pm_gpc_gnic_stride = gm20b_ctxsw_prog_hw_get_pm_gpc_gnic_stride,
#endif /* CONFIG_NVGPU_DEBUGGER */
#ifdef CONFIG_DEBUG_FS
	.dump_ctxsw_stats = gp10b_ctxsw_prog_dump_ctxsw_stats,
#endif
#ifdef CONFIG_NVGPU_FECS_TRACE
	.hw_get_ts_tag_invalid_timestamp = gm20b_ctxsw_prog_hw_get_ts_tag_invalid_timestamp,
	.hw_get_ts_tag = gm20b_ctxsw_prog_hw_get_ts_tag,
	.hw_record_ts_timestamp = gm20b_ctxsw_prog_hw_record_ts_timestamp,
	.hw_get_ts_record_size_in_bytes = gm20b_ctxsw_prog_hw_get_ts_record_size_in_bytes,
	.is_ts_valid_record = gm20b_ctxsw_prog_is_ts_valid_record,
	.get_ts_buffer_aperture_mask = gm20b_ctxsw_prog_get_ts_buffer_aperture_mask,
	.set_ts_num_records = gm20b_ctxsw_prog_set_ts_num_records,
	.set_ts_buffer_ptr = gm20b_ctxsw_prog_set_ts_buffer_ptr,
#endif
};

static const struct gops_gr_config tu104_ops_gr_config = {
	.get_gpc_mask = gm20b_gr_config_get_gpc_mask,
	.get_gpc_tpc_mask = gm20b_gr_config_get_gpc_tpc_mask,
	.get_tpc_count_in_gpc = gm20b_gr_config_get_tpc_count_in_gpc,
	.get_pes_tpc_mask = gm20b_gr_config_get_pes_tpc_mask,
	.get_gpc_pes_mask = gv11b_gr_config_get_gpc_pes_mask,
	.get_pd_dist_skip_table_size = gm20b_gr_config_get_pd_dist_skip_table_size,
	.init_sm_id_table = gv100_gr_config_init_sm_id_table,
#ifdef CONFIG_NVGPU_GRAPHICS
	.get_zcull_count_in_gpc = gm20b_gr_config_get_zcull_count_in_gpc,
#endif /* CONFIG_NVGPU_GRAPHICS */
};

#ifdef CONFIG_NVGPU_FECS_TRACE
static const struct gops_gr_fecs_trace tu104_ops_gr_fecs_trace = {
	.alloc_user_buffer = nvgpu_gr_fecs_trace_ring_alloc,
	.free_user_buffer = nvgpu_gr_fecs_trace_ring_free,
	.get_mmap_user_buffer_info = nvgpu_gr_fecs_trace_get_mmap_buffer_info,
	.init = nvgpu_gr_fecs_trace_init,
	.deinit = nvgpu_gr_fecs_trace_deinit,
	.enable = nvgpu_gr_fecs_trace_enable,
	.disable = nvgpu_gr_fecs_trace_disable,
	.is_enabled = nvgpu_gr_fecs_trace_is_enabled,
	.reset = nvgpu_gr_fecs_trace_reset,
	.flush = NULL,
	.poll = nvgpu_gr_fecs_trace_poll,
	.bind_channel = nvgpu_gr_fecs_trace_bind_channel,
	.unbind_channel = nvgpu_gr_fecs_trace_unbind_channel,
	.max_entries = nvgpu_gr_fecs_trace_max_entries,
	.get_buffer_full_mailbox_val = gv11b_fecs_trace_get_buffer_full_mailbox_val,
	.get_read_index = gm20b_fecs_trace_get_read_index,
	.get_write_index = gm20b_fecs_trace_get_write_index,
	.set_read_index = gm20b_fecs_trace_set_read_index,
};
#endif

static const struct gops_gr_setup tu104_ops_gr_setup = {
	.alloc_obj_ctx = nvgpu_gr_setup_alloc_obj_ctx,
	.free_gr_ctx = nvgpu_gr_setup_free_gr_ctx,
	.free_subctx = nvgpu_gr_setup_free_subctx,
#ifdef CONFIG_NVGPU_GRAPHICS
	.bind_ctxsw_zcull = nvgpu_gr_setup_bind_ctxsw_zcull,
#endif /* CONFIG_NVGPU_GRAPHICS */
	.set_preemption_mode = nvgpu_gr_setup_set_preemption_mode,
};

#ifdef CONFIG_NVGPU_GRAPHICS
static const struct gops_gr_zbc tu104_ops_gr_zbc = {
	.add_color = gp10b_gr_zbc_add_color,
	.add_depth = gp10b_gr_zbc_add_depth,
	.set_table = nvgpu_gr_zbc_set_table,
	.query_table = nvgpu_gr_zbc_query_table,
	.add_stencil = gv11b_gr_zbc_add_stencil,
	.get_gpcs_swdx_dss_zbc_c_format_reg = gv11b_gr_zbc_get_gpcs_swdx_dss_zbc_c_format_reg,
	.get_gpcs_swdx_dss_zbc_z_format_reg = gv11b_gr_zbc_get_gpcs_swdx_dss_zbc_z_format_reg,
	.init_table_indices = gv11b_gr_zbc_init_table_indices,
};
#endif

#ifdef CONFIG_NVGPU_GRAPHICS
static const struct gops_gr_zcull tu104_ops_gr_zcull = {
	.init_zcull_hw = gm20b_gr_init_zcull_hw,
	.get_zcull_info = gm20b_gr_get_zcull_info,
	.program_zcull_mapping = gv11b_gr_program_zcull_mapping,
};
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
static const struct gops_gr_hwpm_map tu104_ops_gr_hwpm_map = {
	.align_regs_perf_pma = gv100_gr_hwpm_map_align_regs_perf_pma,
	.get_active_fbpa_mask = gv100_gr_hwpm_map_get_active_fbpa_mask,
};
#endif

static const struct gops_gr_init tu104_ops_gr_init = {
	.get_no_of_sm = nvgpu_gr_get_no_of_sm,
	.get_nonpes_aware_tpc = gv11b_gr_init_get_nonpes_aware_tpc,
	.ecc_scrub_reg = NULL,
	.pes_vsc_stream = gm20b_gr_init_pes_vsc_stream,
	.gpc_mmu = gv11b_gr_init_gpc_mmu,
	.fifo_access = gm20b_gr_init_fifo_access,
#ifdef CONFIG_NVGPU_SET_FALCON_ACCESS_MAP
	.get_access_map = gv11b_gr_init_get_access_map,
#endif
	.get_sm_id_size = gp10b_gr_init_get_sm_id_size,
	.sm_id_config = gv11b_gr_init_sm_id_config,
	.sm_id_numbering = gv11b_gr_init_sm_id_numbering,
	.tpc_mask = NULL,
	.fs_state = gv11b_gr_init_fs_state,
	.pd_tpc_per_gpc = gm20b_gr_init_pd_tpc_per_gpc,
	.pd_skip_table_gpc = gm20b_gr_init_pd_skip_table_gpc,
	.cwd_gpcs_tpcs_num = gm20b_gr_init_cwd_gpcs_tpcs_num,
	.gr_load_tpc_mask = NULL,
	.wait_empty = gp10b_gr_init_wait_empty,
	.wait_idle = gm20b_gr_init_wait_idle,
	.wait_fe_idle = gm20b_gr_init_wait_fe_idle,
#ifdef CONFIG_NVGPU_GR_GOLDEN_CTX_VERIFICATION
	.restore_stats_counter_bundle_data = gv11b_gr_init_restore_stats_counter_bundle_data,
#endif
	.fe_pwr_mode_force_on = gm20b_gr_init_fe_pwr_mode_force_on,
	.override_context_reset = gm20b_gr_init_override_context_reset,
	.fe_go_idle_timeout = gv11b_gr_init_fe_go_idle_timeout,
	.load_method_init = gm20b_gr_init_load_method_init,
	.commit_global_timeslice = gv11b_gr_init_commit_global_timeslice,
	.get_bundle_cb_default_size = tu104_gr_init_get_bundle_cb_default_size,
	.get_min_gpm_fifo_depth = tu104_gr_init_get_min_gpm_fifo_depth,
	.get_bundle_cb_token_limit = tu104_gr_init_get_bundle_cb_token_limit,
	.get_attrib_cb_default_size = tu104_gr_init_get_attrib_cb_default_size,
	.get_alpha_cb_default_size = tu104_gr_init_get_alpha_cb_default_size,
	.get_attrib_cb_size = gv11b_gr_init_get_attrib_cb_size,
	.get_alpha_cb_size = gv11b_gr_init_get_alpha_cb_size,
	.get_global_attr_cb_size = gv11b_gr_init_get_global_attr_cb_size,
	.get_global_ctx_cb_buffer_size = gm20b_gr_init_get_global_ctx_cb_buffer_size,
	.get_global_ctx_pagepool_buffer_size = gm20b_gr_init_get_global_ctx_pagepool_buffer_size,
	.commit_global_bundle_cb = gp10b_gr_init_commit_global_bundle_cb,
	.pagepool_default_size = gp10b_gr_init_pagepool_default_size,
	.commit_global_pagepool = gp10b_gr_init_commit_global_pagepool,
	.commit_global_attrib_cb = gv11b_gr_init_commit_global_attrib_cb,
	.commit_global_cb_manager = gp10b_gr_init_commit_global_cb_manager,
#ifdef CONFIG_NVGPU_SM_DIVERSITY
	.commit_sm_id_programming = gv11b_gr_init_commit_sm_id_programming,
#endif
	.pipe_mode_override = gm20b_gr_init_pipe_mode_override,
#ifdef CONFIG_NVGPU_GR_GOLDEN_CTX_VERIFICATION
	.load_sw_bundle_init = gv11b_gr_init_load_sw_bundle_init,
#else
	.load_sw_bundle_init = gm20b_gr_init_load_sw_bundle_init,
#endif
	.load_sw_veid_bundle = gv11b_gr_init_load_sw_veid_bundle,
	.load_sw_bundle64 = tu104_gr_init_load_sw_bundle64,
	.get_max_subctx_count = gv11b_gr_init_get_max_subctx_count,
	.get_patch_slots = gv11b_gr_init_get_patch_slots,
	.detect_sm_arch = gv11b_gr_init_detect_sm_arch,
	.get_supported__preemption_modes = gp10b_gr_init_get_supported_preemption_modes,
	.get_default_preemption_modes = gp10b_gr_init_get_default_preemption_modes,
	.is_allowed_sw_bundle = gm20b_gr_init_is_allowed_sw_bundle,
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	.wait_initialized = nvgpu_gr_wait_initialized,
#endif
#ifdef CONFIG_NVGPU_GRAPHICS
	.rop_mapping = gv11b_gr_init_rop_mapping,
	.get_rtv_cb_size = tu104_gr_init_get_rtv_cb_size,
	.commit_rtv_cb = tu104_gr_init_commit_rtv_cb,
#endif /* CONFIG_NVGPU_GRAPHICS */
#ifdef CONFIG_NVGPU_GFXP
	.preemption_state = gv11b_gr_init_preemption_state,
	.get_ctx_attrib_cb_size = gp10b_gr_init_get_ctx_attrib_cb_size,
	.commit_cbes_reserve = gv11b_gr_init_commit_cbes_reserve,
	.commit_gfxp_rtv_cb = tu104_gr_init_commit_gfxp_rtv_cb,
	.get_gfxp_rtv_cb_size = tu104_gr_init_get_gfxp_rtv_cb_size,
	.gfxp_wfi_timeout = gv11b_gr_init_commit_gfxp_wfi_timeout,
	.get_attrib_cb_gfxp_default_size = tu104_gr_init_get_attrib_cb_gfxp_default_size,
	.get_attrib_cb_gfxp_size = tu104_gr_init_get_attrib_cb_gfxp_size,
	.get_ctx_spill_size = gv11b_gr_init_get_ctx_spill_size,
	.get_ctx_pagepool_size = gp10b_gr_init_get_ctx_pagepool_size,
	.get_ctx_betacb_size = gv11b_gr_init_get_ctx_betacb_size,
	.commit_ctxsw_spill = gv11b_gr_init_commit_ctxsw_spill,
#endif /* CONFIG_NVGPU_GFXP */
};

static const struct gops_gr_intr tu104_ops_gr_intr = {
	.handle_fecs_error = gv11b_gr_intr_handle_fecs_error,
	.handle_sw_method = tu104_gr_intr_handle_sw_method,
	.handle_class_error = gp10b_gr_intr_handle_class_error,
	.clear_pending_interrupts = gm20b_gr_intr_clear_pending_interrupts,
	.read_pending_interrupts = gm20b_gr_intr_read_pending_interrupts,
	.handle_exceptions = gm20b_gr_intr_handle_exceptions,
	.read_gpc_tpc_exception = gm20b_gr_intr_read_gpc_tpc_exception,
	.read_gpc_exception = gm20b_gr_intr_read_gpc_exception,
	.read_exception1 = gm20b_gr_intr_read_exception1,
	.trapped_method_info = gm20b_gr_intr_get_trapped_method_info,
	.handle_semaphore_pending = nvgpu_gr_intr_handle_semaphore_pending,
	.handle_notify_pending = nvgpu_gr_intr_handle_notify_pending,
	.handle_gcc_exception = gv11b_gr_intr_handle_gcc_exception,
	.handle_gpc_gpcmmu_exception = gv11b_gr_intr_handle_gpc_gpcmmu_exception,
	.handle_gpc_gpccs_exception = gv11b_gr_intr_handle_gpc_gpccs_exception,
	.get_tpc_exception = gm20b_gr_intr_get_tpc_exception,
	.handle_tpc_mpc_exception = gv11b_gr_intr_handle_tpc_mpc_exception,
	.enable_hww_exceptions = gv11b_gr_intr_enable_hww_exceptions,
	.enable_interrupts = gm20b_gr_intr_enable_interrupts,
	.enable_gpc_exceptions = tu104_gr_intr_enable_gpc_exceptions,
	.enable_exceptions = gv11b_gr_intr_enable_exceptions,
	.handle_sm_exception = nvgpu_gr_intr_handle_sm_exception,
	.stall_isr = nvgpu_gr_intr_stall_isr,
	.flush_channel_tlb = nvgpu_gr_intr_flush_channel_tlb,
	.set_hww_esr_report_mask = gv11b_gr_intr_set_hww_esr_report_mask,
	.handle_tpc_sm_ecc_exception = gv11b_gr_intr_handle_tpc_sm_ecc_exception,
	.get_esr_sm_sel = gv11b_gr_intr_get_esr_sm_sel,
	.clear_sm_hww = gv11b_gr_intr_clear_sm_hww,
	.handle_ssync_hww = gv11b_gr_intr_handle_ssync_hww,
	.log_mme_exception = tu104_gr_intr_log_mme_exception,
	.record_sm_error_state = gv11b_gr_intr_record_sm_error_state,
	.get_sm_hww_warp_esr = gv11b_gr_intr_get_warp_esr_sm_hww,
	.get_sm_hww_warp_esr_pc = gv11b_gr_intr_get_warp_esr_pc_sm_hww,
	.get_sm_hww_global_esr = gv11b_gr_intr_get_sm_hww_global_esr,
	.get_sm_no_lock_down_hww_global_esr_mask = gv11b_gr_intr_get_sm_no_lock_down_hww_global_esr_mask,
	.sm_ecc_status_errors = gv11b_gr_intr_sm_ecc_status_errors,
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	.handle_tex_exception = NULL,
	.set_shader_exceptions = gv11b_gr_intr_set_shader_exceptions,
	.tpc_exception_sm_enable = gm20b_gr_intr_tpc_exception_sm_enable,
#endif
#ifdef CONFIG_NVGPU_DEBUGGER
	.tpc_exception_sm_disable = gm20b_gr_intr_tpc_exception_sm_disable,
	.tpc_enabled_exceptions = gm20b_gr_intr_tpc_enabled_exceptions,
#endif
};

static const struct gops_gr_falcon tu104_ops_gr_falcon = {
	.handle_fecs_ecc_error = gv11b_gr_falcon_handle_fecs_ecc_error,
	.read_fecs_ctxsw_mailbox = gm20b_gr_falcon_read_mailbox_fecs_ctxsw,
	.fecs_host_clear_intr = gm20b_gr_falcon_fecs_host_clear_intr,
	.fecs_host_intr_status = gm20b_gr_falcon_fecs_host_intr_status,
	.fecs_base_addr = gm20b_gr_falcon_fecs_base_addr,
	.gpccs_base_addr = gm20b_gr_falcon_gpccs_base_addr,
	.set_current_ctx_invalid = gm20b_gr_falcon_set_current_ctx_invalid,
	.dump_stats = gm20b_gr_falcon_dump_stats,
	.fecs_ctxsw_mailbox_size = gm20b_gr_falcon_get_fecs_ctxsw_mailbox_size,
	.fecs_ctxsw_clear_mailbox = gm20b_gr_falcon_fecs_ctxsw_clear_mailbox,
	.get_fecs_ctx_state_store_major_rev_id = gm20b_gr_falcon_get_fecs_ctx_state_store_major_rev_id,
	.start_gpccs = gm20b_gr_falcon_start_gpccs,
	.start_fecs = gm20b_gr_falcon_start_fecs,
	.get_gpccs_start_reg_offset = gm20b_gr_falcon_get_gpccs_start_reg_offset,
	.bind_instblk = NULL,
	.load_ctxsw_ucode = nvgpu_gr_falcon_load_secure_ctxsw_ucode,
	.wait_mem_scrubbing = gm20b_gr_falcon_wait_mem_scrubbing,
	.wait_ctxsw_ready = gm20b_gr_falcon_wait_ctxsw_ready,
	.ctrl_ctxsw = tu104_gr_falcon_ctrl_ctxsw,
	.get_current_ctx = gm20b_gr_falcon_get_current_ctx,
	.get_ctx_ptr = gm20b_gr_falcon_get_ctx_ptr,
	.get_fecs_current_ctx_data = gm20b_gr_falcon_get_fecs_current_ctx_data,
	.init_ctx_state = gp10b_gr_falcon_init_ctx_state,
	.fecs_host_int_enable = gv11b_gr_falcon_fecs_host_int_enable,
	.read_fecs_ctxsw_status0 = gm20b_gr_falcon_read_status0_fecs_ctxsw,
	.read_fecs_ctxsw_status1 = gm20b_gr_falcon_read_status1_fecs_ctxsw,
#ifdef CONFIG_NVGPU_GR_FALCON_NON_SECURE_BOOT
	.load_ctxsw_ucode_header = gm20b_gr_falcon_load_ctxsw_ucode_header,
	.load_ctxsw_ucode_boot = gm20b_gr_falcon_load_ctxsw_ucode_boot,
	.load_gpccs_dmem = gm20b_gr_falcon_load_gpccs_dmem,
	.gpccs_dmemc_write = gm20b_gr_falcon_gpccs_dmemc_write,
	.load_fecs_dmem = gm20b_gr_falcon_load_fecs_dmem,
	.fecs_dmemc_write = gm20b_gr_falcon_fecs_dmemc_write,
	.load_gpccs_imem = gm20b_gr_falcon_load_gpccs_imem,
	.gpccs_imemc_write = gm20b_gr_falcon_gpccs_imemc_write,
	.load_fecs_imem = gm20b_gr_falcon_load_fecs_imem,
	.fecs_imemc_write = gm20b_gr_falcon_fecs_imemc_write,
	.start_ucode = gm20b_gr_falcon_start_ucode,
#endif
#ifdef CONFIG_NVGPU_SIM
	.configure_fmodel = gm20b_gr_falcon_configure_fmodel,
#endif
};

static const struct gops_gr tu104_ops_gr = {
	.gr_init_support = nvgpu_gr_init_support,
	.gr_suspend = nvgpu_gr_suspend,
#ifdef CONFIG_NVGPU_DEBUGGER
	.get_gr_status = gr_gm20b_get_gr_status,
	.set_alpha_circular_buffer_size = gr_gv11b_set_alpha_circular_buffer_size,
	.set_circular_buffer_size = gr_gv11b_set_circular_buffer_size,
	.get_sm_dsm_perf_regs = gv11b_gr_get_sm_dsm_perf_regs,
	.get_sm_dsm_perf_ctrl_regs = gr_tu104_get_sm_dsm_perf_ctrl_regs,
#ifdef CONFIG_NVGPU_TEGRA_FUSE
	.set_gpc_tpc_mask = gr_gv100_set_gpc_tpc_mask,
#endif
	.dump_gr_regs = gr_gv11b_dump_gr_status_regs,
	.update_pc_sampling = gr_gm20b_update_pc_sampling,
	.init_sm_dsm_reg_info = gr_tu104_init_sm_dsm_reg_info,
	.init_cyclestats = gr_gm20b_init_cyclestats,
	.set_sm_debug_mode = gv11b_gr_set_sm_debug_mode,
	.bpt_reg_info = gv11b_gr_bpt_reg_info,
	.update_smpc_ctxsw_mode = gr_gk20a_update_smpc_ctxsw_mode,
	.update_smpc_global_mode = tu104_gr_update_smpc_global_mode,
	.set_mmu_debug_mode = gm20b_gr_set_mmu_debug_mode,
	.update_hwpm_ctxsw_mode = gr_gk20a_update_hwpm_ctxsw_mode,
	.disable_cau = tu104_gr_disable_cau,
	.disable_smpc = tu104_gr_disable_smpc,
	.get_hwpm_cau_init_data = tu104_gr_get_hwpm_cau_init_data,
	.init_cau = tu104_gr_init_cau,
	.clear_sm_error_state = gv11b_gr_clear_sm_error_state,
	.suspend_contexts = gr_gp10b_suspend_contexts,
	.resume_contexts = gr_gk20a_resume_contexts,
	.trigger_suspend = NULL,
	.wait_for_pause = NULL,
	.resume_from_pause = NULL,
	.clear_sm_errors = gr_gk20a_clear_sm_errors,
	.is_tsg_ctx_resident = gk20a_is_tsg_ctx_resident,
	.sm_debugger_attached = gv11b_gr_sm_debugger_attached,
	.suspend_single_sm = gv11b_gr_suspend_single_sm,
	.suspend_all_sms = gv11b_gr_suspend_all_sms,
	.resume_single_sm = gv11b_gr_resume_single_sm,
	.resume_all_sms = gv11b_gr_resume_all_sms,
	.lock_down_sm = gv11b_gr_lock_down_sm,
	.wait_for_sm_lock_down = gv11b_gr_wait_for_sm_lock_down,
	.init_ovr_sm_dsm_perf = gv11b_gr_init_ovr_sm_dsm_perf,
	.get_ovr_perf_regs = gv11b_gr_get_ovr_perf_regs,
#ifdef CONFIG_NVGPU_CHANNEL_TSG_SCHEDULING
	.set_boosted_ctx = gr_gp10b_set_boosted_ctx,
#endif
	.pre_process_sm_exception = gr_gv11b_pre_process_sm_exception,
	.set_bes_crop_debug3 = gr_gp10b_set_bes_crop_debug3,
	.set_bes_crop_debug4 = gr_gp10b_set_bes_crop_debug4,
	.is_etpc_addr = gv11b_gr_pri_is_etpc_addr,
	.egpc_etpc_priv_addr_table = gv11b_gr_egpc_etpc_priv_addr_table,
	.get_egpc_base = gv11b_gr_get_egpc_base,
	.get_egpc_etpc_num = gv11b_gr_get_egpc_etpc_num,
	.is_egpc_addr = gv11b_gr_pri_is_egpc_addr,
	.decode_egpc_addr = gv11b_gr_decode_egpc_addr,
	.decode_priv_addr = gr_tu104_decode_priv_addr,
	.create_priv_addr_table = gr_gv11b_create_priv_addr_table,
	.split_fbpa_broadcast_addr = gr_gv100_split_fbpa_broadcast_addr,
	.get_offset_in_gpccs_segment = gr_tu104_get_offset_in_gpccs_segment,
	.process_context_buffer_priv_segment = gr_gk20a_process_context_buffer_priv_segment,
	.get_ctx_buffer_offsets = gr_gk20a_get_ctx_buffer_offsets,
	.get_pm_ctx_buffer_offsets = gr_gk20a_get_pm_ctx_buffer_offsets,
	.find_priv_offset_in_buffer =
		gr_gk20a_find_priv_offset_in_buffer,
	.set_debug_mode = gm20b_gr_set_debug_mode,
	.esr_bpt_pending_events = gv11b_gr_esr_bpt_pending_events,
	.check_warp_esr_error = gv11b_gr_check_warp_esr_error,
#endif /* CONFIG_NVGPU_DEBUGGER */
};

static const struct gops_class tu104_ops_gpu_class = {
	.is_valid = tu104_class_is_valid,
	.is_valid_compute = tu104_class_is_valid_compute,
#ifdef CONFIG_NVGPU_GRAPHICS
	.is_valid_gfx = tu104_class_is_valid_gfx,
#endif
};

static const struct gops_fb_ecc tu104_ops_fb_ecc = {
	.init = gv11b_fb_ecc_init,
	.free = gv11b_fb_ecc_free,
	.l2tlb_error_mask = gv11b_fb_ecc_l2tlb_error_mask,
};

static const struct gops_fb_intr tu104_ops_fb_intr = {
	.enable = tu104_fb_intr_enable,
	.disable = tu104_fb_intr_disable,
	.isr = tu104_fb_intr_isr,
	.is_mmu_fault_pending = tu104_fb_intr_is_mmu_fault_pending,
	.handle_ecc = gv11b_fb_intr_handle_ecc,
	.handle_ecc_l2tlb = gv11b_fb_intr_handle_ecc_l2tlb,
	.handle_ecc_hubtlb = gv11b_fb_intr_handle_ecc_hubtlb,
	.handle_ecc_fillunit = gv11b_fb_intr_handle_ecc_fillunit,
};

static const struct gops_fb tu104_ops_fb = {
	.fbpa_ecc_init = tu104_fbpa_ecc_init,
	.fbpa_ecc_free = tu104_fbpa_ecc_free,
	.init_hw = gv11b_fb_init_hw,
	.init_fs_state = gp106_fb_init_fs_state,
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	.set_atomic_mode = tu104_fb_set_atomic_mode,
#endif
	.set_mmu_page_size = NULL,
	.mmu_ctrl = gm20b_fb_mmu_ctrl,
	.mmu_debug_ctrl = gm20b_fb_mmu_debug_ctrl,
	.mmu_debug_wr = gm20b_fb_mmu_debug_wr,
	.mmu_debug_rd = gm20b_fb_mmu_debug_rd,
#ifdef CONFIG_NVGPU_COMPRESSION
	.cbc_configure = tu104_fb_cbc_configure,
	.cbc_get_alignment = tu104_fb_cbc_get_alignment,
	.set_use_full_comp_tag_line = gm20b_fb_set_use_full_comp_tag_line,
	.compression_page_size = gp10b_fb_compression_page_size,
	.compressible_page_size = gp10b_fb_compressible_page_size,
	.compression_align_mask = gm20b_fb_compression_align_mask,
#endif
	.vpr_info_fetch = NULL,
	.dump_vpr_info = NULL,
	.dump_wpr_info = gm20b_fb_dump_wpr_info,
	.read_wpr_info = gm20b_fb_read_wpr_info,
#ifdef CONFIG_NVGPU_DEBUGGER
	.is_debug_mode_enabled = gm20b_fb_debug_mode_enabled,
	.set_debug_mode = gm20b_fb_set_debug_mode,
	.set_mmu_debug_mode = gv100_fb_set_mmu_debug_mode,
#endif
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	.tlb_invalidate = fb_tu104_tlb_invalidate,
#endif
#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	.handle_replayable_fault = gv11b_fb_handle_replayable_mmu_fault,
	.mmu_invalidate_replay = tu104_fb_mmu_invalidate_replay,
#endif
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	.mem_unlock = gv100_fb_memory_unlock,
	.init_nvlink = gv100_fb_init_nvlink,
	.enable_nvlink = gv100_fb_enable_nvlink,
#endif
	.init_fbpa = tu104_fbpa_init,
	.handle_fbpa_intr = tu104_fbpa_handle_intr,
	.write_mmu_fault_buffer_lo_hi = tu104_fb_write_mmu_fault_buffer_lo_hi,
	.write_mmu_fault_buffer_get = tu104_fb_write_mmu_fault_buffer_get,
	.write_mmu_fault_buffer_size = tu104_fb_write_mmu_fault_buffer_size,
	.write_mmu_fault_status = tu104_fb_write_mmu_fault_status,
	.read_mmu_fault_buffer_get = tu104_fb_read_mmu_fault_buffer_get,
	.read_mmu_fault_buffer_put = tu104_fb_read_mmu_fault_buffer_put,
	.read_mmu_fault_buffer_size = tu104_fb_read_mmu_fault_buffer_size,
	.read_mmu_fault_addr_lo_hi = tu104_fb_read_mmu_fault_addr_lo_hi,
	.read_mmu_fault_inst_lo_hi = tu104_fb_read_mmu_fault_inst_lo_hi,
	.read_mmu_fault_info = tu104_fb_read_mmu_fault_info,
	.read_mmu_fault_status = tu104_fb_read_mmu_fault_status,
	.is_fault_buf_enabled = gv11b_fb_is_fault_buf_enabled,
	.fault_buf_set_state_hw = gv11b_fb_fault_buf_set_state_hw,
	.fault_buf_configure_hw = gv11b_fb_fault_buf_configure_hw,
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
#ifdef CONFIG_NVGPU_DGPU
	.get_vidmem_size = tu104_fb_get_vidmem_size,
#endif
	.apply_pdb_cache_errata = tu104_fb_apply_pdb_cache_errata,
#endif
};

static const struct gops_nvdec tu104_ops_nvdec = {
	.falcon_base_addr = tu104_nvdec_falcon_base_addr,
};

static const struct gops_cg tu104_ops_cg = {
	.slcg_bus_load_gating_prod = tu104_slcg_bus_load_gating_prod,
	.slcg_ce2_load_gating_prod = tu104_slcg_ce2_load_gating_prod,
	.slcg_chiplet_load_gating_prod = tu104_slcg_chiplet_load_gating_prod,
	.slcg_fb_load_gating_prod = tu104_slcg_fb_load_gating_prod,
	.slcg_fifo_load_gating_prod = tu104_slcg_fifo_load_gating_prod,
	.slcg_gr_load_gating_prod = tu104_slcg_gr_load_gating_prod,
	.slcg_ltc_load_gating_prod = tu104_slcg_ltc_load_gating_prod,
	.slcg_perf_load_gating_prod = tu104_slcg_perf_load_gating_prod,
	.slcg_priring_load_gating_prod = tu104_slcg_priring_load_gating_prod,
	.slcg_pmu_load_gating_prod = tu104_slcg_pmu_load_gating_prod,
	.slcg_therm_load_gating_prod = tu104_slcg_therm_load_gating_prod,
	.slcg_xbar_load_gating_prod = tu104_slcg_xbar_load_gating_prod,
	.slcg_hshub_load_gating_prod = tu104_slcg_hshub_load_gating_prod,
	.blcg_bus_load_gating_prod = tu104_blcg_bus_load_gating_prod,
	.blcg_ce_load_gating_prod = tu104_blcg_ce_load_gating_prod,
	.blcg_fb_load_gating_prod = tu104_blcg_fb_load_gating_prod,
	.blcg_fifo_load_gating_prod = tu104_blcg_fifo_load_gating_prod,
	.blcg_gr_load_gating_prod = tu104_blcg_gr_load_gating_prod,
	.blcg_ltc_load_gating_prod = tu104_blcg_ltc_load_gating_prod,
	.blcg_pmu_load_gating_prod = tu104_blcg_pmu_load_gating_prod,
	.blcg_xbar_load_gating_prod = tu104_blcg_xbar_load_gating_prod,
	.blcg_hshub_load_gating_prod = tu104_blcg_hshub_load_gating_prod,
};

static const struct gops_fifo tu104_ops_fifo = {
	.fifo_init_support = nvgpu_fifo_init_support,
	.fifo_suspend = nvgpu_fifo_suspend,
	.init_fifo_setup_hw = tu104_init_fifo_setup_hw,
	.preempt_channel = gv11b_fifo_preempt_channel,
	.preempt_tsg = nvgpu_fifo_preempt_tsg,
	.preempt_trigger = gv11b_fifo_preempt_trigger,
	.preempt_poll_pbdma = gv11b_fifo_preempt_poll_pbdma,
	.is_preempt_pending = gv11b_fifo_is_preempt_pending,
	.reset_enable_hw = gv11b_init_fifo_reset_enable_hw,
#ifdef CONFIG_NVGPU_RECOVERY
	.recover = gv11b_fifo_recover,
#endif
	.intr_set_recover_mask = gv11b_fifo_intr_set_recover_mask,
	.intr_unset_recover_mask = gv11b_fifo_intr_unset_recover_mask,
	.setup_sw = nvgpu_fifo_setup_sw,
	.cleanup_sw = nvgpu_fifo_cleanup_sw,
#ifdef CONFIG_NVGPU_DEBUGGER
	.set_sm_exception_type_mask = nvgpu_tsg_set_sm_exception_type_mask,
#endif
	.intr_0_enable = gv11b_fifo_intr_0_enable,
	.intr_1_enable = gk20a_fifo_intr_1_enable,
	.intr_0_isr = gv11b_fifo_intr_0_isr,
	.intr_1_isr = gk20a_fifo_intr_1_isr,
	.handle_sched_error = gv11b_fifo_handle_sched_error,
	.ctxsw_timeout_enable = gv11b_fifo_ctxsw_timeout_enable,
	.handle_ctxsw_timeout = gv11b_fifo_handle_ctxsw_timeout,
	.trigger_mmu_fault = NULL,
	.get_mmu_fault_info = NULL,
	.get_mmu_fault_desc = NULL,
	.get_mmu_fault_client_desc = NULL,
	.get_mmu_fault_gpc_desc = NULL,
	.get_runlist_timeslice = gk20a_fifo_get_runlist_timeslice,
	.get_pb_timeslice = gk20a_fifo_get_pb_timeslice,
	.mmu_fault_id_to_pbdma_id = gv11b_fifo_mmu_fault_id_to_pbdma_id,
	.find_pbdma_for_runlist = gk20a_fifo_find_pbdma_for_runlist,
};

static const struct gops_engine tu104_ops_engine = {
	.is_fault_engine_subid_gpc = gv11b_is_fault_engine_subid_gpc,
	.init_ce_info = gp10b_engine_init_ce_info,
};

static const struct gops_pbdma tu104_ops_pbdma = {
	.setup_sw = nvgpu_pbdma_setup_sw,
	.cleanup_sw = nvgpu_pbdma_cleanup_sw,
	.setup_hw = gv11b_pbdma_setup_hw,
	.intr_enable = gv11b_pbdma_intr_enable,
	.acquire_val = gm20b_pbdma_acquire_val,
	.get_signature = gp10b_pbdma_get_signature,
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	.syncpt_debug_dump = NULL,
	.dump_status = gm20b_pbdma_dump_status,
#endif
	.handle_intr = gm20b_pbdma_handle_intr,
	.handle_intr_0 = gv11b_pbdma_handle_intr_0,
	.handle_intr_1 = gv11b_pbdma_handle_intr_1,
	.read_data = tu104_pbdma_read_data,
	.reset_header = tu104_pbdma_reset_header,
	.device_fatal_0_intr_descs = gm20b_pbdma_device_fatal_0_intr_descs,
	.channel_fatal_0_intr_descs = gv11b_pbdma_channel_fatal_0_intr_descs,
	.restartable_0_intr_descs = gm20b_pbdma_restartable_0_intr_descs,
	.format_gpfifo_entry = gm20b_pbdma_format_gpfifo_entry,
	.get_gp_base = gm20b_pbdma_get_gp_base,
	.get_gp_base_hi = gm20b_pbdma_get_gp_base_hi,
	.get_fc_formats = NULL,
	.get_fc_pb_header = gv11b_pbdma_get_fc_pb_header,
	.get_fc_subdevice = gm20b_pbdma_get_fc_subdevice,
	.get_fc_target = gv11b_pbdma_get_fc_target,
	.get_ctrl_hce_priv_mode_yes = gm20b_pbdma_get_ctrl_hce_priv_mode_yes,
	.get_userd_aperture_mask = gm20b_pbdma_get_userd_aperture_mask,
	.get_userd_addr = gm20b_pbdma_get_userd_addr,
	.get_userd_hi_addr = gm20b_pbdma_get_userd_hi_addr,
	.get_fc_runlist_timeslice = gp10b_pbdma_get_fc_runlist_timeslice,
	.get_config_auth_level_privileged = gp10b_pbdma_get_config_auth_level_privileged,
	.set_channel_info_veid = gv11b_pbdma_set_channel_info_veid,
	.config_userd_writeback_enable = gv11b_pbdma_config_userd_writeback_enable,
};

#ifdef CONFIG_TEGRA_GK20A_NVHOST
static const struct gops_sync_syncpt tu104_ops_sync_syncpt = {
	.get_sync_ro_map = gv11b_syncpt_get_sync_ro_map,
	.alloc_buf = gv11b_syncpt_alloc_buf,
	.free_buf = gv11b_syncpt_free_buf,
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	.add_wait_cmd = gv11b_syncpt_add_wait_cmd,
	.get_wait_cmd_size = gv11b_syncpt_get_wait_cmd_size,
	.add_incr_cmd = gv11b_syncpt_add_incr_cmd,
	.get_incr_cmd_size = gv11b_syncpt_get_incr_cmd_size,
	.get_incr_per_release = gv11b_syncpt_get_incr_per_release,
#endif
};
#endif

#if defined(CONFIG_NVGPU_KERNEL_MODE_SUBMIT) && \
	defined(CONFIG_NVGPU_SW_SEMAPHORE)
static const struct gops_sync_sema tu104_ops_sync_sema = {
	.add_wait_cmd = gv11b_sema_add_wait_cmd,
	.get_wait_cmd_size = gv11b_sema_get_wait_cmd_size,
	.add_incr_cmd = gv11b_sema_add_incr_cmd,
	.get_incr_cmd_size = gv11b_sema_get_incr_cmd_size,
};
#endif

static const struct gops_sync tu104_ops_sync = {
};

static const struct gops_engine_status tu104_ops_engine_status = {
	.read_engine_status_info = gv100_read_engine_status_info,
	.dump_engine_status = gv100_dump_engine_status,
};

static const struct gops_pbdma_status tu104_ops_pbdma_status = {
	.read_pbdma_status_info = gm20b_read_pbdma_status_info,
};

static const struct gops_ramfc tu104_ops_ramfc = {
	.setup = tu104_ramfc_setup,
	.capture_ram_dump = gv11b_ramfc_capture_ram_dump,
	.commit_userd = gp10b_ramfc_commit_userd,
	.get_syncpt = NULL,
	.set_syncpt = NULL,
};

static const struct gops_ramin tu104_ops_ramin = {
	.set_gr_ptr = gv11b_ramin_set_gr_ptr,
	.set_big_page_size = gm20b_ramin_set_big_page_size,
	.init_pdb = gv11b_ramin_init_pdb,
	.init_subctx_pdb = gv11b_ramin_init_subctx_pdb,
	.set_adr_limit = NULL,
	.base_shift = gk20a_ramin_base_shift,
	.alloc_size = gk20a_ramin_alloc_size,
	.set_eng_method_buffer = gv11b_ramin_set_eng_method_buffer,
	.init_pdb_cache_errata = tu104_ramin_init_pdb_cache_errata,
	.deinit_pdb_cache_errata = tu104_ramin_deinit_pdb_cache_errata,
};

static const struct gops_runlist tu104_ops_runlist = {
	.update = nvgpu_runlist_update,
	.reload = nvgpu_runlist_reload,
	.count_max = tu104_runlist_count_max,
	.entry_size = tu104_runlist_entry_size,
	.length_max = gk20a_runlist_length_max,
	.get_tsg_entry = gv11b_runlist_get_tsg_entry,
	.get_ch_entry = gv11b_runlist_get_ch_entry,
	.hw_submit = tu104_runlist_hw_submit,
	.wait_pending = tu104_runlist_wait_pending,
	.write_state = gk20a_runlist_write_state,
	.init_enginfo = nvgpu_runlist_init_enginfo,
	.get_tsg_max_timeslice = gv11b_runlist_max_timeslice,
	.get_max_channels_per_tsg = gv11b_runlist_get_max_channels_per_tsg,
};

static const struct gops_userd tu104_ops_userd = {
#ifdef CONFIG_NVGPU_USERD
	.setup_sw = nvgpu_userd_setup_sw,
	.cleanup_sw = nvgpu_userd_cleanup_sw,
	.init_mem = gk20a_userd_init_mem,
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	.gp_get = gv11b_userd_gp_get,
	.gp_put = gv11b_userd_gp_put,
	.pb_get = gv11b_userd_pb_get,
#endif
#endif /* CONFIG_NVGPU_USERD */
	.entry_size = gk20a_userd_entry_size,
};

static const struct gops_channel tu104_ops_channel = {
	.alloc_inst = nvgpu_channel_alloc_inst,
	.free_inst = nvgpu_channel_free_inst,
	.bind = gm20b_channel_bind,
	.unbind = gv11b_channel_unbind,
	.enable = gk20a_channel_enable,
	.disable = gk20a_channel_disable,
	.count = gv100_channel_count,
	.read_state = gv11b_channel_read_state,
	.force_ctx_reload = gm20b_channel_force_ctx_reload,
	.abort_clean_up = nvgpu_channel_abort_clean_up,
	.suspend_all_serviceable_ch = nvgpu_channel_suspend_all_serviceable_ch,
	.resume_all_serviceable_ch = nvgpu_channel_resume_all_serviceable_ch,
	.set_error_notifier = nvgpu_set_err_notifier_if_empty,
	.reset_faulted = gv11b_channel_reset_faulted,
};

static const struct gops_tsg tu104_ops_tsg = {
	.enable = gv11b_tsg_enable,
	.disable = nvgpu_tsg_disable,
	.init_eng_method_buffers = gv11b_tsg_init_eng_method_buffers,
	.deinit_eng_method_buffers = gv11b_tsg_deinit_eng_method_buffers,
	.bind_channel = NULL,
	.bind_channel_eng_method_buffers = gv11b_tsg_bind_channel_eng_method_buffers,
	.unbind_channel = NULL,
	.unbind_channel_check_hw_state = nvgpu_tsg_unbind_channel_check_hw_state,
	.unbind_channel_check_hw_next = gk20a_tsg_unbind_channel_check_hw_next,
	.unbind_channel_check_ctx_reload = nvgpu_tsg_unbind_channel_check_ctx_reload,
	.unbind_channel_check_eng_faulted = gv11b_tsg_unbind_channel_check_eng_faulted,
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	.check_ctxsw_timeout = nvgpu_tsg_check_ctxsw_timeout,
#endif
#ifdef CONFIG_NVGPU_CHANNEL_TSG_CONTROL
	.force_reset = nvgpu_tsg_force_reset_ch,
	.post_event_id = nvgpu_tsg_post_event_id,
#endif
#ifdef CONFIG_NVGPU_CHANNEL_TSG_SCHEDULING
	.set_timeslice = nvgpu_tsg_set_timeslice,
	.set_long_timeslice = nvgpu_tsg_set_long_timeslice,
#endif
	.default_timeslice_us = nvgpu_tsg_default_timeslice_us,
};

static const struct gops_usermode tu104_ops_usermode = {
	.setup_hw = tu104_usermode_setup_hw,
	.base = tu104_usermode_base,
	.bus_base = tu104_usermode_bus_base,
	.ring_doorbell = tu104_usermode_ring_doorbell,
	.doorbell_token = tu104_usermode_doorbell_token,
};

static const struct gops_netlist tu104_ops_netlist = {
	.get_netlist_name = tu104_netlist_get_name,
	.is_fw_defined = tu104_netlist_is_firmware_defined,
};

static const struct gops_mm_mmu_fault tu104_ops_mm_mmu_fault = {
	.setup_sw = gv11b_mm_mmu_fault_setup_sw,
	.setup_hw = gv11b_mm_mmu_fault_setup_hw,
	.info_mem_destroy = gv11b_mm_mmu_fault_info_mem_destroy,
	.disable_hw = gv11b_mm_mmu_fault_disable_hw,
	.parse_mmu_fault_info = gv11b_mm_mmu_fault_parse_mmu_fault_info,
};

static const struct gops_mm_cache tu104_ops_mm_cache = {
	.fb_flush = gk20a_mm_fb_flush,
	.l2_invalidate = gk20a_mm_l2_invalidate,
	.l2_flush = gv11b_mm_l2_flush,
#ifdef CONFIG_NVGPU_COMPRESSION
	.cbc_clean = gk20a_mm_cbc_clean,
#endif
};

static const struct gops_mm_gmmu tu104_ops_mm_gmmu = {
	.get_mmu_levels = gp10b_mm_get_mmu_levels,
	.get_max_page_table_levels = gp10b_get_max_page_table_levels,
	.map = nvgpu_gmmu_map_locked,
	.unmap = nvgpu_gmmu_unmap_locked,
	.get_big_page_sizes = gm20b_mm_get_big_page_sizes,
	.get_default_big_page_size = nvgpu_gmmu_default_big_page_size,
	/* iommu bit is not applicable for dgpu */
	.get_iommu_bit = NULL,
	.gpu_phys_addr = gv11b_gpu_phys_addr,
};

static const struct gops_mm tu104_ops_mm = {
	.init_mm_support = nvgpu_init_mm_support,
	.pd_cache_init = nvgpu_pd_cache_init,
	.mm_suspend = nvgpu_mm_suspend,
	.vm_bind_channel = nvgpu_vm_bind_channel,
	.setup_hw = nvgpu_mm_setup_hw,
	.is_bar1_supported = gv11b_mm_is_bar1_supported,
	.init_inst_block = gv11b_mm_init_inst_block,
	.init_inst_block_for_subctxs = gv11b_mm_init_inst_block_for_subctxs,
	.init_bar2_vm = gp10b_mm_init_bar2_vm,
	.remove_bar2_vm = gp10b_mm_remove_bar2_vm,
	.get_flush_retries = tu104_mm_get_flush_retries,
	.get_default_va_sizes = gp10b_mm_get_default_va_sizes,
	.bar1_map_userd = NULL,
};

static const struct gops_therm tu104_ops_therm = {
	.init_therm_support = nvgpu_init_therm_support,
	/* PROD values match with H/W INIT values */
	.init_elcg_mode = gv11b_therm_init_elcg_mode,
#ifdef CONFIG_NVGPU_NON_FUSA
	.init_blcg_mode = gm20b_therm_init_blcg_mode,
#endif
	.elcg_init_idle_filters = NULL,
#ifdef CONFIG_NVGPU_LS_PMU
	.get_internal_sensor_curr_temp =
		tu104_get_internal_sensor_curr_temp,
	.get_internal_sensor_limits = tu104_get_internal_sensor_limits,
#endif
};

#ifdef CONFIG_NVGPU_LS_PMU
static const struct gops_pmu tu104_ops_pmu = {
	.ecc_init = gv11b_pmu_ecc_init,
	.ecc_free = gv11b_pmu_ecc_free,
	/* Init */
	.pmu_early_init = nvgpu_pmu_early_init,
	.pmu_rtos_init = nvgpu_pmu_rtos_init,
	.pmu_pstate_sw_setup = nvgpu_pmu_pstate_sw_setup,
	.pmu_pstate_pmu_setup = nvgpu_pmu_pstate_pmu_setup,
	.pmu_destroy = nvgpu_pmu_destroy,
	.falcon_base_addr = tu104_pmu_falcon_base_addr,
	.pmu_queue_tail = gk20a_pmu_queue_tail,
	.pmu_get_queue_head = tu104_pmu_queue_head_r,
	.pmu_mutex_release = gk20a_pmu_mutex_release,
	.pmu_is_interrupted = gk20a_pmu_is_interrupted,
	.get_irqmask = gk20a_pmu_get_irqmask,
	.pmu_isr = gk20a_pmu_isr,
	.pmu_init_perfmon_counter = gk20a_pmu_init_perfmon_counter,
	.pmu_pg_idle_counter_config = gk20a_pmu_pg_idle_counter_config,
	.pmu_read_idle_counter = gk20a_pmu_read_idle_counter,
	.pmu_reset_idle_counter = gk20a_pmu_reset_idle_counter,
	/* TODO: implement for tu104 */
	.pmu_read_idle_intr_status = NULL,
	.pmu_clear_idle_intr_status = NULL,
	.pmu_dump_elpg_stats = gk20a_pmu_dump_elpg_stats,
	.pmu_dump_falcon_stats = gk20a_pmu_dump_falcon_stats,
	.pmu_enable_irq = gv11b_pmu_enable_irq,
	.is_pmu_supported = tu104_is_pmu_supported,
	.pmu_mutex_owner = gk20a_pmu_mutex_owner,
	.pmu_mutex_acquire = gk20a_pmu_mutex_acquire,
	.pmu_msgq_tail = gk20a_pmu_msgq_tail,
	.pmu_get_queue_head_size = tu104_pmu_queue_head__size_1_v,
	.pmu_reset = nvgpu_pmu_reset,
	.pmu_queue_head = gk20a_pmu_queue_head,
	.pmu_get_queue_tail_size = tu104_pmu_queue_tail__size_1_v,
	.reset_engine = gv11b_pmu_engine_reset,
	.write_dmatrfbase = gp10b_write_dmatrfbase,
	.pmu_mutex_size = tu104_pmu_mutex__size_1_v,
	.is_engine_in_reset = gv11b_pmu_is_engine_in_reset,
	.pmu_get_queue_tail = tu104_pmu_queue_tail_r,
	.get_irqdest = gk20a_pmu_get_irqdest,
	.handle_ext_irq = gv11b_pmu_handle_ext_irq,
	.handle_swgen1_irq = NULL,
	.is_debug_mode_enabled = gm20b_pmu_is_debug_mode_en,
	.setup_apertures = tu104_pmu_setup_apertures,
	.secured_pmu_start = gm20b_secured_pmu_start,
	.pmu_clear_bar0_host_err_status = gm20b_clear_pmu_bar0_host_err_status,
};
#endif

#ifdef CONFIG_NVGPU_LS_PMU
static const struct gops_clk tu104_ops_clk = {
#ifdef CONFIG_NVGPU_CLK_ARB
	.init_clk_support = tu104_init_clk_support,
	.get_crystal_clk_hz = tu104_crystal_clk_hz,
	.get_rate_cntr = tu104_get_rate_cntr,
	.measure_freq = tu104_clk_measure_freq,
	.suspend_clk_support = tu104_suspend_clk_support,
	.clk_domain_get_f_points = tu104_clk_domain_get_f_points,
	.get_maxrate = tu104_clk_maxrate,
	.get_change_seq_time = tu104_get_change_seq_time,
	.get_cntr_xbarclk_source = tu104_clk_get_cntr_xbarclk_source,
	.get_cntr_sysclk_source = tu104_clk_get_cntr_sysclk_source,
#endif
	.change_host_clk_source = tu104_change_host_clk_source,
	.perf_pmu_vfe_load = nvgpu_pmu_perf_load,
	.clk_mon_init_domains = nvgpu_pmu_clk_mon_init_domains,
};
#endif

#ifdef CONFIG_NVGPU_LS_PMU
static const struct gops_clk_mon tu104_ops_clk_mon = {
	.clk_mon_alloc_memory = nvgpu_clk_mon_alloc_memory,
	.clk_mon_check_master_fault_status = tu104_clk_mon_check_master_fault_status,
	.clk_mon_check_status = tu104_clk_mon_check_status,
	.clk_mon_check_clk_good = tu104_clk_mon_check_clk_good,
	.clk_mon_check_pll_lock = tu104_clk_mon_check_pll_lock,
};
#endif

#ifdef CONFIG_NVGPU_CLK_ARB
static const struct gops_clk_arb tu104_ops_clk_arb = {
	.clk_arb_init_arbiter = nvgpu_clk_arb_init_arbiter,
	.check_clk_arb_support = gv100_check_clk_arb_support,
	.get_arbiter_clk_domains = gv100_get_arbiter_clk_domains,
	.get_arbiter_f_points = gv100_get_arbiter_f_points,
	.get_arbiter_clk_range = gv100_get_arbiter_clk_range,
	.get_arbiter_clk_default = gv100_get_arbiter_clk_default,
	.get_current_pstate = nvgpu_clk_arb_get_current_pstate,
	.arbiter_clk_init = gv100_init_clk_arbiter,
	.clk_arb_run_arbiter_cb = gv100_clk_arb_run_arbiter_cb,
	.clk_arb_cleanup = gv100_clk_arb_cleanup,
	.stop_clk_arb_threads = gv100_stop_clk_arb_threads,
};
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
static const struct gops_regops tu104_ops_regops = {
	.exec_regops = exec_regops_gk20a,
	.get_global_whitelist_ranges = tu104_get_global_whitelist_ranges,
	.get_global_whitelist_ranges_count = tu104_get_global_whitelist_ranges_count,
	.get_context_whitelist_ranges = tu104_get_context_whitelist_ranges,
	.get_context_whitelist_ranges_count = tu104_get_context_whitelist_ranges_count,
	.get_runcontrol_whitelist = tu104_get_runcontrol_whitelist,
	.get_runcontrol_whitelist_count = tu104_get_runcontrol_whitelist_count,
	.get_hwpm_perfmon_register_stride = tu104_get_hwpm_perfmon_register_stride,
	.get_hwpm_router_register_stride = tu104_get_hwpm_router_register_stride,
	.get_hwpm_pma_channel_register_stride = tu104_get_hwpm_pma_channel_register_stride,
	.get_hwpm_pma_trigger_register_stride = tu104_get_hwpm_pma_trigger_register_stride,
	.get_smpc_register_stride = tu104_get_smpc_register_stride,
	.get_cau_register_stride = tu104_get_cau_register_stride,
	.get_hwpm_perfmon_register_offset_allowlist =
		tu104_get_hwpm_perfmon_register_offset_allowlist,
	.get_hwpm_router_register_offset_allowlist =
		tu104_get_hwpm_router_register_offset_allowlist,
	.get_hwpm_pma_channel_register_offset_allowlist =
		tu104_get_hwpm_pma_channel_register_offset_allowlist,
	.get_hwpm_pma_trigger_register_offset_allowlist =
		tu104_get_hwpm_pma_trigger_register_offset_allowlist,
	.get_smpc_register_offset_allowlist = tu104_get_smpc_register_offset_allowlist,
	.get_cau_register_offset_allowlist = tu104_get_cau_register_offset_allowlist,
	.get_hwpm_perfmon_register_ranges = tu104_get_hwpm_perfmon_register_ranges,
	.get_hwpm_router_register_ranges = tu104_get_hwpm_router_register_ranges,
	.get_hwpm_pma_channel_register_ranges = tu104_get_hwpm_pma_channel_register_ranges,
	.get_hwpm_pma_trigger_register_ranges = tu104_get_hwpm_pma_trigger_register_ranges,
	.get_hwpm_pc_sampler_register_ranges = tu104_get_hwpm_pc_sampler_register_ranges,
	.get_smpc_register_ranges = tu104_get_smpc_register_ranges,
	.get_cau_register_ranges = tu104_get_cau_register_ranges,
	.get_hwpm_perfmux_register_ranges = tu104_get_hwpm_perfmux_register_ranges,
};
#endif

#ifdef CONFIG_NVGPU_LS_PMU
static const struct gops_mc tu104_ops_mc = {
	.get_chip_details = gm20b_get_chip_details,
	.intr_mask = intr_tu104_mask,
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	.intr_enable = NULL,
#endif
	.intr_stall_unit_config = intr_tu104_stall_unit_config,
	.intr_nonstall_unit_config = intr_tu104_nonstall_unit_config,
	.isr_stall = mc_tu104_isr_stall,
	.intr_stall = intr_tu104_stall,
	.intr_stall_pause = intr_tu104_stall_pause,
	.intr_stall_resume = intr_tu104_stall_resume,
	.intr_nonstall = intr_tu104_nonstall,
	.intr_nonstall_pause = intr_tu104_nonstall_pause,
	.intr_nonstall_resume = intr_tu104_nonstall_resume,
	.isr_nonstall = intr_tu104_isr_nonstall,
	.is_intr1_pending = NULL,
#ifdef CONFIG_NVGPU_NON_FUSA
	.log_pending_intrs = intr_tu104_log_pending_intrs,
#endif
	.is_intr_hub_pending = intr_tu104_is_intr_hub_pending,
	.is_intr_nvlink_pending = gv100_mc_is_intr_nvlink_pending,
	.is_stall_and_eng_intr_pending = gv100_mc_is_stall_and_eng_intr_pending,
	.fbpa_isr = mc_tu104_fbpa_isr,
	.enable_units = gv100_mc_enable_units,
	.enable_dev = gm20b_mc_enable_dev,
	.enable_devtype = gm20b_mc_enable_devtype,
#ifdef CONFIG_NVGPU_LS_PMU
	.is_enabled = gm20b_mc_is_enabled,
#endif
	.fb_reset = NULL,
	.ltc_isr = mc_tu104_ltc_isr,
	.is_mmu_fault_pending = gv11b_mc_is_mmu_fault_pending,
};
#endif

#ifdef CONFIG_NVGPU_LS_PMU
static const struct gops_debug tu104_ops_debug = {
	.show_dump = gk20a_debug_show_dump,
};
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
static const struct gops_debugger tu104_ops_debugger = {
	.post_events = nvgpu_dbg_gpu_post_events,
	.dbg_set_powergate = nvgpu_dbg_set_powergate,
};
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
static const struct gops_perf tu104_ops_perf = {
	.enable_membuf = gv11b_perf_enable_membuf,
	.disable_membuf = gv11b_perf_disable_membuf,
	.bind_mem_bytes_buffer_addr = gv11b_perf_bind_mem_bytes_buffer_addr,
	.init_inst_block = gv11b_perf_init_inst_block,
	.deinit_inst_block = gv11b_perf_deinit_inst_block,
	.membuf_reset_streaming = gv11b_perf_membuf_reset_streaming,
	.get_membuf_pending_bytes = gv11b_perf_get_membuf_pending_bytes,
	.set_membuf_handled_bytes = gv11b_perf_set_membuf_handled_bytes,
	.get_membuf_overflow_status = gv11b_perf_get_membuf_overflow_status,
	.get_pmmsys_per_chiplet_offset = gv11b_perf_get_pmmsys_per_chiplet_offset,
	.get_pmmgpc_per_chiplet_offset = gv11b_perf_get_pmmgpc_per_chiplet_offset,
	.get_pmmfbp_per_chiplet_offset = gv11b_perf_get_pmmfbp_per_chiplet_offset,
	.update_get_put = gv11b_perf_update_get_put,
	.get_hwpm_sys_perfmon_regs = tu104_perf_get_hwpm_sys_perfmon_regs,
	.get_hwpm_gpc_perfmon_regs = tu104_perf_get_hwpm_gpc_perfmon_regs,
	.get_hwpm_fbp_perfmon_regs = tu104_perf_get_hwpm_fbp_perfmon_regs,
	.set_pmm_register = gv11b_perf_set_pmm_register,
	.get_num_hwpm_perfmon = gv11b_perf_get_num_hwpm_perfmon,
	.init_hwpm_pmm_register = gv11b_perf_init_hwpm_pmm_register,
	.reset_hwpm_pmm_registers = gv11b_perf_reset_hwpm_pmm_registers,
	.pma_stream_enable = gv11b_perf_pma_stream_enable,
	.disable_all_perfmons = gv11b_perf_disable_all_perfmons,
	.wait_for_idle_pmm_routers = gv11b_perf_wait_for_idle_pmm_routers,
	.wait_for_idle_pma = gv11b_perf_wait_for_idle_pma,
};
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
static const struct gops_perfbuf tu104_ops_perfbuf = {
	.perfbuf_enable = nvgpu_perfbuf_enable_locked,
	.perfbuf_disable = nvgpu_perfbuf_disable_locked,
	.init_inst_block = nvgpu_perfbuf_init_inst_block,
	.deinit_inst_block = nvgpu_perfbuf_deinit_inst_block,
	.update_get_put = nvgpu_perfbuf_update_get_put,
};
#endif

#ifdef CONFIG_NVGPU_PROFILER
static const struct gops_pm_reservation tu104_ops_pm_reservation = {
	.acquire = nvgpu_pm_reservation_acquire,
	.release = nvgpu_pm_reservation_release,
	.release_all_per_vmid = nvgpu_pm_reservation_release_all_per_vmid,
};
#endif

#ifdef CONFIG_NVGPU_PROFILER
static const struct gops_profiler tu104_ops_profiler = {
	.bind_hwpm = nvgpu_profiler_bind_hwpm,
	.unbind_hwpm = nvgpu_profiler_unbind_hwpm,
	.bind_hwpm_streamout = nvgpu_profiler_bind_hwpm_streamout,
	.unbind_hwpm_streamout = nvgpu_profiler_unbind_hwpm_streamout,
	.bind_smpc = nvgpu_profiler_bind_smpc,
	.unbind_smpc = nvgpu_profiler_unbind_smpc,
};
#endif

#ifdef CONFIG_NVGPU_LS_PMU
static const struct gops_bus tu104_ops_bus = {
	.init_hw = tu104_bus_init_hw,
	.isr = gk20a_bus_isr,
	.bar1_bind = NULL,
	.bar2_bind = bus_tu104_bar2_bind,
	.configure_debug_bus = gv11b_bus_configure_debug_bus,
#ifdef CONFIG_NVGPU_DGPU
	.set_bar0_window = gk20a_bus_set_bar0_window,
	.read_sw_scratch = gv100_bus_read_sw_scratch,
	.write_sw_scratch = gv100_bus_write_sw_scratch,
#endif
};
#endif

#ifdef CONFIG_NVGPU_LS_PMU
static const struct gops_ptimer tu104_ops_ptimer = {
	.isr = gk20a_ptimer_isr,
#ifdef CONFIG_NVGPU_IOCTL_NON_FUSA
	.read_ptimer = gk20a_read_ptimer,
	.get_timestamps_zipper = nvgpu_get_timestamps_zipper,
#endif
#ifdef CONFIG_NVGPU_DEBUGGER
	.config_gr_tick_freq = gp10b_ptimer_config_gr_tick_freq,
#endif
#ifdef CONFIG_NVGPU_PROFILER
	.get_timer_reg_offsets = gv11b_ptimer_get_timer_reg_offsets,
#endif
};
#endif

#if defined(CONFIG_NVGPU_CYCLESTATS)
static const struct gops_css tu104_ops_css = {
	.enable_snapshot = nvgpu_css_enable_snapshot,
	.disable_snapshot = nvgpu_css_disable_snapshot,
	.check_data_available = nvgpu_css_check_data_available,
	.set_handled_snapshots = nvgpu_css_set_handled_snapshots,
	.allocate_perfmon_ids = nvgpu_css_allocate_perfmon_ids,
	.release_perfmon_ids = nvgpu_css_release_perfmon_ids,
	.get_overflow_status = nvgpu_css_get_overflow_status,
	.get_pending_snapshots = nvgpu_css_get_pending_snapshots,
	.get_max_buffer_size = nvgpu_css_get_max_buffer_size,
};
#endif

#ifdef CONFIG_NVGPU_LS_PMU
static const struct gops_xve tu104_ops_xve = {
	.get_speed = xve_get_speed_gp106,
	.xve_readl = xve_xve_readl_gp106,
	.xve_writel = xve_xve_writel_gp106,
	.disable_aspm = xve_disable_aspm_gp106,
	.reset_gpu = xve_reset_gpu_gp106,
#if defined(CONFIG_PCI_MSI)
	.rearm_msi = xve_rearm_msi_gp106,
#endif
	.enable_shadow_rom = NULL,
	.disable_shadow_rom = NULL,
	.devinit_deferred_settings = tu104_devinit_deferred_settings,
};
#endif

#ifdef CONFIG_NVGPU_LS_PMU
static const struct gops_falcon tu104_ops_falcon = {
	.falcon_sw_init = nvgpu_falcon_sw_init,
	.falcon_sw_free = nvgpu_falcon_sw_free,
	.reset = gk20a_falcon_reset,
	.is_falcon_cpu_halted = gk20a_is_falcon_cpu_halted,
	.is_falcon_idle = gk20a_is_falcon_idle,
	.is_falcon_scrubbing_done = gk20a_is_falcon_scrubbing_done,
	.get_mem_size = gk20a_falcon_get_mem_size,
	.get_ports_count = gk20a_falcon_get_ports_count,
	.copy_to_dmem = gk20a_falcon_copy_to_dmem,
	.copy_to_imem = gk20a_falcon_copy_to_imem,
	.dmemc_blk_mask = gk20a_falcon_dmemc_blk_mask,
	.imemc_blk_field = gk20a_falcon_imemc_blk_field,
	.bootstrap = gk20a_falcon_bootstrap,
	.mailbox_read = gk20a_falcon_mailbox_read,
	.mailbox_write = gk20a_falcon_mailbox_write,
	.set_irq = gk20a_falcon_set_irq,
#ifdef CONFIG_NVGPU_FALCON_DEBUG
	.dump_falcon_stats = gk20a_falcon_dump_stats,
#endif
	.clear_halt_interrupt_status = gk20a_falcon_clear_halt_interrupt_status,
#ifdef CONFIG_NVGPU_FALCON_NON_FUSA
	.copy_from_dmem = gk20a_falcon_copy_from_dmem,
	.copy_from_imem = gk20a_falcon_copy_from_imem,
	.get_falcon_ctls = gk20a_falcon_get_ctls,
#endif
};
#endif

#ifdef CONFIG_NVGPU_LS_PMU
static const struct gops_priv_ring tu104_ops_priv_ring = {
	.enable_priv_ring = gm20b_priv_ring_enable,
	.isr = gp10b_priv_ring_isr,
	.isr_handle_0 = gp10b_priv_ring_isr_handle_0,
	.isr_handle_1 = gp10b_priv_ring_isr_handle_1,
	.decode_error_code = gp10b_priv_ring_decode_error_code,
	.set_ppriv_timeout_settings = NULL,
	.enum_ltc = gm20b_priv_ring_enum_ltc,
	.get_gpc_count = gm20b_priv_ring_get_gpc_count,
	.get_fbp_count = gm20b_priv_ring_get_fbp_count,
#ifdef CONFIG_NVGPU_PROFILER
	.read_pri_fence = gv11b_priv_ring_read_pri_fence,
#endif
};
#endif

#ifdef CONFIG_NVGPU_LS_PMU
static const struct gops_fuse tu104_ops_fuse = {
	.is_opt_ecc_enable = gp10b_fuse_is_opt_ecc_enable,
	.is_opt_feature_override_disable = gp10b_fuse_is_opt_feature_override_disable,
	.fuse_status_opt_fbio = gm20b_fuse_status_opt_fbio,
	.fuse_status_opt_fbp = gm20b_fuse_status_opt_fbp,
	.fuse_status_opt_l2_fbp = gm20b_fuse_status_opt_l2_fbp,
	.fuse_status_opt_gpc = gm20b_fuse_status_opt_gpc,
	.fuse_status_opt_pes_gpc = gv11b_fuse_status_opt_pes_gpc,
	.fuse_status_opt_tpc_gpc = gm20b_fuse_status_opt_tpc_gpc,
	.fuse_ctrl_opt_tpc_gpc = gm20b_fuse_ctrl_opt_tpc_gpc,
	/* Turing is multi-GPC config, static GPC PG to be taken care later */
	.fuse_ctrl_opt_fbp = NULL,
	.fuse_ctrl_opt_gpc = NULL,
	.fuse_opt_sec_debug_en = gm20b_fuse_opt_sec_debug_en,
	.fuse_opt_priv_sec_en = gm20b_fuse_opt_priv_sec_en,
	.read_vin_cal_fuse_rev = gp106_fuse_read_vin_cal_fuse_rev,
	.read_vin_cal_slope_intercept_fuse = gp106_fuse_read_vin_cal_slope_intercept_fuse,
	.read_vin_cal_gain_offset_fuse = gp106_fuse_read_vin_cal_gain_offset_fuse,
	.read_per_device_identifier = tu104_fuse_read_per_device_identifier,
};
#endif

#if defined(CONFIG_NVGPU_NVLINK)
static const struct gops_nvlink_link_mode_transitions tu104_ops_nvlink_link_mode_transitions = {
	.setup_pll = tu104_nvlink_setup_pll,
	.data_ready_en = tu104_nvlink_data_ready_en,
	.get_link_state = gv100_nvlink_get_link_state,
	.get_link_mode = gv100_nvlink_get_link_mode,
	.set_link_mode = gv100_nvlink_set_link_mode,
	.get_tx_sublink_state = tu104_nvlink_link_get_tx_sublink_state,
	.get_rx_sublink_state = tu104_nvlink_link_get_rx_sublink_state,
	.get_sublink_mode = gv100_nvlink_link_get_sublink_mode,
	.set_sublink_mode = gv100_nvlink_link_set_sublink_mode,
};
#endif

#if defined(CONFIG_NVGPU_NVLINK)
static const struct gops_nvlink_minion tu104_ops_nvlink_minion = {
	.base_addr = gv100_nvlink_minion_base_addr,
	.is_running = gv100_nvlink_minion_is_running,
	.is_boot_complete = gv100_nvlink_minion_is_boot_complete,
	.get_dlcmd_ordinal = tu104_nvlink_minion_get_dlcmd_ordinal,
	.send_dlcmd = gv100_nvlink_minion_send_dlcmd,
	.clear_intr = gv100_nvlink_minion_clear_intr,
	.init_intr = gv100_nvlink_minion_init_intr,
	.enable_link_intr = gv100_nvlink_minion_enable_link_intr,
	.falcon_isr = gv100_nvlink_minion_falcon_isr,
	.isr = gv100_nvlink_minion_isr,
	.is_debug_mode = tu104_nvlink_minion_is_debug_mode,
};
#endif

#if defined(CONFIG_NVGPU_NVLINK)
static const struct gops_nvlink_intr tu104_ops_nvlink_intr = {
	.init_link_err_intr = tu104_nvlink_init_link_err_intr,
	.enable_link_err_intr = tu104_nvlink_enable_link_err_intr,
	.isr = tu104_nvlink_isr,
};
#endif

#if defined(CONFIG_NVGPU_NVLINK)
static const struct gops_nvlink tu104_ops_nvlink = {
	.init = nvgpu_nvlink_init,
	.get_link_reset_mask = gv100_nvlink_get_link_reset_mask,
	.discover_link = gv100_nvlink_discover_link,
	.rxdet = tu104_nvlink_rxdet,
	.get_connected_link_mask = tu104_nvlink_get_connected_link_mask,
	.set_sw_errata = NULL,
	.configure_ac_coupling = gv100_nvlink_configure_ac_coupling,
	.prog_alt_clk = gv100_nvlink_prog_alt_clk,
	.clear_link_reset = gv100_nvlink_clear_link_reset,
	.enable_link_an0 = gv100_nvlink_enable_link_an0,
	/* API */
	.reg_init = gv100_nvlink_reg_init,
};
#endif

#ifdef CONFIG_NVGPU_DGPU
static const struct gops_sec2 tu104_ops_sec2 = {
	.init_sec2_setup_sw = nvgpu_init_sec2_setup_sw,
	.init_sec2_support = nvgpu_init_sec2_support,
	.sec2_destroy = nvgpu_sec2_destroy,
	.secured_sec2_start = tu104_start_sec2_secure,
	.enable_irq = tu104_sec2_enable_irq,
	.is_interrupted = tu104_sec2_is_interrupted,
	.get_intr = tu104_sec2_get_intr,
	.msg_intr_received = tu104_sec2_msg_intr_received,
	.set_msg_intr = tu104_sec2_set_msg_intr,
	.clr_intr = tu104_sec2_clr_intr,
	.process_intr = tu104_sec2_process_intr,
	.msgq_tail = tu104_sec2_msgq_tail,
	.falcon_base_addr = tu104_sec2_falcon_base_addr,
	.sec2_reset = tu104_sec2_reset,
	.sec2_copy_to_emem = tu104_sec2_flcn_copy_to_emem,
	.sec2_copy_from_emem = tu104_sec2_flcn_copy_from_emem,
	.sec2_queue_head = tu104_sec2_queue_head,
	.sec2_queue_tail = tu104_sec2_queue_tail,
	.flcn_setup_boot_config = tu104_sec2_flcn_setup_boot_config,
};
#endif

#ifdef CONFIG_NVGPU_LS_PMU
static const struct gops_gsp tu104_ops_gsp = {
	.falcon_base_addr = tu104_gsp_falcon_base_addr,
	.falcon_setup_boot_config = tu104_gsp_flcn_setup_boot_config,
	.gsp_reset = tu104_gsp_reset,
};
#endif

#ifdef CONFIG_NVGPU_LS_PMU
static const struct gops_top tu104_ops_top = {
	.parse_next_device = gm20b_top_parse_next_dev,
	.device_info_parse_enum = gm20b_device_info_parse_enum,
	.device_info_parse_data = gv11b_device_info_parse_data,
	.get_max_gpc_count = gm20b_top_get_max_gpc_count,
	.get_max_tpc_per_gpc_count = gm20b_top_get_max_tpc_per_gpc_count,
	.get_max_fbps_count = gm20b_top_get_max_fbps_count,
	.get_max_fbpas_count = gv100_top_get_max_fbpas_count,
	.get_max_ltc_per_fbp = gm20b_top_get_max_ltc_per_fbp,
	.get_max_lts_per_ltc = gm20b_top_get_max_lts_per_ltc,
	.get_num_ltcs = gm20b_top_get_num_ltcs,
	.get_num_lce = gv11b_top_get_num_lce,
	.get_max_pes_per_gpc = gv11b_top_get_max_pes_per_gpc,
};
#endif

#ifdef CONFIG_NVGPU_LS_PMU
static const struct gops_grmgr tu104_ops_grmgr = {
	.init_gr_manager = nvgpu_init_gr_manager,
};
#endif

static const struct gops_cic_mon tu104_ops_cic_mon = {
	.init = NULL,
	.report_err = NULL,
};

int tu104_init_hal(struct gk20a *g)
{
	struct gpu_ops *gops = &g->ops;

	gops->acr = tu104_ops_acr;
	gops->sbr = tu104_ops_sbr;
	gops->func = tu104_ops_func;
	gops->bios = tu104_ops_bios;
	gops->ecc = tu104_ops_ecc;
	gops->ltc = tu104_ops_ltc;
	gops->ltc.intr = tu104_ops_ltc_intr;
#ifdef CONFIG_NVGPU_COMPRESSION
	gops->cbc = tu104_ops_cbc;
#endif
	gops->ce = tu104_ops_ce;
	gops->gr = tu104_ops_gr;
	gops->gr.ecc = tu104_ops_gr_ecc;
	gops->gr.ctxsw_prog = tu104_ops_gr_ctxsw_prog;
	gops->gr.config = tu104_ops_gr_config;
#ifdef CONFIG_NVGPU_FECS_TRACE
	gops->gr.fecs_trace = tu104_ops_gr_fecs_trace;
#endif /* CONFIG_NVGPU_FECS_TRACE */
	gops->gr.setup = tu104_ops_gr_setup;
#ifdef CONFIG_NVGPU_GRAPHICS
	gops->gr.zbc = tu104_ops_gr_zbc;
	gops->gr.zcull = tu104_ops_gr_zcull;
#endif /* CONFIG_NVGPU_GRAPHICS */
#ifdef CONFIG_NVGPU_DEBUGGER
	gops->gr.hwpm_map = tu104_ops_gr_hwpm_map;
#endif
	gops->gr.init = tu104_ops_gr_init;
	gops->gr.intr = tu104_ops_gr_intr;
	gops->gr.falcon = tu104_ops_gr_falcon;
	gops->gpu_class = tu104_ops_gpu_class;
	gops->fb = tu104_ops_fb;
	gops->fb.ecc = tu104_ops_fb_ecc;
	gops->fb.intr = tu104_ops_fb_intr;
	gops->nvdec = tu104_ops_nvdec;
	gops->cg = tu104_ops_cg;
	gops->fifo = tu104_ops_fifo;
	gops->engine = tu104_ops_engine;
	gops->pbdma = tu104_ops_pbdma;
	gops->sync = tu104_ops_sync;
#ifdef CONFIG_TEGRA_GK20A_NVHOST
	gops->sync.syncpt = tu104_ops_sync_syncpt;
#endif /* CONFIG_TEGRA_GK20A_NVHOST */
#if defined(CONFIG_NVGPU_KERNEL_MODE_SUBMIT) && \
	defined(CONFIG_NVGPU_SW_SEMAPHORE)
	gops->sync.sema = tu104_ops_sync_sema;
#endif
	gops->engine_status = tu104_ops_engine_status;
	gops->pbdma_status = tu104_ops_pbdma_status;
	gops->ramfc = tu104_ops_ramfc;
	gops->ramin = tu104_ops_ramin;
	gops->runlist = tu104_ops_runlist;
	gops->userd = tu104_ops_userd;
	gops->channel = tu104_ops_channel;
	gops->tsg = tu104_ops_tsg;
	gops->usermode = tu104_ops_usermode;
	gops->netlist = tu104_ops_netlist;
	gops->mm = tu104_ops_mm;
	gops->mm.mmu_fault = tu104_ops_mm_mmu_fault;
	gops->mm.cache = tu104_ops_mm_cache;
	gops->mm.gmmu = tu104_ops_mm_gmmu;
	gops->therm = tu104_ops_therm;
#ifdef CONFIG_NVGPU_LS_PMU
	gops->pmu = tu104_ops_pmu;
#endif
	gops->clk = tu104_ops_clk;
	gops->clk_mon = tu104_ops_clk_mon;
#ifdef CONFIG_NVGPU_CLK_ARB
	gops->clk_arb = tu104_ops_clk_arb;
#endif
#ifdef CONFIG_NVGPU_DEBUGGER
	gops->regops = tu104_ops_regops;
#endif
	gops->mc = tu104_ops_mc;
	gops->debug = tu104_ops_debug;
#ifdef CONFIG_NVGPU_DEBUGGER
	gops->debugger = tu104_ops_debugger;
	gops->perf = tu104_ops_perf;
	gops->perfbuf = tu104_ops_perfbuf;
#endif
#ifdef CONFIG_NVGPU_PROFILER
	gops->pm_reservation = tu104_ops_pm_reservation;
	gops->profiler = tu104_ops_profiler;
#endif
	gops->bus = tu104_ops_bus;
	gops->ptimer = tu104_ops_ptimer;
#if defined(CONFIG_NVGPU_CYCLESTATS)
	gops->css = tu104_ops_css;
#endif
	gops->xve = tu104_ops_xve;
	gops->falcon = tu104_ops_falcon;
	gops->priv_ring = tu104_ops_priv_ring;
	gops->fuse = tu104_ops_fuse;
#if defined(CONFIG_NVGPU_NVLINK)
	gops->nvlink = tu104_ops_nvlink;
	gops->nvlink.link_mode_transitions = tu104_ops_nvlink_link_mode_transitions;
	gops->nvlink.minion = tu104_ops_nvlink_minion;
	gops->nvlink.intr = tu104_ops_nvlink_intr;
#endif
#ifdef CONFIG_NVGPU_DGPU
	gops->sec2 = tu104_ops_sec2;
#endif
	gops->gsp = tu104_ops_gsp;
	gops->top = tu104_ops_top;
	gops->grmgr = tu104_ops_grmgr;
	gops->cic_mon = tu104_ops_cic_mon;
	gops->chip_init_gpu_characteristics = tu104_init_gpu_characteristics;
	gops->get_litter_value = tu104_get_litter_value;
	gops->semaphore_wakeup = nvgpu_channel_semaphore_wakeup;

	nvgpu_set_errata(g, NVGPU_ERRATA_INIT_PDB_CACHE, true);
	nvgpu_set_errata(g, NVGPU_ERRATA_FB_PDB_CACHE, true);
	nvgpu_set_errata(g, NVGPU_ERRATA_VBIOS_NVLINK_MASK, true);
	nvgpu_set_errata(g, NVGPU_ERRATA_200391931, true);
	nvgpu_set_errata(g, NVGPU_ERRATA_SYNCPT_INVALID_ID_0, true);
	nvgpu_set_errata(g, NVGPU_ERRATA_3524791, true);

	nvgpu_set_enabled(g, NVGPU_SEC_PRIVSECURITY, true);
	nvgpu_set_enabled(g, NVGPU_SEC_SECUREGPCCS, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_MULTIPLE_WPR, true);
#ifdef CONFIG_NVGPU_FECS_TRACE
	nvgpu_set_enabled(g, NVGPU_FECS_TRACE_VA, true);
	nvgpu_set_enabled(g, NVGPU_FECS_TRACE_FEATURE_CONTROL, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_FECS_CTXSW_TRACE, true);
#endif
#ifdef CONFIG_NVGPU_PROFILER
	nvgpu_set_enabled(g, NVGPU_SUPPORT_PROFILER_V2_DEVICE, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_PROFILER_V2_CONTEXT, false);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_SMPC_GLOBAL_MODE, true);
#endif
	nvgpu_set_enabled(g, NVGPU_SUPPORT_SEC2_RTOS, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_PMU_RTOS_FBQ, true);
#ifdef CONFIG_NVGPU_GRAPHICS
	nvgpu_set_enabled(g, NVGPU_SUPPORT_ZBC_STENCIL, true);
#endif
#ifdef CONFIG_NVGPU_GFXP
	nvgpu_set_enabled(g, NVGPU_SUPPORT_PREEMPTION_GFXP, true);
#endif
	nvgpu_set_enabled(g, NVGPU_SUPPORT_PLATFORM_ATOMIC, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_SEC2_VM, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_GSP_VM, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_PMU_SUPER_SURFACE, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_SET_CTX_MMU_DEBUG_MODE, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_DGPU_THERMAL_ALERT, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_DGPU_PCIE_SCRIPT_EXECUTE, true);
	nvgpu_set_enabled(g, NVGPU_FMON_SUPPORT_ENABLE, true);

	/*
	 * Tu104 has multiple async-LCE (3), GRCE (2) and PCE (4).
	 * The allocation used for the HW structures is deterministic.
	 * LCE/PCE is likely to follow the same resource allocation in primary
	 * and redundant execution mode if we use the same LCE/PCE pairs for
	 * both execution modes. All available LCEs and GRCEs should be mapped
	 * to unique PCEs.
	 *
	 * The recommendation is to swap the GRCEs with each other during
	 * redundant execution. The async-LCEs have their own PCEs,
	 * so the suggestion is to use a different async-LCE during redundant
	 * execution. This will allow us to claim very high coverage for
	 * permanent fault.
	 */
	nvgpu_set_enabled(g, NVGPU_SUPPORT_COPY_ENGINE_DIVERSITY, true);

#ifdef CONFIG_NVGPU_SM_DIVERSITY
	/*
	 * To achieve permanent fault coverage, the CTAs launched by each kernel
	 * in the mission and redundant contexts must execute on different
	 * hardware resources. This feature proposes modifications in the
	 * software to modify the virtual SM id to TPC mapping across the
	 * mission and redundant contexts.
	 *
	 * The virtual SM identifier to TPC mapping is done by the nvgpu
	 * when setting up the golden context. Once the table with this mapping
	 * is initialized, it is used by all subsequent contexts that are
	 * created. The proposal is for setting up the virtual SM identifier
	 * to TPC mapping on a per-context basis and initializing this
	 * virtual SM identifier to TPC mapping differently for the mission and
	 * redundant contexts.
	 *
	 * The recommendation for the redundant setting is to offset the
	 * assignment by 1 (TPC). This will ensure both GPC and TPC diversity.
	 * The SM and Quadrant diversity will happen naturally.
	 *
	 * For kernels with few CTAs, the diversity is guaranteed to be 100%.
	 * In case of completely random CTA allocation, e.g. large number of
	 * CTAs in the waiting queue, the diversity is 1 - 1/#SM,
	 * or 97.9% for TU104.
	 */
	nvgpu_set_enabled(g, NVGPU_SUPPORT_SM_DIVERSITY, true);
	g->max_sm_diversity_config_count =
		NVGPU_MAX_SM_DIVERSITY_CONFIG_COUNT;
#else
	g->max_sm_diversity_config_count =
		NVGPU_DEFAULT_SM_DIVERSITY_CONFIG_COUNT;
#endif
	/* for now */
	gops->clk.support_pmgr_domain = false;
	gops->clk.support_lpwr_pg = false;
	gops->clk.support_vf_point = true;
	gops->clk.lut_num_entries = CTRL_CLK_LUT_NUM_ENTRIES_GV10x;
#ifdef CONFIG_NVGPU_LS_PMU
	gops->clk.perf_pmu_vfe_load = nvgpu_pmu_perf_load;
#endif
#ifdef CONFIG_NVGPU_DGPU
	nvgpu_pramin_ops_init(g);
#endif

#ifdef CONFIG_NVGPU_COMPRESSION
	if (!nvgpu_is_enabled(g, NVGPU_IS_FMODEL)) {
		nvgpu_set_enabled(g, NVGPU_SUPPORT_COMPRESSION, true);
	}

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_COMPRESSION)) {
		gops->cbc.init = NULL;
		gops->cbc.ctrl = NULL;
		gops->cbc.alloc_comptags = NULL;
	}
#endif

	/* dGpu VDK support */
#ifdef CONFIG_NVGPU_SIM
	if (nvgpu_is_enabled(g, NVGPU_IS_FMODEL)) {
#ifdef CONFIG_NVGPU_GR_FALCON_NON_SECURE_BOOT
		gops->gr.falcon.load_ctxsw_ucode =
			nvgpu_gr_falcon_load_ctxsw_ucode;
#endif

		nvgpu_set_enabled(g, NVGPU_GR_USE_DMA_FOR_FW_BOOTSTRAP,
									false);
		/* Disable fb mem_unlock */
		gops->fb.mem_unlock = NULL;

		/* Disable clock support */
#ifdef CONFIG_NVGPU_CLK_ARB
		gops->clk_arb.get_arbiter_clk_domains = NULL;
		nvgpu_set_enabled(g, NVGPU_CLK_ARB_ENABLED, false);
#endif
	} else
#endif
	{
		nvgpu_set_enabled(g, NVGPU_GR_USE_DMA_FOR_FW_BOOTSTRAP, true);
	}
#ifdef CONFIG_NVGPU_CLK_ARB
	nvgpu_set_enabled(g, NVGPU_CLK_ARB_ENABLED, false);
#endif
	nvgpu_set_enabled(g, NVGPU_SUPPORT_PES_FS, true);
	g->name = "tu10x";

	return 0;
}
