/*
 * tegra-hwpm.c:
 * This is Tegra's driver for dummy SOC HWPM.
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

#include <uapi/linux/tegra-soc-hwpm-uapi.h>

void tegra_soc_hwpm_ip_register(struct tegra_soc_hwpm_ip_ops *hwpm_ip_ops)
{
	/* Dummy function */
}

void tegra_soc_hwpm_ip_unregister(struct tegra_soc_hwpm_ip_ops *hwpm_ip_ops)
{
	/* Dummy function */
}
