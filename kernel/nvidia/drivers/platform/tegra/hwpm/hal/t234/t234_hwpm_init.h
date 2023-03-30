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

#ifndef T234_HWPM_INIT_H
#define T234_HWPM_INIT_H

struct tegra_soc_hwpm;

#ifdef CONFIG_TEGRA_T234_HWPM
int t234_hwpm_init_chip_info(struct tegra_soc_hwpm *hwpm);
#else
int t234_hwpm_init_chip_info(struct tegra_soc_hwpm *hwpm)
{
	return -EINVAL;
}
#endif

#endif /* T234_HWPM_INIT_H */
