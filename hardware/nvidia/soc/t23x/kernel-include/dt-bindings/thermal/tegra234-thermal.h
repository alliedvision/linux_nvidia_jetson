/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef BPMP_ABI_MACH_T234_THERMAL_H
#define BPMP_ABI_MACH_T234_THERMAL_H

/**
 * @file
 * @defgroup bpmp_thermal_ids Thermal Zone ID's
 * @{
 */
#define TEGRA234_THERMAL_ZONE_CPU	0U
#define TEGRA234_THERMAL_ZONE_GPU	1U
#define TEGRA234_THERMAL_ZONE_CV0	2U
#define TEGRA234_THERMAL_ZONE_CV1	3U
#define TEGRA234_THERMAL_ZONE_CV2	4U
#define TEGRA234_THERMAL_ZONE_SOC0	5U
#define TEGRA234_THERMAL_ZONE_SOC1	6U
#define TEGRA234_THERMAL_ZONE_SOC2	7U
#define TEGRA234_THERMAL_ZONE_TJ_MAX	8U
#define TEGRA234_THERMAL_ZONE_COUNT	9U
/** @} */

/**
 * Caution: The use of THERMAL_ZONE_XXX identifiers is
 * discouraged. Their value depend on a version of Tegra chip.
 */
#define THERMAL_ZONE_CPU	TEGRA234_THERMAL_ZONE_CPU
#define THERMAL_ZONE_GPU	TEGRA234_THERMAL_ZONE_GPU
#define THERMAL_ZONE_CV0	TEGRA234_THERMAL_ZONE_CV0
#define THERMAL_ZONE_CV1	TEGRA234_THERMAL_ZONE_CV1
#define THERMAL_ZONE_CV2	TEGRA234_THERMAL_ZONE_CV2
#define THERMAL_ZONE_SOC0	TEGRA234_THERMAL_ZONE_SOC0
#define THERMAL_ZONE_SOC1	TEGRA234_THERMAL_ZONE_SOC1
#define THERMAL_ZONE_SOC2	TEGRA234_THERMAL_ZONE_SOC2
#define THERMAL_ZONE_TJ_MAX	TEGRA234_THERMAL_ZONE_TJ_MAX
#define THERMAL_ZONE_COUNT	TEGRA234_THERMAL_ZONE_COUNT

#endif
