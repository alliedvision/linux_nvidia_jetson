/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef TEGRA_HWPM_DEBUGFS_H
#define TEGRA_HWPM_DEBUGFS_H

struct tegra_soc_hwpm;

#ifdef CONFIG_DEBUG_FS
void tegra_hwpm_debugfs_init(struct tegra_soc_hwpm *hwpm);
void tegra_hwpm_debugfs_deinit(struct tegra_soc_hwpm *hwpm);
#else
static inline void tegra_hwpm_debugfs_init(struct tegra_soc_hwpm *hwpm) {}
static inline void tegra_hwpm_debugfs_deinit(struct tegra_soc_hwpm *hwpm) {}
#endif  /* CONFIG_DEBUG_FS  */

#endif /* TEGRA_HWPM_DEBUGFS_H */
