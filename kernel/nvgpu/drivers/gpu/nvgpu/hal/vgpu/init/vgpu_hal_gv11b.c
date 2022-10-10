/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include "hal/bus/bus_gk20a.h"
#include "hal/bus/bus_gm20b.h"
#include "hal/mm/mm_gm20b.h"
#include "hal/mm/mm_gp10b.h"
#include "hal/mm/mm_gv11b.h"
#include "hal/mm/gmmu/gmmu_gk20a.h"
#include "hal/mm/gmmu/gmmu_gm20b.h"
#include "hal/mm/gmmu/gmmu_gp10b.h"
#include "hal/mm/mmu_fault/mmu_fault_gv11b.h"
#include "hal/regops/regops_gv11b.h"
#include "hal/class/class_gv11b.h"
#include "hal/fifo/fifo_gv11b.h"
#include "hal/fifo/preempt_gv11b.h"
#include "hal/fifo/engines_gp10b.h"
#include "hal/fifo/engines_gv11b.h"
#include "hal/fifo/pbdma_gm20b.h"
#include "hal/fifo/pbdma_gp10b.h"
#include "hal/fifo/pbdma_gv11b.h"
#include "hal/fifo/ramin_gk20a.h"
#include "hal/fifo/ramin_gm20b.h"
#include "hal/fifo/ramin_gv11b.h"
#include "hal/fifo/runlist_ram_gv11b.h"
#include "hal/fifo/runlist_fifo_gv11b.h"
#include "hal/fifo/tsg_gv11b.h"
#include "hal/fifo/userd_gk20a.h"
#include "hal/fifo/userd_gv11b.h"
#include "hal/fifo/usermode_gv11b.h"
#include "hal/fifo/fifo_intr_gv11b.h"
#include "hal/therm/therm_gm20b.h"
#include "hal/therm/therm_gp10b.h"
#include "hal/therm/therm_gv11b.h"
#include "hal/gr/fecs_trace/fecs_trace_gv11b.h"
#ifdef CONFIG_NVGPU_GRAPHICS
#include "hal/gr/zbc/zbc_gv11b.h"
#endif
#include "hal/gr/hwpm_map/hwpm_map_gv100.h"
#include "hal/gr/init/gr_init_gv11b.h"
#include "hal/ltc/ltc_gm20b.h"
#include "hal/ltc/ltc_gp10b.h"
#include "hal/ltc/ltc_gv11b.h"
#include "hal/fb/fb_gm20b.h"
#include "hal/fb/fb_gp10b.h"
#include "hal/fb/fb_gv11b.h"
#include "hal/fb/fb_mmu_fault_gv11b.h"
#include "hal/fb/intr/fb_intr_gv11b.h"
#include "hal/gr/init/gr_init_gm20b.h"
#include "hal/gr/init/gr_init_gp10b.h"
#include "hal/gr/init/gr_init_gv11b.h"
#include "hal/gr/intr/gr_intr_gm20b.h"
#include "hal/gr/intr/gr_intr_gv11b.h"
#include "hal/gr/ctxsw_prog/ctxsw_prog_gm20b.h"
#include "hal/gr/ctxsw_prog/ctxsw_prog_gp10b.h"
#include "hal/gr/ctxsw_prog/ctxsw_prog_gv11b.h"
#include "hal/gr/gr/gr_gk20a.h"
#include "hal/gr/gr/gr_gm20b.h"
#include "hal/gr/gr/gr_gp10b.h"
#include "hal/gr/gr/gr_gv11b.h"
#include "hal/gr/gr/gr_gv100.h"
#include "hal/perf/perf_gv11b.h"
#include "hal/netlist/netlist_gv11b.h"
#include "hal/sync/syncpt_cmdbuf_gv11b.h"
#include "hal/sync/sema_cmdbuf_gv11b.h"
#include "hal/init/hal_gv11b.h"
#include "hal/init/hal_gv11b_litter.h"
#include "hal/fifo/channel_gv11b.h"
#ifdef CONFIG_NVGPU_DEBUGGER
#include "hal/regops/regops_gv11b.h"
#include "hal/regops/allowlist_gv11b.h"
#endif
#include "hal/ptimer/ptimer_gv11b.h"

#include "hal/vgpu/fifo/fifo_gv11b_vgpu.h"
#include "hal/vgpu/sync/syncpt_cmdbuf_gv11b_vgpu.h"

#include "common/clk_arb/clk_arb_gp10b.h"

#include <nvgpu/gk20a.h>
#include <nvgpu/errata.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/gr_intr.h>
#include <nvgpu/vgpu/vgpu.h>
#include <nvgpu/error_notifier.h>
#include <nvgpu/acr.h>
#include <nvgpu/ce.h>
#include <nvgpu/pmu.h>
#include <nvgpu/runlist.h>
#include <nvgpu/nvhost.h>
#ifdef CONFIG_NVGPU_LS_PMU
#include <nvgpu/pmu/pmu_pstate.h>
#endif
#include <nvgpu/therm.h>
#include <nvgpu/clk_arb.h>
#include <nvgpu/grmgr.h>
#include <nvgpu/perfbuf.h>

#include "common/vgpu/init/init_vgpu.h"
#include "common/vgpu/fb/fb_vgpu.h"
#include "common/vgpu/top/top_vgpu.h"
#include "common/vgpu/fifo/fifo_vgpu.h"
#include "common/vgpu/fifo/channel_vgpu.h"
#include "common/vgpu/fifo/tsg_vgpu.h"
#include "common/vgpu/fifo/preempt_vgpu.h"
#include "common/vgpu/fifo/runlist_vgpu.h"
#include "common/vgpu/fifo/ramfc_vgpu.h"
#include "common/vgpu/fifo/userd_vgpu.h"
#include "common/vgpu/gr/gr_vgpu.h"
#include "common/vgpu/gr/ctx_vgpu.h"
#include "common/vgpu/gr/subctx_vgpu.h"
#include "common/vgpu/ltc/ltc_vgpu.h"
#include "common/vgpu/mm/mm_vgpu.h"
#include "common/vgpu/cbc/cbc_vgpu.h"
#include "common/vgpu/debugger_vgpu.h"
#include "common/vgpu/pm_reservation_vgpu.h"
#include "common/vgpu/perf/perf_vgpu.h"
#include "common/vgpu/gr/fecs_trace_vgpu.h"
#include "common/vgpu/perf/cyclestats_snapshot_vgpu.h"
#include "common/vgpu/ptimer/ptimer_vgpu.h"
#include "common/vgpu/profiler/profiler_vgpu.h"
#include "vgpu_hal_gv11b.h"

#include <nvgpu/debugger.h>
#include <nvgpu/enabled.h>
#include <nvgpu/channel.h>

#include <nvgpu/vgpu/ce_vgpu.h>
#include <nvgpu/vgpu/vm_vgpu.h>
#ifdef CONFIG_NVGPU_GRAPHICS
#include <nvgpu/gr/zbc.h>
#endif
#include <nvgpu/gr/setup.h>

#include <nvgpu/hw/gv11b/hw_pwr_gv11b.h>

static int vgpu_gv11b_init_gpu_characteristics(struct gk20a *g)
{
	int err;

	nvgpu_log_fn(g, " ");

	err = vgpu_init_gpu_characteristics(g);
	if (err != 0) {
		nvgpu_err(g, "failed to init GPU characteristics");
		return err;
	}

	nvgpu_set_enabled(g, NVGPU_SUPPORT_TSG_SUBCONTEXTS, true);
	nvgpu_set_enabled(g, NVGPU_USE_COHERENT_SYSMEM, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_IO_COHERENCE, true);
	if (nvgpu_has_syncpoints(g)) {
		nvgpu_set_enabled(g, NVGPU_SUPPORT_SYNCPOINT_ADDRESS, true);
		nvgpu_set_enabled(g, NVGPU_SUPPORT_USER_SYNCPOINT, true);
	}
	nvgpu_set_enabled(g, NVGPU_SUPPORT_USERMODE_SUBMIT, true);
#ifdef CONFIG_NVGPU_GRAPHICS
	nvgpu_set_enabled(g, NVGPU_SUPPORT_SCG, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_ZBC_STENCIL, true);
#endif
	nvgpu_set_enabled(g, NVGPU_SUPPORT_PREEMPTION_GFXP, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_PLATFORM_ATOMIC, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_SET_CTX_MMU_DEBUG_MODE, true);

	return 0;
}

static const struct gops_acr vgpu_gv11b_ops_acr = {
	.acr_init = nvgpu_acr_init,
	.acr_construct_execute = nvgpu_acr_construct_execute,
};

#ifdef CONFIG_NVGPU_DGPU
static const struct gops_bios vgpu_gv11b_ops_bios = {
	.bios_sw_init = nvgpu_bios_sw_init,
};
#endif

static const struct gops_ltc_intr vgpu_gv11b_ops_ltc_intr = {
	.configure = NULL,
	.isr = NULL,
#ifdef CONFIG_NVGPU_NON_FUSA
	.en_illegal_compstat = NULL,
#endif
};

static const struct gops_ltc vgpu_gv11b_ops_ltc = {
	.init_ltc_support = nvgpu_init_ltc_support,
	.ltc_remove_support = nvgpu_ltc_remove_support,
	.determine_L2_size_bytes = vgpu_determine_L2_size_bytes,
	.init_fs_state = vgpu_ltc_init_fs_state,
	.flush = NULL,
#if defined(CONFIG_NVGPU_NON_FUSA) || defined(CONFIG_NVGPU_KERNEL_MODE_SUBMIT)
	.set_enabled = NULL,
#endif
#ifdef CONFIG_NVGPU_GRAPHICS
	.set_zbc_s_entry = NULL,
	.set_zbc_color_entry = NULL,
	.set_zbc_depth_entry = NULL,
#endif
#ifdef CONFIG_NVGPU_DEBUGGER
	.pri_is_ltc_addr = gm20b_ltc_pri_is_ltc_addr,
	.is_ltcs_ltss_addr = gm20b_ltc_is_ltcs_ltss_addr,
	.is_ltcn_ltss_addr = gm20b_ltc_is_ltcn_ltss_addr,
	.split_lts_broadcast_addr = gm20b_ltc_split_lts_broadcast_addr,
	.split_ltc_broadcast_addr = gm20b_ltc_split_ltc_broadcast_addr,
#endif /* CONFIG_NVGPU_DEBUGGER */
};

#ifdef CONFIG_NVGPU_COMPRESSION
static const struct gops_cbc vgpu_gv11b_ops_cbc = {
	.cbc_init_support = nvgpu_cbc_init_support,
	.cbc_remove_support = nvgpu_cbc_remove_support,
	.init = NULL,
	.ctrl = NULL,
	.alloc_comptags = vgpu_cbc_alloc_comptags,
};
#endif

static const struct gops_ce vgpu_gv11b_ops_ce = {
	.ce_init_support = nvgpu_ce_init_support,
#ifdef CONFIG_NVGPU_DGPU
	.ce_app_init_support = NULL,
	.ce_app_suspend = NULL,
	.ce_app_destroy = NULL,
#endif
	.isr_stall = NULL,
#ifdef CONFIG_NVGPU_NONSTALL_INTR
	.isr_nonstall = NULL,
#endif
	.get_num_pce = vgpu_ce_get_num_pce,
};

static const struct gops_gr_ctxsw_prog vgpu_gv11b_ops_gr_ctxsw_prog = {
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
#ifdef CONFIG_NVGPU_GRAPHICS
	.set_zcull_ptr = gv11b_ctxsw_prog_set_zcull_ptr,
	.set_zcull = gm20b_ctxsw_prog_set_zcull,
	.set_zcull_mode_no_ctxsw = gm20b_ctxsw_prog_set_zcull_mode_no_ctxsw,
	.is_zcull_mode_separate_buffer = gm20b_ctxsw_prog_is_zcull_mode_separate_buffer,
	.set_graphics_preemption_mode_gfxp = gp10b_ctxsw_prog_set_graphics_preemption_mode_gfxp,
	.set_pmu_options_boost_clock_frequencies = NULL,
	.set_full_preemption_ptr = gv11b_ctxsw_prog_set_full_preemption_ptr,
	.set_full_preemption_ptr_veid0 = gv11b_ctxsw_prog_set_full_preemption_ptr_veid0,
#endif /* CONFIG_NVGPU_GRAPHICS */
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
	.set_pc_sampling = gm20b_ctxsw_prog_set_pc_sampling,
	.check_main_image_header_magic = gm20b_ctxsw_prog_check_main_image_header_magic,
	.check_local_header_magic = gm20b_ctxsw_prog_check_local_header_magic,
	.get_num_gpcs = gm20b_ctxsw_prog_get_num_gpcs,
	.get_num_tpcs = gm20b_ctxsw_prog_get_num_tpcs,
	.get_extended_buffer_size_offset = gm20b_ctxsw_prog_get_extended_buffer_size_offset,
	.get_ppc_info = gm20b_ctxsw_prog_get_ppc_info,
	.get_local_priv_register_ctl_offset = gm20b_ctxsw_prog_get_local_priv_register_ctl_offset,
	.hw_get_pm_gpc_gnic_stride = gm20b_ctxsw_prog_hw_get_pm_gpc_gnic_stride,
#endif /* CONFIG_NVGPU_DEBUGGER */
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
	.hw_get_perf_counter_register_stride = gv11b_ctxsw_prog_hw_get_perf_counter_register_stride,
	.set_context_buffer_ptr = gv11b_ctxsw_prog_set_context_buffer_ptr,
	.set_type_per_veid_header = gv11b_ctxsw_prog_set_type_per_veid_header,
#ifdef CONFIG_DEBUG_FS
	.dump_ctxsw_stats = NULL,
#endif
};

static const struct gops_gr_config vgpu_gv11b_ops_gr_config = {
	.get_gpc_mask = vgpu_gr_get_gpc_mask,
	.get_gpc_tpc_mask = vgpu_gr_get_gpc_tpc_mask,
	.init_sm_id_table = vgpu_gr_init_sm_id_table,
};

static const struct gops_gr_setup vgpu_gv11b_ops_gr_setup = {
	.alloc_obj_ctx = vgpu_gr_alloc_obj_ctx,
	.free_gr_ctx = vgpu_gr_free_gr_ctx,
	.free_subctx = vgpu_gr_setup_free_subctx,
	.set_preemption_mode = vgpu_gr_set_preemption_mode,
#ifdef CONFIG_NVGPU_GRAPHICS
	.bind_ctxsw_zcull = vgpu_gr_bind_ctxsw_zcull,
#endif
};

#ifdef CONFIG_NVGPU_GRAPHICS
static const struct gops_gr_zbc vgpu_gv11b_ops_gr_zbc = {
	.add_color = NULL,
	.add_depth = NULL,
	.set_table = vgpu_gr_add_zbc,
	.query_table = vgpu_gr_query_zbc,
	.add_stencil = gv11b_gr_zbc_add_stencil,
	.get_gpcs_swdx_dss_zbc_c_format_reg = NULL,
	.get_gpcs_swdx_dss_zbc_z_format_reg = NULL,
};
#endif

#ifdef CONFIG_NVGPU_GRAPHICS
static const struct gops_gr_zcull vgpu_gv11b_ops_gr_zcull = {
	.get_zcull_info = vgpu_gr_get_zcull_info,
	.program_zcull_mapping = NULL,
};
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
static const struct gops_gr_hwpm_map vgpu_gv11b_ops_gr_hwpm_map = {
	.align_regs_perf_pma = gv100_gr_hwpm_map_align_regs_perf_pma,
};
#endif

static const struct gops_gr_falcon vgpu_gv11b_ops_gr_falcon = {
	.init_ctx_state = vgpu_gr_init_ctx_state,
	.load_ctxsw_ucode = NULL,
};

#ifdef CONFIG_NVGPU_FECS_TRACE
static const struct gops_gr_fecs_trace vgpu_gv11b_ops_gr_fecs_trace = {
	.alloc_user_buffer = vgpu_alloc_user_buffer,
	.free_user_buffer = vgpu_free_user_buffer,
	.get_mmap_user_buffer_info = vgpu_get_mmap_user_buffer_info,
	.init = vgpu_fecs_trace_init,
	.deinit = vgpu_fecs_trace_deinit,
	.enable = vgpu_fecs_trace_enable,
	.disable = vgpu_fecs_trace_disable,
	.is_enabled = vgpu_fecs_trace_is_enabled,
	.reset = NULL,
	.flush = NULL,
	.poll = vgpu_fecs_trace_poll,
	.bind_channel = NULL,
	.unbind_channel = NULL,
	.max_entries = vgpu_fecs_trace_max_entries,
	.set_filter = vgpu_fecs_trace_set_filter,
	.get_buffer_full_mailbox_val = gv11b_fecs_trace_get_buffer_full_mailbox_val,
};
#endif

static const struct gops_gr_init vgpu_gv11b_ops_gr_init = {
	.get_no_of_sm = nvgpu_gr_get_no_of_sm,
	.get_nonpes_aware_tpc = gv11b_gr_init_get_nonpes_aware_tpc,
	.get_bundle_cb_default_size = gv11b_gr_init_get_bundle_cb_default_size,
	.get_min_gpm_fifo_depth = gv11b_gr_init_get_min_gpm_fifo_depth,
	.get_bundle_cb_token_limit = gv11b_gr_init_get_bundle_cb_token_limit,
	.get_attrib_cb_default_size = gv11b_gr_init_get_attrib_cb_default_size,
	.get_alpha_cb_default_size = gv11b_gr_init_get_alpha_cb_default_size,
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
	.get_ctx_attrib_cb_size = gp10b_gr_init_get_ctx_attrib_cb_size,
	.commit_cbes_reserve = gv11b_gr_init_commit_cbes_reserve,
	.detect_sm_arch = vgpu_gr_detect_sm_arch,
	.get_supported__preemption_modes = gp10b_gr_init_get_supported_preemption_modes,
	.get_default_preemption_modes = gp10b_gr_init_get_default_preemption_modes,
#ifdef CONFIG_NVGPU_GRAPHICS
	.get_attrib_cb_gfxp_default_size = gv11b_gr_init_get_attrib_cb_gfxp_default_size,
	.get_attrib_cb_gfxp_size = gv11b_gr_init_get_attrib_cb_gfxp_size,
	.get_ctx_spill_size = gv11b_gr_init_get_ctx_spill_size,
	.get_ctx_pagepool_size = gp10b_gr_init_get_ctx_pagepool_size,
	.get_ctx_betacb_size = gv11b_gr_init_get_ctx_betacb_size,
	.commit_ctxsw_spill = gv11b_gr_init_commit_ctxsw_spill,
	.gfxp_wfi_timeout = gv11b_gr_init_commit_gfxp_wfi_timeout,
#endif /* CONFIG_NVGPU_GRAPHICS */
};

static const struct gops_gr_intr vgpu_gv11b_ops_gr_intr = {
	.handle_gcc_exception = gv11b_gr_intr_handle_gcc_exception,
	.handle_gpc_gpcmmu_exception = gv11b_gr_intr_handle_gpc_gpcmmu_exception,
	.handle_gpc_gpccs_exception = gv11b_gr_intr_handle_gpc_gpccs_exception,
	.get_tpc_exception = gm20b_gr_intr_get_tpc_exception,
	.handle_tpc_mpc_exception = gv11b_gr_intr_handle_tpc_mpc_exception,
	.handle_tex_exception = NULL,
	.flush_channel_tlb = nvgpu_gr_intr_flush_channel_tlb,
	.get_sm_no_lock_down_hww_global_esr_mask = gv11b_gr_intr_get_sm_no_lock_down_hww_global_esr_mask,
#ifdef CONFIG_NVGPU_DEBUGGER
	.tpc_enabled_exceptions = vgpu_gr_gk20a_tpc_enabled_exceptions,
#endif
};

static const struct gops_gr vgpu_gv11b_ops_gr = {
	.gr_init_support = nvgpu_gr_init_support,
	.gr_suspend = nvgpu_gr_suspend,
#ifdef CONFIG_NVGPU_DEBUGGER
	.set_alpha_circular_buffer_size = NULL,
	.set_circular_buffer_size = NULL,
	.get_sm_dsm_perf_regs = gv11b_gr_get_sm_dsm_perf_regs,
	.get_sm_dsm_perf_ctrl_regs = gv11b_gr_get_sm_dsm_perf_ctrl_regs,
#ifdef CONFIG_NVGPU_TEGRA_FUSE
	.set_gpc_tpc_mask = NULL,
#endif
	.dump_gr_regs = NULL,
	.update_pc_sampling = vgpu_gr_update_pc_sampling,
	.init_sm_dsm_reg_info = gv11b_gr_init_sm_dsm_reg_info,
	.init_cyclestats = vgpu_gr_init_cyclestats,
	.set_sm_debug_mode = vgpu_gr_set_sm_debug_mode,
	.bpt_reg_info = NULL,
	.update_smpc_ctxsw_mode = vgpu_gr_update_smpc_ctxsw_mode,
	.update_hwpm_ctxsw_mode = vgpu_gr_update_hwpm_ctxsw_mode,
	.clear_sm_error_state = vgpu_gr_clear_sm_error_state,
	.suspend_contexts = vgpu_gr_suspend_contexts,
	.resume_contexts = vgpu_gr_resume_contexts,
	.trigger_suspend = NULL,
	.wait_for_pause = gr_gk20a_wait_for_pause,
	.resume_from_pause = NULL,
	.clear_sm_errors = NULL,
	.sm_debugger_attached = NULL,
	.suspend_single_sm = NULL,
	.suspend_all_sms = NULL,
	.resume_single_sm = NULL,
	.resume_all_sms = NULL,
	.lock_down_sm = NULL,
	.wait_for_sm_lock_down = NULL,
	.init_ovr_sm_dsm_perf = gv11b_gr_init_ovr_sm_dsm_perf,
	.get_ovr_perf_regs = gv11b_gr_get_ovr_perf_regs,
	.set_boosted_ctx = NULL,
	.pre_process_sm_exception = NULL,
	.set_bes_crop_debug3 = NULL,
	.set_bes_crop_debug4 = NULL,
	.is_etpc_addr = gv11b_gr_pri_is_etpc_addr,
	.egpc_etpc_priv_addr_table = gv11b_gr_egpc_etpc_priv_addr_table,
	.get_egpc_base = gv11b_gr_get_egpc_base,
	.get_egpc_etpc_num = gv11b_gr_get_egpc_etpc_num,
	.is_egpc_addr = gv11b_gr_pri_is_egpc_addr,
	.decode_egpc_addr = gv11b_gr_decode_egpc_addr,
	.decode_priv_addr = gr_gv11b_decode_priv_addr,
	.create_priv_addr_table = gr_gv11b_create_priv_addr_table,
	.split_fbpa_broadcast_addr = gr_gk20a_split_fbpa_broadcast_addr,
	.get_offset_in_gpccs_segment = gr_gk20a_get_offset_in_gpccs_segment,
	.set_debug_mode = gm20b_gr_set_debug_mode,
	.set_mmu_debug_mode = vgpu_gr_set_mmu_debug_mode,
#endif
};

static const struct gops_class vgpu_gv11b_ops_gpu_class = {
	.is_valid = gv11b_class_is_valid,
	.is_valid_gfx = gv11b_class_is_valid_gfx,
	.is_valid_compute = gv11b_class_is_valid_compute,
};

static const struct gops_fb_intr vgpu_gv11b_ops_fb_intr = {
	.enable = gv11b_fb_intr_enable,
	.disable = gv11b_fb_intr_disable,
	.isr = gv11b_fb_intr_isr,
	.is_mmu_fault_pending = NULL,
};

static const struct gops_fb vgpu_gv11b_ops_fb = {
	.init_hw = NULL,
	.init_fs_state = NULL,
	.set_mmu_page_size = NULL,
#ifdef CONFIG_NVGPU_COMPRESSION
	.set_use_full_comp_tag_line = NULL,
	.compression_page_size = gp10b_fb_compression_page_size,
	.compressible_page_size = gp10b_fb_compressible_page_size,
	.compression_align_mask = gm20b_fb_compression_align_mask,
#endif
	.vpr_info_fetch = NULL,
	.dump_vpr_info = NULL,
	.dump_wpr_info = NULL,
	.read_wpr_info = NULL,
#ifdef CONFIG_NVGPU_DEBUGGER
	.is_debug_mode_enabled = NULL,
	.set_debug_mode = vgpu_mm_mmu_set_debug_mode,
	.set_mmu_debug_mode = vgpu_fb_set_mmu_debug_mode,
#endif
	.tlb_invalidate = vgpu_mm_tlb_invalidate,
	.write_mmu_fault_buffer_lo_hi = gv11b_fb_write_mmu_fault_buffer_lo_hi,
	.write_mmu_fault_buffer_get = fb_gv11b_write_mmu_fault_buffer_get,
	.write_mmu_fault_buffer_size = gv11b_fb_write_mmu_fault_buffer_size,
	.write_mmu_fault_status = gv11b_fb_write_mmu_fault_status,
	.read_mmu_fault_buffer_get = gv11b_fb_read_mmu_fault_buffer_get,
	.read_mmu_fault_buffer_put = gv11b_fb_read_mmu_fault_buffer_put,
	.read_mmu_fault_buffer_size = gv11b_fb_read_mmu_fault_buffer_size,
	.read_mmu_fault_addr_lo_hi = gv11b_fb_read_mmu_fault_addr_lo_hi,
	.read_mmu_fault_inst_lo_hi = gv11b_fb_read_mmu_fault_inst_lo_hi,
	.read_mmu_fault_info = gv11b_fb_read_mmu_fault_info,
	.read_mmu_fault_status = gv11b_fb_read_mmu_fault_status,
};

static const struct gops_cg vgpu_gv11b_ops_cg = {
	.slcg_bus_load_gating_prod = NULL,
	.slcg_ce2_load_gating_prod = NULL,
	.slcg_chiplet_load_gating_prod = NULL,
	.slcg_fb_load_gating_prod = NULL,
	.slcg_fifo_load_gating_prod = NULL,
	.slcg_gr_load_gating_prod = NULL,
	.slcg_ltc_load_gating_prod = NULL,
	.slcg_perf_load_gating_prod = NULL,
	.slcg_priring_load_gating_prod = NULL,
	.slcg_pmu_load_gating_prod = NULL,
	.slcg_therm_load_gating_prod = NULL,
	.slcg_xbar_load_gating_prod = NULL,
	.blcg_bus_load_gating_prod = NULL,
	.blcg_ce_load_gating_prod = NULL,
	.blcg_fb_load_gating_prod = NULL,
	.blcg_fifo_load_gating_prod = NULL,
	.blcg_gr_load_gating_prod = NULL,
	.blcg_ltc_load_gating_prod = NULL,
	.blcg_pmu_load_gating_prod = NULL,
	.blcg_xbar_load_gating_prod = NULL,
};

static const struct gops_fifo vgpu_gv11b_ops_fifo = {
	.fifo_init_support = nvgpu_fifo_init_support,
	.fifo_suspend = nvgpu_fifo_suspend,
	.init_fifo_setup_hw = vgpu_gv11b_init_fifo_setup_hw,
	.preempt_channel = vgpu_fifo_preempt_channel,
	.preempt_tsg = vgpu_fifo_preempt_tsg,
	.is_preempt_pending = gv11b_fifo_is_preempt_pending,
	.reset_enable_hw = NULL,
#ifdef CONFIG_NVGPU_RECOVERY
	.recover = NULL,
#endif
	.setup_sw = vgpu_fifo_setup_sw,
	.cleanup_sw = vgpu_fifo_cleanup_sw,
	.set_sm_exception_type_mask = vgpu_set_sm_exception_type_mask,
	.intr_0_enable = NULL,
	.intr_1_enable = NULL,
	.intr_0_isr = NULL,
	.intr_1_isr = NULL,
	.handle_sched_error = NULL,
	.handle_ctxsw_timeout = NULL,
	.ctxsw_timeout_enable = NULL,
	.trigger_mmu_fault = NULL,
	.get_mmu_fault_info = NULL,
	.get_mmu_fault_desc = NULL,
	.get_mmu_fault_client_desc = NULL,
	.get_mmu_fault_gpc_desc = NULL,
	.mmu_fault_id_to_pbdma_id = gv11b_fifo_mmu_fault_id_to_pbdma_id,
};

static const struct gops_engine vgpu_gv11b_ops_engine = {
	.is_fault_engine_subid_gpc = gv11b_is_fault_engine_subid_gpc,
	.init_ce_info = gp10b_engine_init_ce_info,
};

static const struct gops_pbdma vgpu_gv11b_ops_pbdma = {
	.setup_sw = NULL,
	.cleanup_sw = NULL,
	.setup_hw = NULL,
	.intr_enable = NULL,
	.acquire_val = gm20b_pbdma_acquire_val,
	.get_signature = gp10b_pbdma_get_signature,
	.dump_status = NULL,
	.handle_intr_0 = NULL,
	.handle_intr_1 = gv11b_pbdma_handle_intr_1,
	.handle_intr = gm20b_pbdma_handle_intr,
	.read_data = NULL,
	.reset_header = NULL,
	.device_fatal_0_intr_descs = NULL,
	.channel_fatal_0_intr_descs = NULL,
	.restartable_0_intr_descs = NULL,
	.format_gpfifo_entry = gm20b_pbdma_format_gpfifo_entry,
};

#ifdef CONFIG_TEGRA_GK20A_NVHOST
static const struct gops_sync_syncpt vgpu_gv11b_ops_sync_syncpt = {
	.get_sync_ro_map = vgpu_gv11b_syncpt_get_sync_ro_map,
	.alloc_buf = vgpu_gv11b_syncpt_alloc_buf,
	.free_buf = vgpu_gv11b_syncpt_free_buf,
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
static const struct gops_sync_sema vgpu_gv11b_ops_sync_sema = {
	.add_wait_cmd = gv11b_sema_add_wait_cmd,
	.get_wait_cmd_size = gv11b_sema_get_wait_cmd_size,
	.add_incr_cmd = gv11b_sema_add_incr_cmd,
	.get_incr_cmd_size = gv11b_sema_get_incr_cmd_size,
};
#endif

static const struct gops_sync vgpu_gv11b_ops_sync = {
};

static const struct gops_engine_status vgpu_gv11b_ops_engine_status = {
	.read_engine_status_info = NULL,
	.dump_engine_status = NULL,
};

static const struct gops_pbdma_status vgpu_gv11b_ops_pbdma_status = {
	.read_pbdma_status_info = NULL,
};

static const struct gops_ramfc vgpu_gv11b_ops_ramfc = {
	.setup = vgpu_ramfc_setup,
	.capture_ram_dump = NULL,
	.commit_userd = NULL,
	.get_syncpt = NULL,
	.set_syncpt = NULL,
};

static const struct gops_ramin vgpu_gv11b_ops_ramin = {
	.set_gr_ptr = NULL,
	.set_big_page_size = gm20b_ramin_set_big_page_size,
	.init_pdb = gv11b_ramin_init_pdb,
	.init_subctx_pdb = gv11b_ramin_init_subctx_pdb,
	.set_adr_limit = NULL,
	.base_shift = gk20a_ramin_base_shift,
	.alloc_size = gk20a_ramin_alloc_size,
	.set_eng_method_buffer = NULL,
};

static const struct gops_runlist vgpu_gv11b_ops_runlist = {
	.reschedule = NULL,
	.update = vgpu_runlist_update,
	.reload = vgpu_runlist_reload,
	.count_max = gv11b_runlist_count_max,
	.entry_size = vgpu_runlist_entry_size,
	.length_max = vgpu_runlist_length_max,
	.get_tsg_entry = gv11b_runlist_get_tsg_entry,
	.get_ch_entry = gv11b_runlist_get_ch_entry,
	.hw_submit = NULL,
	.wait_pending = NULL,
	.init_enginfo = nvgpu_runlist_init_enginfo,
	.get_tsg_max_timeslice = gv11b_runlist_max_timeslice,
	.get_max_channels_per_tsg = gv11b_runlist_get_max_channels_per_tsg,
};

static const struct gops_userd vgpu_gv11b_ops_userd = {
#ifdef CONFIG_NVGPU_USERD
	.setup_sw = vgpu_userd_setup_sw,
	.cleanup_sw = vgpu_userd_cleanup_sw,
	.init_mem = gk20a_userd_init_mem,
	.gp_get = gv11b_userd_gp_get,
	.gp_put = gv11b_userd_gp_put,
	.pb_get = gv11b_userd_pb_get,
#endif
	.entry_size = gk20a_userd_entry_size,
};

static const struct gops_channel vgpu_gv11b_ops_channel = {
	.alloc_inst = vgpu_channel_alloc_inst,
	.free_inst = vgpu_channel_free_inst,
	.bind = vgpu_channel_bind,
	.unbind = vgpu_channel_unbind,
	.enable = vgpu_channel_enable,
	.disable = vgpu_channel_disable,
	.count = vgpu_channel_count,
	.abort_clean_up = nvgpu_channel_abort_clean_up,
	.suspend_all_serviceable_ch = nvgpu_channel_suspend_all_serviceable_ch,
	.resume_all_serviceable_ch = nvgpu_channel_resume_all_serviceable_ch,
	.set_error_notifier = nvgpu_set_err_notifier,
};

static const struct gops_tsg vgpu_gv11b_ops_tsg = {
	.open = vgpu_tsg_open,
	.release = vgpu_tsg_release,
	.init_eng_method_buffers = NULL,
	.deinit_eng_method_buffers = NULL,
	.enable = gv11b_tsg_enable,
	.disable = nvgpu_tsg_disable,
	.bind_channel = vgpu_tsg_bind_channel,
	.bind_channel_eng_method_buffers = NULL,
	.unbind_channel = vgpu_tsg_unbind_channel,
	.unbind_channel_check_hw_state = NULL,
	.unbind_channel_check_ctx_reload = NULL,
	.unbind_channel_check_eng_faulted = NULL,
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	.check_ctxsw_timeout = nvgpu_tsg_check_ctxsw_timeout,
#endif
	.force_reset = vgpu_tsg_force_reset_ch,
	.post_event_id = nvgpu_tsg_post_event_id,
	.set_timeslice = vgpu_tsg_set_timeslice,
	.default_timeslice_us = vgpu_tsg_default_timeslice_us,
	.set_interleave = vgpu_tsg_set_interleave,
	.set_long_timeslice = vgpu_tsg_set_long_timeslice,
};

static const struct gops_usermode vgpu_gv11b_ops_usermode = {
	.setup_hw = NULL,
	.base = gv11b_usermode_base,
	.bus_base = gv11b_usermode_bus_base,
	.ring_doorbell = gv11b_usermode_ring_doorbell,
	.doorbell_token = gv11b_usermode_doorbell_token,
};

static const struct gops_netlist vgpu_gv11b_ops_netlist = {
	.get_netlist_name = gv11b_netlist_get_name,
	.is_fw_defined = gv11b_netlist_is_firmware_defined,
};

static const struct gops_mm_mmu_fault vgpu_gv11b_ops_mm_mmu_fault = {
	.info_mem_destroy = gv11b_mm_mmu_fault_info_mem_destroy,
};

static const struct gops_mm_cache vgpu_gv11b_ops_mm_cache = {
	.fb_flush = vgpu_mm_fb_flush,
	.l2_invalidate = vgpu_mm_l2_invalidate,
	.l2_flush = vgpu_mm_l2_flush,
#ifdef CONFIG_NVGPU_COMPRESSION
	.cbc_clean = NULL,
#endif
};

static const struct gops_mm_gmmu vgpu_gv11b_ops_mm_gmmu = {
	.map = vgpu_locked_gmmu_map,
	.unmap = vgpu_locked_gmmu_unmap,
	.get_big_page_sizes = gm20b_mm_get_big_page_sizes,
	.get_default_big_page_size = nvgpu_gmmu_default_big_page_size,
	.gpu_phys_addr = gm20b_gpu_phys_addr,
	.get_iommu_bit = gk20a_mm_get_iommu_bit,
	.get_mmu_levels = gp10b_mm_get_mmu_levels,
	.get_max_page_table_levels = gp10b_get_max_page_table_levels,
};

static const struct gops_mm vgpu_gv11b_ops_mm = {
	.init_mm_support = nvgpu_init_mm_support,
	.pd_cache_init = nvgpu_pd_cache_init,
	.mm_suspend = nvgpu_mm_suspend,
	.vm_bind_channel = vgpu_vm_bind_channel,
	.setup_hw = NULL,
	.is_bar1_supported = gv11b_mm_is_bar1_supported,
	.init_inst_block = gv11b_mm_init_inst_block,
	.init_inst_block_for_subctxs = gv11b_mm_init_inst_block_for_subctxs,
	.init_bar2_vm = gp10b_mm_init_bar2_vm,
	.remove_bar2_vm = gp10b_mm_remove_bar2_vm,
	.bar1_map_userd = NULL,
	.vm_as_alloc_share = vgpu_vm_as_alloc_share,
	.vm_as_free_share = vgpu_vm_as_free_share,
	.get_default_va_sizes = gp10b_mm_get_default_va_sizes,
};

static const struct gops_therm vgpu_gv11b_ops_therm = {
	.init_therm_support = nvgpu_init_therm_support,
	.init_therm_setup_hw = NULL,
	.init_elcg_mode = NULL,
	.init_blcg_mode = NULL,
	.elcg_init_idle_filters = NULL,
};

#ifdef CONFIG_NVGPU_LS_PMU
static const struct gops_pmu vgpu_gv11b_ops_pmu = {
	.pmu_early_init = NULL,
	.pmu_rtos_init = NULL,
	.pmu_pstate_sw_setup = NULL,
	.pmu_pstate_pmu_setup = NULL,
	.pmu_destroy = NULL,
	.pmu_setup_elpg = NULL,
	.pmu_get_queue_head = NULL,
	.pmu_get_queue_head_size = NULL,
	.pmu_get_queue_tail = NULL,
	.pmu_get_queue_tail_size = NULL,
	.pmu_reset = NULL,
	.pmu_queue_head = NULL,
	.pmu_queue_tail = NULL,
	.pmu_msgq_tail = NULL,
	.pmu_mutex_size = NULL,
	.pmu_mutex_acquire = NULL,
	.pmu_mutex_release = NULL,
	.pmu_is_interrupted = NULL,
	.pmu_isr = NULL,
	.pmu_init_perfmon_counter = NULL,
	.pmu_pg_idle_counter_config = NULL,
	.pmu_read_idle_counter = NULL,
	.pmu_reset_idle_counter = NULL,
	.pmu_read_idle_intr_status = NULL,
	.pmu_clear_idle_intr_status = NULL,
	.pmu_dump_elpg_stats = NULL,
	.pmu_dump_falcon_stats = NULL,
	.pmu_enable_irq = NULL,
	.write_dmatrfbase = NULL,
	.dump_secure_fuses = NULL,
	.reset_engine = NULL,
	.is_engine_in_reset = NULL,
	.pmu_ns_bootstrap = NULL,
	.is_pmu_supported = NULL,
};
#endif

static const struct gops_clk_arb vgpu_gv11b_ops_clk_arb = {
	.clk_arb_init_arbiter = nvgpu_clk_arb_init_arbiter,
	.check_clk_arb_support = gp10b_check_clk_arb_support,
	.get_arbiter_clk_domains = gp10b_get_arbiter_clk_domains,
	.get_arbiter_f_points = gp10b_get_arbiter_f_points,
	.get_arbiter_clk_range = gp10b_get_arbiter_clk_range,
	.get_arbiter_clk_default = gp10b_get_arbiter_clk_default,
	.arbiter_clk_init = gp10b_init_clk_arbiter,
	.clk_arb_run_arbiter_cb = gp10b_clk_arb_run_arbiter_cb,
	.clk_arb_cleanup = gp10b_clk_arb_cleanup,
};

#ifdef CONFIG_NVGPU_DEBUGGER
static const struct gops_regops vgpu_gv11b_ops_regops = {
	.exec_regops = vgpu_exec_regops,
	.get_global_whitelist_ranges = gv11b_get_global_whitelist_ranges,
	.get_global_whitelist_ranges_count = gv11b_get_global_whitelist_ranges_count,
	.get_context_whitelist_ranges = gv11b_get_context_whitelist_ranges,
	.get_context_whitelist_ranges_count = gv11b_get_context_whitelist_ranges_count,
	.get_runcontrol_whitelist = gv11b_get_runcontrol_whitelist,
	.get_runcontrol_whitelist_count = gv11b_get_runcontrol_whitelist_count,
	.get_hwpm_perfmon_register_stride = gv11b_get_hwpm_perfmon_register_stride,
	.get_hwpm_router_register_stride = gv11b_get_hwpm_router_register_stride,
	.get_hwpm_pma_channel_register_stride = gv11b_get_hwpm_pma_channel_register_stride,
	.get_hwpm_pma_trigger_register_stride = gv11b_get_hwpm_pma_trigger_register_stride,
	.get_smpc_register_stride = gv11b_get_smpc_register_stride,
	.get_cau_register_stride = NULL,
	.get_hwpm_perfmon_register_offset_allowlist =
		gv11b_get_hwpm_perfmon_register_offset_allowlist,
	.get_hwpm_router_register_offset_allowlist =
		gv11b_get_hwpm_router_register_offset_allowlist,
	.get_hwpm_pma_channel_register_offset_allowlist =
		gv11b_get_hwpm_pma_channel_register_offset_allowlist,
	.get_hwpm_pma_trigger_register_offset_allowlist =
		gv11b_get_hwpm_pma_trigger_register_offset_allowlist,
	.get_smpc_register_offset_allowlist = gv11b_get_smpc_register_offset_allowlist,
	.get_cau_register_offset_allowlist = NULL,
	.get_hwpm_perfmon_register_ranges = gv11b_get_hwpm_perfmon_register_ranges,
	.get_hwpm_router_register_ranges = gv11b_get_hwpm_router_register_ranges,
	.get_hwpm_pma_channel_register_ranges = gv11b_get_hwpm_pma_channel_register_ranges,
	.get_hwpm_pma_trigger_register_ranges = gv11b_get_hwpm_pma_trigger_register_ranges,
	.get_smpc_register_ranges = gv11b_get_smpc_register_ranges,
	.get_hwpm_pc_sampler_register_ranges = gv11b_get_hwpm_pc_sampler_register_ranges,
	.get_cau_register_ranges = NULL,
	.get_hwpm_perfmux_register_ranges = gv11b_get_hwpm_perfmux_register_ranges,
};
#endif

static const struct gops_mc vgpu_gv11b_ops_mc = {
	.get_chip_details = NULL,
	.intr_mask = NULL,
	.intr_enable = NULL,
	.intr_stall_unit_config = NULL,
	.intr_nonstall_unit_config = NULL,
	.isr_stall = NULL,
	.intr_stall = NULL,
	.intr_stall_pause = NULL,
	.intr_stall_resume = NULL,
	.intr_nonstall = NULL,
	.intr_nonstall_pause = NULL,
	.intr_nonstall_resume = NULL,
	.isr_nonstall = NULL,
	.is_intr1_pending = NULL,
	.is_intr_hub_pending = NULL,
	.log_pending_intrs = NULL,
	.is_enabled = NULL,
	.fb_reset = NULL,
	.is_mmu_fault_pending = NULL,
	.enable_units = NULL,
	.enable_dev = NULL,
	.enable_devtype = NULL,
};

static const struct gops_debug vgpu_gv11b_ops_debug = {
	.show_dump = NULL,
};

#ifdef CONFIG_NVGPU_DEBUGGER
static const struct gops_debugger vgpu_gv11b_ops_debugger = {
	.post_events = nvgpu_dbg_gpu_post_events,
	.dbg_set_powergate = vgpu_dbg_set_powergate,
};
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
static const struct gops_perf vgpu_gv11b_ops_perf = {
	.get_pmmsys_per_chiplet_offset = gv11b_perf_get_pmmsys_per_chiplet_offset,
	.get_pmmgpc_per_chiplet_offset = gv11b_perf_get_pmmgpc_per_chiplet_offset,
	.get_pmmfbp_per_chiplet_offset = gv11b_perf_get_pmmfbp_per_chiplet_offset,
	.update_get_put = vgpu_perf_update_get_put,
};
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
static const struct gops_perfbuf vgpu_gv11b_ops_perfbuf = {
	.perfbuf_enable = vgpu_perfbuffer_enable,
	.perfbuf_disable = vgpu_perfbuffer_disable,
	.init_inst_block = vgpu_perfbuffer_init_inst_block,
	.deinit_inst_block = vgpu_perfbuffer_deinit_inst_block,
	.update_get_put = nvgpu_perfbuf_update_get_put,
};
#endif

#ifdef CONFIG_NVGPU_PROFILER
static const struct gops_pm_reservation vgpu_gv11b_ops_pm_reservation = {
	.acquire = vgpu_pm_reservation_acquire,
	.release = vgpu_pm_reservation_release,
	.release_all_per_vmid = NULL,
};
#endif

#ifdef CONFIG_NVGPU_PROFILER
static const struct gops_profiler vgpu_gv11b_ops_profiler = {
	.bind_hwpm = vgpu_profiler_bind_hwpm,
	.unbind_hwpm = vgpu_profiler_unbind_hwpm,
	.bind_hwpm_streamout = vgpu_profiler_bind_hwpm_streamout,
	.unbind_hwpm_streamout = vgpu_profiler_unbind_hwpm_streamout,
	.bind_smpc = vgpu_profiler_bind_smpc,
	.unbind_smpc = vgpu_profiler_unbind_smpc,
};
#endif

static const struct gops_bus vgpu_gv11b_ops_bus = {
	.init_hw = NULL,
	.isr = NULL,
	.bar1_bind = NULL,
	.bar2_bind = NULL,
#ifdef CONFIG_NVGPU_DGPU
	.set_bar0_window = NULL,
#endif
};

static const struct gops_ptimer vgpu_gv11b_ops_ptimer = {
	.isr = NULL,
	.read_ptimer = vgpu_read_ptimer,
#ifdef CONFIG_NVGPU_IOCTL_NON_FUSA
	.get_timestamps_zipper = vgpu_get_timestamps_zipper,
#endif
#ifdef CONFIG_NVGPU_PROFILER
	.get_timer_reg_offsets = gv11b_ptimer_get_timer_reg_offsets,
#endif
};

#if defined(CONFIG_NVGPU_CYCLESTATS)
static const struct gops_css vgpu_gv11b_ops_css = {
	.enable_snapshot = vgpu_css_enable_snapshot_buffer,
	.disable_snapshot = vgpu_css_release_snapshot_buffer,
	.check_data_available = vgpu_css_flush_snapshots,
	.detach_snapshot = vgpu_css_detach,
	.set_handled_snapshots = NULL,
	.allocate_perfmon_ids = NULL,
	.release_perfmon_ids = NULL,
	.get_max_buffer_size = vgpu_css_get_buffer_size,
};
#endif

static const struct gops_falcon vgpu_gv11b_ops_falcon = {
	.falcon_sw_init = nvgpu_falcon_sw_init,
	.falcon_sw_free = nvgpu_falcon_sw_free,
};

static const struct gops_priv_ring vgpu_gv11b_ops_priv_ring = {
	.enable_priv_ring = NULL,
	.isr = NULL,
	.set_ppriv_timeout_settings = NULL,
	.enum_ltc = NULL,
	.get_gpc_count = vgpu_gr_get_gpc_count,
};

static const struct gops_fuse vgpu_gv11b_ops_fuse = {
	.is_opt_ecc_enable = NULL,
	.is_opt_feature_override_disable = NULL,
	.fuse_status_opt_fbio = NULL,
	.fuse_status_opt_fbp = NULL,
	.fuse_status_opt_l2_fbp = NULL,
	.fuse_status_opt_gpc = NULL,
	.fuse_status_opt_tpc_gpc = NULL,
	.fuse_ctrl_opt_tpc_gpc = NULL,
	.fuse_ctrl_opt_fbp = NULL,
	.fuse_ctrl_opt_gpc = NULL,
	.fuse_opt_sec_debug_en = NULL,
	.fuse_opt_priv_sec_en = NULL,
	.read_vin_cal_fuse_rev = NULL,
	.read_vin_cal_slope_intercept_fuse = NULL,
	.read_vin_cal_gain_offset_fuse = NULL,
};

static const struct gops_top vgpu_gv11b_ops_top = {
	.get_max_gpc_count = vgpu_gr_get_max_gpc_count,
	.get_max_fbps_count = vgpu_gr_get_max_fbps_count,
	.get_max_ltc_per_fbp = vgpu_gr_get_max_ltc_per_fbp,
	.get_max_lts_per_ltc = vgpu_gr_get_max_lts_per_ltc,
	.parse_next_device = vgpu_top_parse_next_dev,
};

static const struct gops_grmgr vgpu_gv11b_ops_grmgr = {
	.init_gr_manager = nvgpu_init_gr_manager,
};

int vgpu_gv11b_init_hal(struct gk20a *g)
{
	struct gpu_ops *gops = &g->ops;
	struct vgpu_priv_data *priv = vgpu_get_priv_data(g);

	gops->acr = vgpu_gv11b_ops_acr;
#ifdef CONFIG_NVGPU_DGPU
	gops->bios = vgpu_gv11b_ops_bios;
#endif /* CONFIG_NVGPU_DGPU */
	gops->ltc = vgpu_gv11b_ops_ltc;
	gops->ltc.intr = vgpu_gv11b_ops_ltc_intr;
#ifdef CONFIG_NVGPU_COMPRESSION
	gops->cbc = vgpu_gv11b_ops_cbc;
#endif
	gops->ce = vgpu_gv11b_ops_ce;
	gops->gr = vgpu_gv11b_ops_gr;
	gops->gr.ctxsw_prog = vgpu_gv11b_ops_gr_ctxsw_prog;
	gops->gr.config = vgpu_gv11b_ops_gr_config;
	gops->gr.setup = vgpu_gv11b_ops_gr_setup;
#ifdef CONFIG_NVGPU_GRAPHICS
	gops->gr.zbc = vgpu_gv11b_ops_gr_zbc;
	gops->gr.zcull = vgpu_gv11b_ops_gr_zcull;
#endif /* CONFIG_NVGPU_GRAPHICS */
#ifdef CONFIG_NVGPU_DEBUGGER
	gops->gr.hwpm_map = vgpu_gv11b_ops_gr_hwpm_map;
#endif
	gops->gr.falcon = vgpu_gv11b_ops_gr_falcon;
#ifdef CONFIG_NVGPU_FECS_TRACE
	gops->gr.fecs_trace = vgpu_gv11b_ops_gr_fecs_trace;
#endif /* CONFIG_NVGPU_FECS_TRACE */
	gops->gr.init = vgpu_gv11b_ops_gr_init;
	gops->gr.intr = vgpu_gv11b_ops_gr_intr;
	gops->gpu_class = vgpu_gv11b_ops_gpu_class;
	gops->fb = vgpu_gv11b_ops_fb;
	gops->fb.intr = vgpu_gv11b_ops_fb_intr;
	gops->cg = vgpu_gv11b_ops_cg;
	gops->fifo = vgpu_gv11b_ops_fifo;
	gops->engine = vgpu_gv11b_ops_engine;
	gops->pbdma = vgpu_gv11b_ops_pbdma;
	gops->sync = vgpu_gv11b_ops_sync;
#ifdef CONFIG_TEGRA_GK20A_NVHOST
	gops->sync.syncpt = vgpu_gv11b_ops_sync_syncpt;
#endif /* CONFIG_TEGRA_GK20A_NVHOST */
#if defined(CONFIG_NVGPU_KERNEL_MODE_SUBMIT) && \
	defined(CONFIG_NVGPU_SW_SEMAPHORE)
	gops->sync.sema = vgpu_gv11b_ops_sync_sema;
#endif
	gops->engine_status = vgpu_gv11b_ops_engine_status;
	gops->pbdma_status = vgpu_gv11b_ops_pbdma_status;
	gops->ramfc = vgpu_gv11b_ops_ramfc;
	gops->ramin = vgpu_gv11b_ops_ramin;
	gops->runlist = vgpu_gv11b_ops_runlist;
	gops->userd = vgpu_gv11b_ops_userd;
	gops->channel = vgpu_gv11b_ops_channel;
	gops->tsg = vgpu_gv11b_ops_tsg;
	gops->usermode = vgpu_gv11b_ops_usermode;
	gops->netlist = vgpu_gv11b_ops_netlist;
	gops->mm = vgpu_gv11b_ops_mm;
	gops->mm.mmu_fault = vgpu_gv11b_ops_mm_mmu_fault;
	gops->mm.cache = vgpu_gv11b_ops_mm_cache;
	gops->mm.gmmu = vgpu_gv11b_ops_mm_gmmu;
	gops->therm = vgpu_gv11b_ops_therm;
#ifdef CONFIG_NVGPU_LS_PMU
	gops->pmu = vgpu_gv11b_ops_pmu;
#endif
	gops->clk_arb = vgpu_gv11b_ops_clk_arb;
#ifdef CONFIG_NVGPU_DEBUGGER
	gops->regops = vgpu_gv11b_ops_regops;
#endif
	gops->mc = vgpu_gv11b_ops_mc;
	gops->debug = vgpu_gv11b_ops_debug;
#ifdef CONFIG_NVGPU_DEBUGGER
	gops->debugger = vgpu_gv11b_ops_debugger;
	gops->perf = vgpu_gv11b_ops_perf;
	gops->perfbuf = vgpu_gv11b_ops_perfbuf;
#endif
#ifdef CONFIG_NVGPU_PROFILER
	gops->pm_reservation = vgpu_gv11b_ops_pm_reservation;
	gops->profiler = vgpu_gv11b_ops_profiler;
#endif
	gops->bus = vgpu_gv11b_ops_bus;
	gops->ptimer = vgpu_gv11b_ops_ptimer;
#if defined(CONFIG_NVGPU_CYCLESTATS)
	gops->css = vgpu_gv11b_ops_css;
#endif
	gops->falcon = vgpu_gv11b_ops_falcon;
	gops->priv_ring = vgpu_gv11b_ops_priv_ring;
	gops->fuse = vgpu_gv11b_ops_fuse;
	gops->top = vgpu_gv11b_ops_top;
	gops->grmgr = vgpu_gv11b_ops_grmgr;

	nvgpu_set_errata(g, NVGPU_ERRATA_2016608, true);
	nvgpu_set_errata(g, NVGPU_ERRATA_200391931, true);
	nvgpu_set_errata(g, NVGPU_ERRATA_SYNCPT_INVALID_ID_0, true);

#ifdef CONFIG_NVGPU_FECS_TRACE
	nvgpu_set_enabled(g, NVGPU_SUPPORT_FECS_CTXSW_TRACE, true);
#endif
#ifdef CONFIG_NVGPU_PROFILER
	nvgpu_set_enabled(g, NVGPU_SUPPORT_PROFILER_V2_DEVICE, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_PROFILER_V2_CONTEXT, false);
#endif

	/* Lone functions */
	gops->chip_init_gpu_characteristics = vgpu_gv11b_init_gpu_characteristics;
	gops->get_litter_value = gv11b_get_litter_value;
	gops->semaphore_wakeup = nvgpu_channel_semaphore_wakeup;

	if (!priv->constants.can_set_clkrate) {
		gops->clk_arb.get_arbiter_clk_domains = NULL;
		nvgpu_set_enabled(g, NVGPU_CLK_ARB_ENABLED, false);
	} else {
		nvgpu_set_enabled(g, NVGPU_CLK_ARB_ENABLED, true);
	}

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
	 * is initialized, it is used by all subsequent contexts created.
	 * The proposal is for setting up the virtual SM identifier to TPC
	 * mapping on a per-context basis and initializing this virtual SM
	 * identifier to TPC mapping differently for the mission and
	 * redundant contexts.
	 *
	 * The recommendation for the redundant setting is to offset the
	 * assignment by 1 (TPC). This will ensure both GPC and TPC diversity.
	 * The SM and Quadrant diversity will happen naturally.
	 *
	 * For kernels with few CTAs, the diversity is guaranteed to be 100%.
	 * In case of completely random CTA allocation, e.g. large number of
	 * CTAs in the waiting queue, the diversity is 1 - 1/#SM,
	 * or 87.5% for GV11B.
	 */
	if (priv->constants.max_sm_diversity_config_count > 1U) {
		nvgpu_set_enabled(g, NVGPU_SUPPORT_SM_DIVERSITY, true);
	}
#else
	priv->constants.max_sm_diversity_config_count =
		NVGPU_DEFAULT_SM_DIVERSITY_CONFIG_COUNT;
#endif
	g->max_sm_diversity_config_count =
		priv->constants.max_sm_diversity_config_count;

#ifdef CONFIG_NVGPU_COMPRESSION
	nvgpu_set_enabled(g, NVGPU_SUPPORT_COMPRESSION, true);
#endif

#ifdef CONFIG_NVGPU_RECOVERY
	nvgpu_set_enabled(g, NVGPU_SUPPORT_FAULT_RECOVERY, true);
#endif

	g->name = "gv11b";

	return 0;
}
