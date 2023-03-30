/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_GOPS_DEBUGGER_H
#define NVGPU_GOPS_DEBUGGER_H

#ifdef CONFIG_NVGPU_DEBUGGER
struct gops_regops {
	int (*exec_regops)(struct gk20a *g,
				struct nvgpu_tsg *tsg,
				struct nvgpu_dbg_reg_op *ops,
				u32 num_ops,
				u32 ctx_wr_count,
				u32 ctx_rd_count,
				u32 *flags);
	const struct regop_offset_range* (
			*get_global_whitelist_ranges)(void);
	u64 (*get_global_whitelist_ranges_count)(void);
	const struct regop_offset_range* (
			*get_context_whitelist_ranges)(void);
	u64 (*get_context_whitelist_ranges_count)(void);
	const u32* (*get_runcontrol_whitelist)(void);
	u64 (*get_runcontrol_whitelist_count)(void);
	u32 (*get_hwpm_perfmon_register_stride)(void);
	u32 (*get_hwpm_router_register_stride)(void);
	u32 (*get_hwpm_pma_channel_register_stride)(void);
	u32 (*get_hwpm_pma_trigger_register_stride)(void);
	u32 (*get_smpc_register_stride)(void);
	u32 (*get_cau_register_stride)(void);
	const u32 *(*get_hwpm_perfmon_register_offset_allowlist)(u32 *count);
	const u32 *(*get_hwpm_router_register_offset_allowlist)(u32 *count);
	const u32 *(*get_hwpm_pma_channel_register_offset_allowlist)(u32 *count);
	const u32 *(*get_hwpm_pma_trigger_register_offset_allowlist)(u32 *count);
	const u32 *(*get_smpc_register_offset_allowlist)(u32 *count);
	const u32 *(*get_cau_register_offset_allowlist)(u32 *count);
	const struct nvgpu_pm_resource_register_range *
		(*get_hwpm_perfmon_register_ranges)(u32 *count);
	const struct nvgpu_pm_resource_register_range *
		(*get_hwpm_router_register_ranges)(u32 *count);
	const struct nvgpu_pm_resource_register_range *
		(*get_hwpm_pma_channel_register_ranges)(u32 *count);
	const struct nvgpu_pm_resource_register_range *
		(*get_hwpm_pc_sampler_register_ranges)(u32 *count);
	const struct nvgpu_pm_resource_register_range *
		(*get_hwpm_pma_trigger_register_ranges)(u32 *count);
	const struct nvgpu_pm_resource_register_range *
		(*get_smpc_register_ranges)(u32 *count);
	const struct nvgpu_pm_resource_register_range *
		(*get_cau_register_ranges)(u32 *count);
	const struct nvgpu_pm_resource_register_range *
		(*get_hwpm_perfmux_register_ranges)(u32 *count);
};
struct gops_debugger {
	void (*post_events)(struct nvgpu_channel *ch);
	int (*dbg_set_powergate)(struct dbg_session_gk20a *dbg_s,
				bool disable_powergate);
};
struct gops_perf {
	void (*enable_membuf)(struct gk20a *g, u32 size, u64 buf_addr);
	void (*disable_membuf)(struct gk20a *g);
	void (*bind_mem_bytes_buffer_addr)(struct gk20a *g, u64 buf_addr);
	void (*init_inst_block)(struct gk20a *g,
		struct nvgpu_mem *inst_block);
	void (*deinit_inst_block)(struct gk20a *g);
	void (*membuf_reset_streaming)(struct gk20a *g);
	u32 (*get_membuf_pending_bytes)(struct gk20a *g);
	void (*set_membuf_handled_bytes)(struct gk20a *g,
		u32 entries, u32 entry_size);
	bool (*get_membuf_overflow_status)(struct gk20a *g);
	u32 (*get_pmmsys_per_chiplet_offset)(void);
	u32 (*get_pmmgpc_per_chiplet_offset)(void);
	u32 (*get_pmmgpcrouter_per_chiplet_offset)(void);
	u32 (*get_pmmfbprouter_per_chiplet_offset)(void);
	u32 (*get_pmmfbp_per_chiplet_offset)(void);
	int (*update_get_put)(struct gk20a *g, u64 bytes_consumed,
		bool update_available_bytes, u64 *put_ptr, bool *overflowed);
	const u32 *(*get_hwpm_sys_perfmon_regs)(u32 *count);
	const u32 *(*get_hwpm_fbp_perfmon_regs)(u32 *count);
	const u32 *(*get_hwpm_gpc_perfmon_regs)(u32 *count);
	u32 (*get_hwpm_fbp_perfmon_regs_base)(struct gk20a *g);
	u32 (*get_hwpm_gpc_perfmon_regs_base)(struct gk20a *g);
	u32 (*get_hwpm_fbprouter_perfmon_regs_base)(struct gk20a *g);
	u32 (*get_hwpm_gpcrouter_perfmon_regs_base)(struct gk20a *g);
	void (*init_hwpm_pmm_register)(struct gk20a *g);
	void (*get_num_hwpm_perfmon)(struct gk20a *g, u32 *num_sys_perfmon,
				     u32 *num_fbp_perfmon,
				     u32 *num_gpc_perfmon);
	void (*set_pmm_register)(struct gk20a *g, u32 offset, u32 val,
			 u32 num_chiplets, u32 chiplet_stride, u32 num_perfmons);
	void (*reset_hwpm_pmm_registers)(struct gk20a *g);
	void (*pma_stream_enable)(struct gk20a *g, bool enable);
	void (*disable_all_perfmons)(struct gk20a *g);
	int (*wait_for_idle_pmm_routers)(struct gk20a *g);
	int (*wait_for_idle_pma)(struct gk20a *g);
#if defined(CONFIG_NVGPU_HAL_NON_FUSA)
	void (*enable_hs_streaming)(struct gk20a *g, bool enable);
	void (*reset_hs_streaming_credits)(struct gk20a *g);
	void (*enable_pmasys_legacy_mode)(struct gk20a *g, bool enable);
#endif
};
struct gops_perfbuf {
	int (*perfbuf_enable)(struct gk20a *g, u64 offset, u32 size);
	int (*perfbuf_disable)(struct gk20a *g);
	int (*init_inst_block)(struct gk20a *g);
	void (*deinit_inst_block)(struct gk20a *g);
	int (*update_get_put)(struct gk20a *g, u64 bytes_consumed, u64 *bytes_available,
		void *cpuva, bool wait, u64 *put_ptr, bool *overflowed);
};
#endif

#endif /* NVGPU_GOPS_DEBUGGER_H */
