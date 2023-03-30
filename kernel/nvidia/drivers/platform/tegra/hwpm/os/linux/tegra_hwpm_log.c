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

#include <linux/kernel.h>

#include <tegra_hwpm.h>
#include <tegra_hwpm_log.h>

#define LOG_BUF_SIZE	160

static void tegra_hwpm_print(const char *func, int line,
	int type, const char *log)
{
	switch (type) {
	case TEGRA_HWPM_ERROR:
		pr_err(TEGRA_SOC_HWPM_MODULE_NAME ": %s: %d: ERROR: %s\n",
			func, line, log);
		break;
	case TEGRA_HWPM_DEBUG:
		pr_info(TEGRA_SOC_HWPM_MODULE_NAME ": %s: %d: DEBUG: %s\n",
			func, line, log);
		break;
	}
}

void tegra_hwpm_err_impl(struct tegra_soc_hwpm *hwpm,
	const char *func, int line, const char *fmt, ...)
{
	char log[LOG_BUF_SIZE];
	va_list args;

	va_start(args, fmt);
	(void) vsnprintf(log, LOG_BUF_SIZE, fmt, args);
	va_end(args);

	tegra_hwpm_print(func, line, TEGRA_HWPM_ERROR, log);
}

void tegra_hwpm_dbg_impl(struct tegra_soc_hwpm *hwpm,
	u32 dbg_mask, const char *func, int line, const char *fmt, ...)
{
	char log[LOG_BUF_SIZE];
	va_list args;

	if ((hwpm == NULL) || ((dbg_mask & hwpm->dbg_mask) == 0)) {
		return;
	}

	va_start(args, fmt);
	(void) vsnprintf(log, LOG_BUF_SIZE, fmt, args);
	va_end(args);

	tegra_hwpm_print(func, line, TEGRA_HWPM_DEBUG, log);
}
