/*
 * tegra-soc-hwpm-log.c:
 * This file adds logging APIs for the Tegra SOC HWPM driver.
 *
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
 */

#include <linux/kernel.h>

#include "tegra-soc-hwpm.h"
#include "tegra-soc-hwpm-log.h"

#define LOG_BUF_SIZE	160

static void tegra_soc_hwpm_print(const char *func,
				 int line,
				 int type,
				 const char *log)
{
	switch (type) {
	case tegra_soc_hwpm_log_err:
		pr_err(TEGRA_SOC_HWPM_MODULE_NAME ": %s: %d: ERROR: %s\n",
			func, line, log);
		break;
	case tegra_soc_hwpm_log_dbg:
		pr_info(TEGRA_SOC_HWPM_MODULE_NAME ": %s: %d: DEBUG: %s\n",
			func, line, log);
		break;
	}
}

void tegra_soc_hwpm_log(const char *func, int line, int type, const char *fmt, ...)
{
	char log[LOG_BUF_SIZE];
	va_list args;

	va_start(args, fmt);
	(void) vsnprintf(log, LOG_BUF_SIZE, fmt, args);
	va_end(args);

	tegra_soc_hwpm_print(func, line, type, log);
}
