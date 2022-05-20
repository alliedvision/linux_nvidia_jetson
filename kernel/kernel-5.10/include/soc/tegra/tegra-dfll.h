/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2014-2020, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef _TEGRA_DFLL_H_
#define _TEGRA_DFLL_H_

#include <linux/kernel.h>
#include <linux/platform_device.h>

enum tegra_dfll_thermal_type {
	TEGRA_DFLL_THERMAL_FLOOR = 0,
	TEGRA_DFLL_THERMAL_CAP,
};

struct tegra_dfll;

extern struct tegra_dfll *tegra_dfll_get_by_phandle(struct device_node *np,
						    const char *prop);
extern int tegra_dfll_update_thermal_index(struct tegra_dfll *td,
			enum tegra_dfll_thermal_type type,
			unsigned long new_index);
extern int tegra_dfll_get_thermal_index(struct tegra_dfll *td,
			enum tegra_dfll_thermal_type type);
extern int tegra_dfll_count_thermal_states(struct tegra_dfll *td,
			enum tegra_dfll_thermal_type type);
int tegra_dfll_set_external_floor_mv(int external_floor_mv);
u32 tegra_dfll_get_thermal_floor_mv(void);
u32 tegra_dfll_get_peak_thermal_floor_mv(void);
u32 tegra_dfll_get_thermal_cap_mv(void);
u32 tegra_dfll_get_min_millivolts(void);
struct rail_alignment *tegra_dfll_get_alignment(void);
const char *tegra_dfll_get_cvb_version(void);
#endif
