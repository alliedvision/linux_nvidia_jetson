/*
 * drivers/misc/tegra-profiler/tegra23x_pmu_dsu.h
 *
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
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
 */

#ifndef __TEGRA23X_PMU_DSU_H
#define __TEGRA23X_PMU_DSU_H

struct quadd_event_source;

struct quadd_event_source *quadd_tegra23x_pmu_dsu_init(void);
void quadd_tegra23x_pmu_dsu_deinit(void);

#endif	/* __TEGRA23X_PMU_DSU_H */
