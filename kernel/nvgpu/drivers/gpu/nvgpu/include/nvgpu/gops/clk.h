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
#ifndef NVGPU_GOPS_CLK_H
#define NVGPU_GOPS_CLK_H

#include <nvgpu/types.h>

struct gk20a;
struct namemap_cfg;
struct clk_gk20a;
#ifdef CONFIG_NVGPU_CLK_ARB
struct nvgpu_clk_pll_debug_data;
#endif

/**
 * @brief clk gops.
 *
 * The structure contains function pointers to getting clock max rate. The
 * details of these callbacks are described in the assigned function to these
 * func pointers.
 */
struct gops_clk {
#ifdef CONFIG_NVGPU_CLK_ARB
	int (*init_debugfs)(struct gk20a *g);
	int (*init_clk_support)(struct gk20a *g);
	void (*suspend_clk_support)(struct gk20a *g);
	u32 (*get_crystal_clk_hz)(struct gk20a *g);
	int (*clk_domain_get_f_points)(struct gk20a *g,
		u32 clkapidomain, u32 *pfpointscount,
		u16 *pfreqpointsinmhz);
	int (*clk_get_round_rate)(struct gk20a *g, u32 api_domain,
		unsigned long rate_target, unsigned long *rounded_rate);
	int (*get_clk_range)(struct gk20a *g, u32 api_domain,
		u16 *min_mhz, u16 *max_mhz);
	unsigned long (*measure_freq)(struct gk20a *g, u32 api_domain);
	u32 (*get_rate_cntr)(struct gk20a *g, struct namemap_cfg *c);
	u32 (*get_cntr_xbarclk_source)(struct gk20a *g);
	u32 (*get_cntr_sysclk_source)(struct gk20a *g);
	unsigned long (*get_rate)(struct gk20a *g, u32 api_domain);
	int (*set_rate)(struct gk20a *g, u32 api_domain, unsigned long rate);
	unsigned long (*get_fmax_at_vmin_safe)(struct gk20a *g);
	u32 (*get_ref_clock_rate)(struct gk20a *g);
	int (*predict_mv_at_hz_cur_tfloor)(struct clk_gk20a *clk,
		unsigned long rate);
#endif
	/**
	 * @brief Get max rate of gpu clock in Hertz.
	 *
	 * @param g [in]		gpu device struct pointer
	 * - The function does not perform validation of g parameter.
	 * @param api_domain [in]	clock domains type
	 * - Only CTRL_CLK_DOMAIN_GPCCLK value is valid.
	 *
	 * This routine helps to get max supported rate (in Hz) for given clock
	 * domain. Currently API only supports Graphics clock domain. Steps
	 * involved are
	 * - Pointer to struct \ref nvgpu_os_rmos which embeds \a g is retrieved
	 *   using function \ref nvgpu_os_rmos_from_gk20a
	 *   "nvgpu_os_rmos_from_gk20a(g)".
	 * - Then pointer to struct \ref nvgpu_power_ctx is obtained using
	 *   \ref nvgpu_os_rmos "nvgpu_os_rmos.context" in a local variable
	 *   ctx.
	 * - If \a api_domain CTRL_CLK_DOMAIN_GPCCLK is used, handle of
	 *   NvClockClock is obtained by calling \ref nvgpu_power_ctx_get_clk
	 *   "nvgpu_power_ctx_get_clk(ctx, 0, &gpu_clk)" in a local variable
	 *   gpu_clk. If the call fails return 0.
	 * - Finally, QNX-BSP external call
	 *   NvClockGetMaxDeviceClockFreq(gpu_clk, &freq) is made and the
	 *   frequency corresponding to \a api_domain CTRL_CLK_DOMAIN_GPCCLK is
	 *   returned in a local variable freq. If the call fails then set freq
	 *   to 0.
	 * - If the \a api_domain passed is not CTRL_CLK_DOMAIN_GPCCLK, then log
	 *   error that "unknown clock domain".
	 * - Return freq variable after typecasting to u64.
	 *
	 * @return 0 in case of failure and > 0 in case of success
	 */
	unsigned long (*get_maxrate)(struct gk20a *g, u32 api_domain);
#ifdef CONFIG_NVGPU_CLK_ARB
	int (*prepare_enable)(struct clk_gk20a *clk);
	void (*disable_unprepare)(struct clk_gk20a *clk);
	int (*get_voltage)(struct clk_gk20a *clk, u64 *val);
	int (*get_gpcclk_clock_counter)(struct clk_gk20a *clk, u64 *val);
	int (*pll_reg_write)(struct gk20a *g, u32 reg, u32 val);
	int (*get_pll_debug_data)(struct gk20a *g,
			struct nvgpu_clk_pll_debug_data *d);
	int (*mclk_init)(struct gk20a *g);
	void (*mclk_deinit)(struct gk20a *g);
	int (*mclk_change)(struct gk20a *g, u16 val);
	void (*get_change_seq_time)(struct gk20a *g, s64 *change_time);
	void (*change_host_clk_source)(struct gk20a *g);
	u32 (*clk_mon_init_domains)(struct gk20a *g);
	bool split_rail_support;
	bool support_pmgr_domain;
	bool support_lpwr_pg;
	int (*perf_pmu_vfe_load)(struct gk20a *g);
	bool support_vf_point;
	u8 lut_num_entries;
#endif
};

struct gops_clk_mon {
	int (*clk_mon_alloc_memory)(struct gk20a *g);
	bool (*clk_mon_check_master_fault_status)(struct gk20a *g);
	int (*clk_mon_check_status)(struct gk20a *g,
		u32 domain_mask);
	bool (*clk_mon_check_clk_good)(struct gk20a *g);
	bool (*clk_mon_check_pll_lock)(struct gk20a *g);
};

#endif /* NVGPU_GOPS_CLK_H */
