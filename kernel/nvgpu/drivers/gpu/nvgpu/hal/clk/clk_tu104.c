/*
 * TU104 Clocks
 *
 * Copyright (c) 2016-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include "os/linux/os_linux.h"
#endif

#include <nvgpu/kmem.h>
#include <nvgpu/io.h>
#include <nvgpu/list.h>
#include <nvgpu/pmu/clk/clk.h>
#include <nvgpu/soc.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/pmu/perf.h>
#include <nvgpu/clk_arb.h>
#include <nvgpu/pmu/volt.h>
#include <nvgpu/hw/tu104/hw_trim_tu104.h>

#include "clk_tu104.h"



#define CLK_NAMEMAP_INDEX_GPCCLK	0x00
#define CLK_NAMEMAP_INDEX_XBARCLK	0x02
#define CLK_NAMEMAP_INDEX_SYSCLK	0x07	/* SYSPLL */
#define CLK_NAMEMAP_INDEX_DRAMCLK	0x20	/* DRAMPLL */

#define CLK_DEFAULT_CNTRL_SETTLE_RETRIES 10
#define CLK_DEFAULT_CNTRL_SETTLE_USECS   5

#define XTAL_CNTR_CLKS		27000	/* 1000usec at 27KHz XTAL */
#define XTAL_CNTR_DELAY		10000	/* we need acuracy up to the 10ms   */
#define XTAL_SCALE_TO_KHZ	1
#define NUM_NAMEMAPS    (3U)
#define XTAL4X_KHZ 108000
#define BOOT_GPCCLK_MHZ 645U

#ifdef CONFIG_NVGPU_CLK_ARB
u32 tu104_crystal_clk_hz(struct gk20a *g)
{
	return (XTAL4X_KHZ * 1000);
}

unsigned long tu104_clk_measure_freq(struct gk20a *g, u32 api_domain)
{
	struct clk_gk20a *clk = &g->clk;
	u32 freq_khz;
	u32 i;
	struct namemap_cfg *c = NULL;

	for (i = 0; i < clk->namemap_num; i++) {
		if (api_domain == clk->namemap_xlat_table[i]) {
			c = &clk->clk_namemap[i];
			break;
		}
	}

	if (c == NULL) {
		return 0;
	}
	if (c->is_counter != 0U) {
		freq_khz = c->scale * tu104_get_rate_cntr(g, c);
	} else {
		freq_khz = 0U;
		 /* TODO: PLL read */
	}

	/* Convert to HZ */
	return (freq_khz * 1000UL);
}

static void nvgpu_gpu_gpcclk_counter_init(struct gk20a *g)
{
	u32 data;

	data = gk20a_readl(g, trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_r());
	data |= trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_update_cycle_init_f() |
		trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_cont_update_enabled_f() |
		trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_start_count_disabled_f() |
		trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_reset_asserted_f() |
		trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_source_gpcclk_noeg_f();
	gk20a_writel(g,trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_r(), data);
	/*
	* Based on the clock counter design, it takes 16 clock cycles of the
	* "counted clock" for the counter to completely reset. Considering
	* 27MHz as the slowest clock during boot time, delay of 16/27us (~1us)
	* should be sufficient. See Bug 1953217.
	*/
	nvgpu_udelay(1);
	data = gk20a_readl(g, trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_r());
	data = set_field(data, trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_reset_m(),
			trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_reset_deasserted_f());
	gk20a_writel(g,trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_r(), data);
	/*
	* Enable clock counter.
	* Note : Need to write un-reset and enable signal in different
	* register writes as the source (register block) and destination
	* (FR counter) are on the same clock and far away from each other,
	* so the signals can not reach in the same clock cycle hence some
	* delay is required between signals.
	*/
	data = gk20a_readl(g, trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_r());
	data |= trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_start_count_enabled_f();
	gk20a_writel(g,trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_r(), data);
}

u32 tu104_clk_get_cntr_sysclk_source(struct gk20a *g)
{
	return trim_sys_fr_clk_cntr_sysclk_cfg_source_sys_noeg_f();
}

static void nvgpu_gpu_sysclk_counter_init(struct gk20a *g)
{
	u32 data;

	data = gk20a_readl(g, trim_sys_fr_clk_cntr_sysclk_cfg_r());
	data |= trim_sys_fr_clk_cntr_sysclk_cfg_update_cycle_init_f() |
		trim_sys_fr_clk_cntr_sysclk_cfg_cont_update_enabled_f() |
		trim_sys_fr_clk_cntr_sysclk_cfg_start_count_disabled_f() |
		trim_sys_fr_clk_cntr_sysclk_cfg_reset_asserted_f() |
		g->ops.clk.get_cntr_sysclk_source(g);
	gk20a_writel(g,trim_sys_fr_clk_cntr_sysclk_cfg_r(), data);

	nvgpu_udelay(1);

	data = gk20a_readl(g, trim_sys_fr_clk_cntr_sysclk_cfg_r());
	data = set_field(data, trim_sys_fr_clk_cntr_sysclk_cfg_reset_m(),
			trim_sys_fr_clk_cntr_sysclk_cfg_reset_deasserted_f());
	gk20a_writel(g,trim_sys_fr_clk_cntr_sysclk_cfg_r(), data);

	data = gk20a_readl(g, trim_sys_fr_clk_cntr_sysclk_cfg_r());
	data |= trim_sys_fr_clk_cntr_sysclk_cfg_start_count_enabled_f();
	gk20a_writel(g,trim_sys_fr_clk_cntr_sysclk_cfg_r(), data);
}

u32 tu104_clk_get_cntr_xbarclk_source(struct gk20a *g)
{
	return trim_sys_fll_fr_clk_cntr_xbarclk_cfg_source_xbar_nobg_f();
}

static void nvgpu_gpu_xbarclk_counter_init(struct gk20a *g)
{
	u32 data;

	data = gk20a_readl(g, trim_sys_fll_fr_clk_cntr_xbarclk_cfg_r());
	data |= trim_sys_fll_fr_clk_cntr_xbarclk_cfg_update_cycle_init_f() |
		trim_sys_fll_fr_clk_cntr_xbarclk_cfg_cont_update_enabled_f() |
		trim_sys_fll_fr_clk_cntr_xbarclk_cfg_start_count_disabled_f() |
		trim_sys_fll_fr_clk_cntr_xbarclk_cfg_reset_asserted_f() |
		g->ops.clk.get_cntr_xbarclk_source(g);
	gk20a_writel(g,trim_sys_fll_fr_clk_cntr_xbarclk_cfg_r(), data);

	nvgpu_udelay(1);

	data = gk20a_readl(g, trim_sys_fll_fr_clk_cntr_xbarclk_cfg_r());
	data = set_field(data, trim_sys_fll_fr_clk_cntr_xbarclk_cfg_reset_m(),
			trim_sys_fll_fr_clk_cntr_xbarclk_cfg_reset_deasserted_f());
	gk20a_writel(g,trim_sys_fll_fr_clk_cntr_xbarclk_cfg_r(), data);

	data = gk20a_readl(g, trim_sys_fll_fr_clk_cntr_xbarclk_cfg_r());
	data |= trim_sys_fll_fr_clk_cntr_xbarclk_cfg_start_count_enabled_f();
	gk20a_writel(g,trim_sys_fll_fr_clk_cntr_xbarclk_cfg_r(), data);
}

int tu104_init_clk_support(struct gk20a *g)
{
	struct clk_gk20a *clk = &g->clk;


	nvgpu_log_fn(g, " ");

	nvgpu_mutex_init(&clk->clk_mutex);

	clk->clk_namemap = (struct namemap_cfg *)
		nvgpu_kzalloc(g, sizeof(struct namemap_cfg) * NUM_NAMEMAPS);

	if (clk->clk_namemap == NULL) {
		nvgpu_mutex_destroy(&clk->clk_mutex);
		return -ENOMEM;
	}

	clk->namemap_xlat_table = nvgpu_kcalloc(g, NUM_NAMEMAPS, sizeof(u32));

	if (clk->namemap_xlat_table == NULL) {
		nvgpu_kfree(g, clk->clk_namemap);
		nvgpu_mutex_destroy(&clk->clk_mutex);
		return -ENOMEM;
	}

	clk->clk_namemap[0] = (struct namemap_cfg) {
		.namemap = CLK_NAMEMAP_INDEX_GPCCLK,
		.is_enable = 1,
		.is_counter = 1,
		.g = g,
		.cntr = {
			.reg_ctrl_addr = trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_r(),
			.reg_ctrl_idx  = trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_source_gpcclk_noeg_f(),
			.reg_cntr_addr[0] = trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cnt0_r(),
			.reg_cntr_addr[1] = trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cnt1_r()
		},
		.name = "gpcclk",
		.scale = 1
	};

	nvgpu_gpu_gpcclk_counter_init(g);
	clk->namemap_xlat_table[0] = CTRL_CLK_DOMAIN_GPCCLK;

	clk->clk_namemap[1] = (struct namemap_cfg) {
		.namemap = CLK_NAMEMAP_INDEX_SYSCLK,
		.is_enable = 1,
		.is_counter = 1,
		.g = g,
		.cntr = {
			.reg_ctrl_addr = trim_sys_fr_clk_cntr_sysclk_cfg_r(),
			.reg_ctrl_idx  = g->ops.clk.get_cntr_sysclk_source(g),
			.reg_cntr_addr[0] = trim_sys_fr_clk_cntr_sysclk_cntr0_r(),
			.reg_cntr_addr[1] = trim_sys_fr_clk_cntr_sysclk_cntr1_r()
		},
		.name = "sysclk",
		.scale = 1
	};

	nvgpu_gpu_sysclk_counter_init(g);
	clk->namemap_xlat_table[1] = CTRL_CLK_DOMAIN_SYSCLK;

	clk->clk_namemap[2] = (struct namemap_cfg) {
		.namemap = CLK_NAMEMAP_INDEX_XBARCLK,
		.is_enable = 1,
		.is_counter = 1,
		.g = g,
		.cntr = {
			.reg_ctrl_addr = trim_sys_fll_fr_clk_cntr_xbarclk_cfg_r(),
			.reg_ctrl_idx  = g->ops.clk.get_cntr_xbarclk_source(g),
			.reg_cntr_addr[0] = trim_sys_fll_fr_clk_cntr_xbarclk_cntr0_r(),
			.reg_cntr_addr[1] = trim_sys_fll_fr_clk_cntr_xbarclk_cntr1_r()
		},
		.name = "xbarclk",
		.scale = 1
	};

	nvgpu_gpu_xbarclk_counter_init(g);
	clk->namemap_xlat_table[2] = CTRL_CLK_DOMAIN_XBARCLK;

	clk->namemap_num = NUM_NAMEMAPS;

	clk->g = g;

	return 0;
}

u32 tu104_get_rate_cntr(struct gk20a *g, struct namemap_cfg *c) {
#ifdef CONFIG_NVGPU_NON_FUSA
	u32 cntr = 0;
	u64 cntr_start = 0;
	u64 cntr_stop = 0;
	u64 start_time, stop_time;
	const int max_iterations = 3;
	int i = 0;

	struct clk_gk20a *clk = &g->clk;

	if ((c == NULL) || (c->cntr.reg_ctrl_addr == 0U) ||
		(c->cntr.reg_cntr_addr[0] == 0U) ||
		(c->cntr.reg_cntr_addr[1]) == 0U) {
			return 0;
	}

	nvgpu_mutex_acquire(&clk->clk_mutex);

	for (i = 0; i < max_iterations; i++) {
		/*
		 * Read the counter values. Counter is 36 bits, 32
		 * bits on addr[0] and 4 lsb on addr[1] others zero.
		 */
		cntr_start = (u64)nvgpu_readl(g,
				c->cntr.reg_cntr_addr[0]);
		cntr_start += ((u64)nvgpu_readl(g,
				c->cntr.reg_cntr_addr[1]) << 32);
		start_time = (u64)nvgpu_hr_timestamp_us();
		nvgpu_udelay(XTAL_CNTR_DELAY);
		stop_time = (u64)nvgpu_hr_timestamp_us();
		cntr_stop = (u64)nvgpu_readl(g,
				c->cntr.reg_cntr_addr[0]);
		cntr_stop += ((u64)nvgpu_readl(g,
				c->cntr.reg_cntr_addr[1]) << 32);

		if (cntr_stop > cntr_start) {
			/*
			 * Calculate the difference with Acutal time
			 * and convert to KHz
			 */
			cntr = (u32)(((cntr_stop - cntr_start) /
				(stop_time - start_time)) * 1000U);
			nvgpu_mutex_release(&clk->clk_mutex);
			return cntr;
		}
		/* Else wrap around detected. Hence, retry. */
	}

	nvgpu_mutex_release(&clk->clk_mutex);
#endif
	/* too many iterations, bail out */
	nvgpu_err(g, "failed to get clk rate");
	return -EBUSY;
}

int tu104_clk_domain_get_f_points(
	struct gk20a *g,
	u32 clkapidomain,
	u32 *pfpointscount,
	u16 *pfreqpointsinmhz)
{
	int status = -EINVAL;

	if (pfpointscount == NULL) {
		return -EINVAL;
	}

	if ((pfreqpointsinmhz == NULL) && (*pfpointscount != 0U)) {
		return -EINVAL;
	}

	status = nvgpu_pmu_clk_domain_get_f_points(g,
			clkapidomain, pfpointscount, pfreqpointsinmhz);
	if (status != 0) {
		nvgpu_err(g, "Unable to get frequency points");
	}
	return status;
}

void tu104_suspend_clk_support(struct gk20a *g)
{
	nvgpu_mutex_destroy(&g->clk.clk_mutex);
}

unsigned long tu104_clk_maxrate(struct gk20a *g, u32 api_domain)
{
	u16 min_mhz = 0, max_mhz = 0;
	int status;

	if (nvgpu_is_enabled(g, NVGPU_PMU_PSTATE)) {
		status = nvgpu_clk_arb_get_arbiter_clk_range(g, api_domain,
				&min_mhz, &max_mhz);
		if (status != 0) {
			nvgpu_err(g, "failed to fetch clock range");
			return 0U;
		}
	} else {
		if (api_domain == NVGPU_CLK_DOMAIN_GPCCLK) {
			max_mhz = BOOT_GPCCLK_MHZ;
		}
	}

	return (max_mhz * 1000UL * 1000UL);
}

void tu104_get_change_seq_time(struct gk20a *g, s64 *change_time)
{
	nvgpu_perf_change_seq_execute_time(g, change_time);
}
#endif
void tu104_change_host_clk_source(struct gk20a *g)
{
	nvgpu_writel(g, trim_sys_ind_clk_sys_core_clksrc_r(),
			trim_sys_ind_clk_sys_core_clksrc_hostclk_fll_f());
}
