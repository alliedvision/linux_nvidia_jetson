/*
 * Tegra Virtualized GPU Platform Interface
 *
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>

#include "os/linux/platform_gk20a.h"
#include "common/vgpu/clk_vgpu.h"
#include "vgpu_linux.h"

long vgpu_plat_clk_round_rate(struct device *dev, unsigned long rate)
{
	/* server will handle frequency rounding */
	return rate;
}

int vgpu_plat_clk_get_freqs(struct device *dev, unsigned long **freqs,
			int *num_freqs)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = platform->g;

	return vgpu_clk_get_freqs(g, freqs, num_freqs);
}

int vgpu_plat_clk_cap_rate(struct device *dev, unsigned long rate)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = platform->g;

	return vgpu_clk_cap_rate(g, rate);
}
