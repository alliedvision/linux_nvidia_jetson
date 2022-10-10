/*
 * GP10B Platform (SoC) Interface
 *
 * Copyright (c) 2014-2022, NVIDIA Corporation.  All rights reserved.
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

#ifndef _GP10B_PLATFORM_H_
#define _GP10B_PLATFORM_H_

struct device;
struct gk20a_platform_clk {
	char *name;
	unsigned long default_rate;
};

/* Select every GP10B_FREQ_SELECT_STEP'th frequency from h/w table */
#define GP10B_FREQ_SELECT_STEP  8

/* Max number of freq supported in h/w */
#define GP10B_MAX_SUPPORTED_FREQS 120

/* Allow limited set of frequencies to be available */
#define GP10B_NUM_SUPPORTED_FREQS ((GP10B_MAX_SUPPORTED_FREQS) / (GP10B_FREQ_SELECT_STEP))

#define TEGRA_GP10B_BW_PER_FREQ 64
#define TEGRA_DDR4_BW_PER_FREQ 16

#define EMC_BW_RATIO  (TEGRA_GP10B_BW_PER_FREQ / TEGRA_DDR4_BW_PER_FREQ)

extern unsigned long gp10b_freq_table[GP10B_NUM_SUPPORTED_FREQS];

void gp10b_tegra_clks_control(struct device *dev, bool enable);
int gp10b_tegra_get_clocks(struct device *dev);
int gp10b_tegra_reset_assert(struct device *dev);
int gp10b_tegra_reset_deassert(struct device *dev);
void gp10b_tegra_scale_init(struct device *dev);
long gp10b_round_clk_rate(struct device *dev, unsigned long rate);
int gp10b_clk_get_freqs(struct device *dev,
			unsigned long **freqs, int *num_freqs);
void gp10b_tegra_prescale(struct device *dev);
void gp10b_tegra_postscale(struct device *pdev, unsigned long freq);
int gp10b_tegra_acquire_platform_clocks(struct device *dev,
		struct gk20a_platform_clk *clk_entries,
		unsigned int num_clk_entries);
#endif
