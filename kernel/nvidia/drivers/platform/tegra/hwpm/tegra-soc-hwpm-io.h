/*
 * tegra-soc-hwpm-io.h:
 * This header defines register read/write APIs for the Tegra SOC HWPM driver.
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

#ifndef TEGRA_SOC_HWPM_IO_H
#define TEGRA_SOC_HWPM_IO_H

struct tegra_soc_hwpm;
struct hwpm_resource_aperture;

struct hwpm_resource_aperture *find_hwpm_aperture(struct tegra_soc_hwpm *hwpm,
						  u64 phys_addr,
						  bool use_absolute_base,
						  bool check_reservation,
						  u64 *updated_pa);
u32 hwpm_readl(struct tegra_soc_hwpm *hwpm,
		u32 dt_aperture,
		u32 reg_offset);
void hwpm_writel(struct tegra_soc_hwpm *hwpm,
		u32 dt_aperture,
		u32 reg_offset, u32 val);
u32 ip_readl(struct tegra_soc_hwpm *hwpm, u64 phys_addr);
void ip_writel(struct tegra_soc_hwpm *hwpm, u64 phys_addr, u32 reg_val);
u32 ioctl_readl(struct tegra_soc_hwpm *hwpm,
		struct hwpm_resource_aperture *aperture,
		u64 addr);
void ioctl_writel(struct tegra_soc_hwpm *hwpm,
		  struct hwpm_resource_aperture *aperture,
		  u64 addr,
		  u32 val);
int reg_rmw(struct tegra_soc_hwpm *hwpm,
	    struct hwpm_resource_aperture *aperture,
	    u32 dt_aperture,
	    u64 addr,
	    u32 field_mask,
	    u32 field_val,
	    bool is_ioctl,
	    bool is_ip);

#endif /* TEGRA_SOC_HWPM_IO_H */
