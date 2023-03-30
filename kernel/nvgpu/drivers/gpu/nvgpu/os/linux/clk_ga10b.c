/*
 * Linux clock support for ga10b
 *
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/clk.h>

#include "clk.h"
#include "os_linux.h"
#include "platform_gk20a.h"

#include <nvgpu/gk20a.h>
#include <nvgpu/clk_arb.h>
#include <nvgpu/gr/gr_utils.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/pmu/clk/clk.h>

/*
 * GA10B clock list:
 * platform->clk[0]- sysclk
 * For GPU Full config:
 * platform->clk[1] - gpc0 clk
 * platform->clk[2] - gpc1 clk
 * platform->clk[3] - fuse clk
 * For GPU GPC Floor-swept config:
 * platform->clk[1] - Active gpc(gpc0/gpc1) clk
 * platform->clk[2] - fuse clk
 */

/*
 * GPU clock policy for ga10b:
 * All sys, gpc0 and gpc1 clk are at same rate.
 * So, for clock set_rate, change all clocks for
 * any clock rate change request.
 */

/*
 * PWRCLK is used for pmu runs at fixed rate 204MHZ in ga10b
 * PWRCLK is enabled once gpu out of reset. CCF is not
 * supporting any clock set/get calls for PWRCLK. To support
 * legacy code, nvgpu driver only supporting clk_get_rate by
 * returning fixed 204MHz rate
 */
#define NVGPU_GA10B_PWRCLK_RATE 204000000UL;

unsigned long nvgpu_ga10b_linux_clk_get_rate(
					struct gk20a *g, u32 api_domain)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev_from_gk20a(g));
	unsigned long ret;

	switch (api_domain) {
	case CTRL_CLK_DOMAIN_SYSCLK:
	case CTRL_CLK_DOMAIN_GPCCLK:
		ret = clk_get_rate(platform->clk[0]);
		break;
	case CTRL_CLK_DOMAIN_PWRCLK:
		/* power domain is at fixed clock */
		ret = NVGPU_GA10B_PWRCLK_RATE;
		break;
	default:
		nvgpu_err(g, "unknown clock: %u", api_domain);
		ret = 0;
		break;
	}

	return ret;
}

int nvgpu_ga10b_linux_clk_set_rate(struct gk20a *g,
				     u32 api_domain, unsigned long rate)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev_from_gk20a(g));
	struct nvgpu_gr_config *gr_config = nvgpu_gr_get_config_ptr(g);
	u32 gpc_count = nvgpu_gr_config_get_gpc_count(gr_config);
	int ret;

	switch (api_domain) {
	case CTRL_CLK_DOMAIN_GPCCLK:
	case CTRL_CLK_DOMAIN_SYSCLK:
		ret = clk_set_rate(platform->clk[0], rate);
		ret = clk_set_rate(platform->clk[1], rate);
		/* Set second gpcclk for full-config */
		if (gpc_count == 2U)
			ret = clk_set_rate(platform->clk[2], rate);
		break;
	case CTRL_CLK_DOMAIN_PWRCLK:
		nvgpu_err(g, "unsupported operation: %u", api_domain);
		ret = -EINVAL;
		break;
	default:
		nvgpu_err(g, "unknown clock: %u", api_domain);
		ret = -EINVAL;
		break;
	}

	return ret;
}
