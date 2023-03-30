/*
 * TU104 Clocks Monitor
 *
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/kmem.h>
#include <nvgpu/io.h>
#include <nvgpu/list.h>
#include <nvgpu/soc.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/clk_mon.h>
#include <nvgpu/hw/tu104/hw_trim_tu104.h>

#include "clk_mon_tu104.h"
/**
 * Mapping between the clk domain and the various clock monitor registers
 * The rows represent clock domains starting from index 0 and column represent
 * the various registers each domain has, non available domains are set to 0
 * for easy accessing, refer nvgpu_clk_mon_init_domains() for valid domains.
 */
static  u32 clock_mon_map_tu104[CLK_CLOCK_MON_DOMAIN_COUNT]
				[CLK_CLOCK_MON_REG_TYPE_COUNT] = {
	{
		trim_gpcclk_fault_threshold_high_r(),
		trim_gpcclk_fault_threshold_low_r(),
		trim_gpcclk_fault_status_r(),
		trim_gpcclk_fault_priv_level_mask_r(),
	},
	{
		trim_xbarclk_fault_threshold_high_r(),
		trim_xbarclk_fault_threshold_low_r(),
		trim_xbarclk_fault_status_r(),
		trim_xbarclk_fault_priv_level_mask_r(),
	},
	{
		trim_sysclk_fault_threshold_high_r(),
		trim_sysclk_fault_threshold_low_r(),
		trim_sysclk_fault_status_r(),
		trim_sysclk_fault_priv_level_mask_r(),
	},
	{
		trim_hubclk_fault_threshold_high_r(),
		trim_hubclk_fault_threshold_low_r(),
		trim_hubclk_fault_status_r(),
		trim_hubclk_fault_priv_level_mask_r(),
	},
	{
		trim_dramclk_fault_threshold_high_r(),
		trim_dramclk_fault_threshold_low_r(),
		trim_dramclk_fault_status_r(),
		trim_dramclk_fault_priv_level_mask_r(),
	},
	{
		trim_hostclk_fault_threshold_high_r(),
		trim_hostclk_fault_threshold_low_r(),
		trim_hostclk_fault_status_r(),
		trim_hostclk_fault_priv_level_mask_r(),
	},
	{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
	{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
	{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
	{
		trim_utilsclk_fault_threshold_high_r(),
		trim_utilsclk_fault_threshold_low_r(),
		trim_utilsclk_fault_status_r(),
		trim_utilsclk_fault_priv_level_mask_r(),
	},
	{
		trim_pwrclk_fault_threshold_high_r(),
		trim_pwrclk_fault_threshold_low_r(),
		trim_pwrclk_fault_status_r(),
		trim_pwrclk_fault_priv_level_mask_r(),
	},
	{
		trim_nvdclk_fault_threshold_high_r(),
		trim_nvdclk_fault_threshold_low_r(),
		trim_nvdclk_fault_status_r(),
		trim_nvdclk_fault_priv_level_mask_r(),
	},
	{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
	{
		trim_xclk_fault_threshold_high_r(),
		trim_xclk_fault_threshold_low_r(),
		trim_xclk_fault_status_r(),
		trim_xclk_fault_priv_level_mask_r(),
	},
	{
		trim_nvl_commonclk_fault_threshold_high_r(),
		trim_nvl_commonclk_fault_threshold_low_r(),
		trim_nvl_commonclk_fault_status_r(),
		trim_nvl_commonclk_fault_priv_level_mask_r(),
	},
	{
		trim_pex_refclk_fault_threshold_high_r(),
		trim_pex_refclk_fault_threshold_low_r(),
		trim_pex_refclk_fault_status_r(),
		trim_pex_refclk_fault_priv_level_mask_r(),
	},
	{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}
};

static u32 nvgpu_check_for_dc_fault(u32 data)
{
	return (trim_fault_status_dc_v(data) ==
		trim_fault_status_dc_true_v()) ?
				trim_fault_status_dc_m() : 0U;
}

static u32 nvgpu_check_for_lower_threshold_fault(u32 data)
{
	return (trim_fault_status_lower_threshold_v(data) ==
		trim_fault_status_lower_threshold_true_v()) ?
				trim_fault_status_lower_threshold_m() : 0U;
}

static u32 nvgpu_check_for_higher_threshold_fault(u32 data)
{
	return (trim_fault_status_higher_threshold_v(data) ==
		trim_fault_status_higher_threshold_true_v()) ?
				trim_fault_status_higher_threshold_m() : 0U;
}

static u32 nvgpu_check_for_overflow_err(u32 data)
{
	return (trim_fault_status_overflow_v(data) ==
		trim_fault_status_overflow_true_v()) ?
				trim_fault_status_overflow_m() : 0U;
}

static int nvgpu_clk_mon_get_fault(struct gk20a *g, u32 i, u32 data,
		struct clk_domains_mon_status_params *clk_mon_status)
{
	u32 reg_address;
	int status = 0;

	/* Fields for faults are same for all clock domains */
	clk_mon_status->clk_mon_list[i].clk_domain_fault_status =
		((nvgpu_check_for_dc_fault(data)) |
		(nvgpu_check_for_lower_threshold_fault(data)) |
		(nvgpu_check_for_higher_threshold_fault(data)) |
		(nvgpu_check_for_overflow_err(data)));
	nvgpu_err(g, "FMON faulted domain 0x%x value 0x%x",
		clk_mon_status->clk_mon_list[i].clk_api_domain,
		clk_mon_status->clk_mon_list[i].
		clk_domain_fault_status);

	/* Get the low threshold limit */
	reg_address = clock_mon_map_tu104[i][FMON_THRESHOLD_LOW];
	data = nvgpu_readl(g, reg_address);
	clk_mon_status->clk_mon_list[i].low_threshold =
		trim_fault_threshold_low_count_v(data);

	/* Get the high threshold limit */
	reg_address = clock_mon_map_tu104[i][FMON_THRESHOLD_HIGH];
	data = nvgpu_readl(g, reg_address);
	clk_mon_status->clk_mon_list[i].high_threshold =
		trim_fault_threshold_high_count_v(data);

	return status;
}

bool tu104_clk_mon_check_master_fault_status(struct gk20a *g)
{
	u32 fmon_master_status = nvgpu_readl(g, trim_fmon_master_status_r());

	if (trim_fmon_master_status_fault_out_v(fmon_master_status) ==
			trim_fmon_master_status_fault_out_true_v()) {
		return true;
	}
	return false;
}

int nvgpu_clk_mon_alloc_memory(struct gk20a *g)
{
	struct clk_gk20a *clk = &g->clk;

	/* If already allocated, do not re-allocate */
	if (clk->clk_mon_status != NULL) {
		return 0;
	}

	clk->clk_mon_status = nvgpu_kzalloc(g,
			sizeof(struct clk_domains_mon_status_params));
	if (clk->clk_mon_status == NULL) {
		return -ENOMEM;
	}

	return 0;
}

int tu104_clk_mon_check_status(struct gk20a *g, u32 domain_mask)
{
	u32 reg_address, bit_pos;
	u32 data;
	int status;
	struct clk_domains_mon_status_params *clk_mon_status;

	clk_mon_status = g->clk.clk_mon_status;
	clk_mon_status->clk_mon_domain_mask = domain_mask;
	/*
	 * Parse through each domain and check for faults, each bit set
	 * represents a domain here
	 */
	for (bit_pos = 0U; bit_pos < (sizeof(domain_mask) * BITS_PER_BYTE);
			bit_pos++) {
		if (nvgpu_test_bit(bit_pos, (void *)&domain_mask)) {
			clk_mon_status->clk_mon_list[bit_pos].clk_api_domain =
					BIT(bit_pos);

			reg_address = clock_mon_map_tu104[bit_pos]
					[FMON_FAULT_STATUS];
			data = nvgpu_readl(g, reg_address);

			clk_mon_status->clk_mon_list[bit_pos].
				clk_domain_fault_status = 0U;
			/* Check FMON fault status, field is same for all */
			if (trim_fault_status_fault_out_v(data) ==
				trim_fault_status_fault_out_true_v()) {
				status = nvgpu_clk_mon_get_fault(g, bit_pos,
						data, clk_mon_status);
				if (status != 0) {
					nvgpu_err(g, "Failed to get status");
					return -EINVAL;
				}
			}
		}
	}
	return 0;
}

bool tu104_clk_mon_check_clk_good(struct gk20a *g)
{
	u32 clk_status = nvgpu_readl(g, trim_xtal4x_cfg5_r());

	if (trim_xtal4x_cfg5_curr_state_v(clk_status) !=
			trim_xtal4x_cfg5_curr_state_good_v()) {
		return true;
	}
	return false;
}

bool tu104_clk_mon_check_pll_lock(struct gk20a *g)
{
	u32 clk_status = nvgpu_readl(g, trim_xtal4x_cfg_r());

	/* check xtal4 */
	if (trim_xtal4x_cfg_pll_lock_v(clk_status) !=
			trim_xtal4x_cfg_pll_lock_true_v()) {
		return true;
	}

	/* check mem pll */
	clk_status = nvgpu_readl(g, trim_mem_pll_status_r());

	if (trim_mem_pll_status_dram_curr_state_v(clk_status) !=
			trim_mem_pll_status_dram_curr_state_good_v()) {
		return true;
	}
	if (trim_mem_pll_status_refm_curr_state_v(clk_status) !=
			trim_mem_pll_status_refm_curr_state_good_v()) {
		return true;
	}

	/* check sppll0,1 */
	clk_status = nvgpu_readl(g, trim_sppll0_cfg_r());

	if (trim_sppll0_cfg_curr_state_v(clk_status) !=
			trim_sppll0_cfg_curr_state_good_v()) {
		return true;
	}
	clk_status = nvgpu_readl(g, trim_sppll1_cfg_r());

	if (trim_sppll1_cfg_curr_state_v(clk_status) !=
			trim_sppll1_cfg_curr_state_good_v()) {
		return true;
	}
	return false;
}

