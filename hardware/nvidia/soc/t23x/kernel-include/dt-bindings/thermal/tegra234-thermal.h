/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
