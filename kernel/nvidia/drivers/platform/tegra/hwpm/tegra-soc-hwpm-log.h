/*
 * tegra-soc-hwpm-log.h:
 * This is the logging API header for the Tegra SOC HWPM driver.
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

#ifndef TEGRA_SOC_HWPM_LOG_H
#define TEGRA_SOC_HWPM_LOG_H

#define TEGRA_SOC_HWPM_MODULE_NAME	"tegra-soc-hwpm"

enum tegra_soc_hwpm_log_type {
	tegra_soc_hwpm_log_err,	/* Error prints */
	tegra_soc_hwpm_log_dbg,	/* Debug prints */
};

#define tegra_soc_hwpm_err(fmt, arg...)						\
		tegra_soc_hwpm_log(__func__, __LINE__, tegra_soc_hwpm_log_err,	\
				fmt, ##arg)
#define tegra_soc_hwpm_dbg(fmt, arg...)						\
		tegra_soc_hwpm_log(__func__, __LINE__, tegra_soc_hwpm_log_dbg,	\
				fmt, ##arg)

void tegra_soc_hwpm_log(const char *func, int line, int type, const char *fmt, ...);

#endif /* TEGRA_SOC_HWPM_LOG_H */
