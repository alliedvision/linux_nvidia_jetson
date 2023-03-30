/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * tegra_asoc_utils.h - Definitions for Tegra DAS driver
 *
 * Author: Stephen Warren <swarren@nvidia.com>
 * Copyright (c) 2010,2012-2021, NVIDIA CORPORATION.  All rights reserved.
 */

#ifndef __TEGRA_ASOC_UTILS_H__
#define __TEGRA_ASOC_UTILS_H__

struct clk;
struct device;

enum tegra_asoc_utils_soc {
	TEGRA_ASOC_UTILS_SOC_TEGRA20,
	TEGRA_ASOC_UTILS_SOC_TEGRA30,
	TEGRA_ASOC_UTILS_SOC_TEGRA114,
	TEGRA_ASOC_UTILS_SOC_TEGRA124,
	TEGRA_ASOC_UTILS_SOC_TEGRA210,
	TEGRA_ASOC_UTILS_SOC_TEGRA186,
	TEGRA_ASOC_UTILS_SOC_TEGRA194,
	TEGRA_ASOC_UTILS_SOC_TEGRA234,
};

struct tegra_asoc_utils_data {
	struct device *dev;
	enum tegra_asoc_utils_soc soc;
	struct clk *clk_pll_a;
	struct clk *clk_pll_a_out0;
	struct clk *clk_cdev1;
	int set_baseclock;
	int set_mclk;
	unsigned int set_pll_out;
	unsigned int *pll_base_rate;
	unsigned int mclk_fs;
	bool fixed_pll;
};

int tegra_asoc_utils_set_rate(struct tegra_asoc_utils_data *data, int srate,
			      int mclk);
int tegra_asoc_utils_set_ac97_rate(struct tegra_asoc_utils_data *data);
int tegra_asoc_utils_set_tegra210_rate(struct tegra_asoc_utils_data *data,
				       unsigned int sample_rate,
				       unsigned int channels,
				       unsigned int sample_size);
int tegra_asoc_utils_clk_enable(struct tegra_asoc_utils_data *data);
void tegra_asoc_utils_clk_disable(struct tegra_asoc_utils_data *data);
int tegra_asoc_utils_init(struct tegra_asoc_utils_data *data,
			  struct device *dev);

#endif
