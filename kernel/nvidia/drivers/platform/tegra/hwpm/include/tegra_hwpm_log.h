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

#ifndef TEGRA_HWPM_LOG_H
#define TEGRA_HWPM_LOG_H

#include <linux/bits.h>

#define TEGRA_SOC_HWPM_MODULE_NAME	"tegra-soc-hwpm"

enum tegra_soc_hwpm_log_type {
	TEGRA_HWPM_ERROR,	/* Error prints */
	TEGRA_HWPM_DEBUG,	/* Debug prints */
};

#define TEGRA_HWPM_DEFAULT_DBG_MASK	(0)
/* Primary info prints */
#define hwpm_info			BIT(0)
/* Trace function execution */
#define hwpm_fn				BIT(1)
/* Log register accesses */
#define hwpm_register			BIT(2)
/* General verbose prints */
#define hwpm_verbose			BIT(3)
/* Driver init specific verbose prints */
#define hwpm_dbg_driver_init		BIT(4)
/* IP register specific verbose prints */
#define hwpm_dbg_ip_register		BIT(5)
/* Device info specific verbose prints */
#define hwpm_dbg_device_info		BIT(6)
/* Floorsweep info specific verbose prints */
#define hwpm_dbg_floorsweep_info	BIT(7)
/* Resource info specific verbose prints */
#define hwpm_dbg_resource_info		BIT(8)
/* Reserve resource specific verbose prints */
#define hwpm_dbg_reserve_resource	BIT(9)
/* Release resource specific verbose prints */
#define hwpm_dbg_release_resource	BIT(10)
/* Alloc PMA stream specific verbose prints */
#define hwpm_dbg_alloc_pma_stream	BIT(11)
/* Bind operation specific verbose prints */
#define hwpm_dbg_bind			BIT(12)
/* Allowlist specific verbose prints */
#define hwpm_dbg_allowlist		BIT(13)
/* Regops specific verbose prints */
#define hwpm_dbg_regops			BIT(14)
/* Get Put pointer specific verbose prints */
#define hwpm_dbg_update_get_put		BIT(15)
/* Driver release specific verbose prints */
#define hwpm_dbg_driver_release		BIT(16)


#define tegra_hwpm_err(hwpm, fmt, arg...)				\
	tegra_hwpm_err_impl(hwpm, __func__, __LINE__, fmt, ##arg)
#define tegra_hwpm_dbg(hwpm, dbg_mask, fmt, arg...)			\
	tegra_hwpm_dbg_impl(hwpm, dbg_mask, __func__, __LINE__, fmt, ##arg)
#define tegra_hwpm_fn(hwpm, fmt, arg...)				\
	tegra_hwpm_dbg_impl(hwpm, hwpm_fn, __func__, __LINE__, fmt, ##arg)

struct tegra_soc_hwpm;

void tegra_hwpm_err_impl(struct tegra_soc_hwpm *hwpm,
	const char *func, int line, const char *fmt, ...);
void tegra_hwpm_dbg_impl(struct tegra_soc_hwpm *hwpm,
	u32 dbg_mask, const char *func, int line, const char *fmt, ...);

#endif /* TEGRA_HWPM_LOG_H */
