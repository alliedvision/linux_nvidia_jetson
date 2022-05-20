/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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
 *
 * tegra-soc-hwpm.h:
 * This is the header for the Tegra SOC HWPM driver.
 */

#ifndef TEGRA_SOC_HWPM_H
#define TEGRA_SOC_HWPM_H

//
//
// #include <linux/scatterlist.h>
//

// #include "tegra-soc-hwpm-log.h"
//

struct tegra_soc_hwpm;

#ifdef CONFIG_DEBUG_FS
void tegra_soc_hwpm_debugfs_init(struct tegra_soc_hwpm *hwpm);
void tegra_soc_hwpm_debugfs_deinit(struct tegra_soc_hwpm *hwpm);
#else
static inline void tegra_soc_hwpm_debugfs_init(struct tegra_soc_hwpm *hwpm) {}
static inline void tegra_soc_hwpm_debugfs_deinit(struct tegra_soc_hwpm *hwpm) {}
#endif  /* CONFIG_DEBUG_FS  */

#endif /* TEGRA_SOC_HWPM_H */
