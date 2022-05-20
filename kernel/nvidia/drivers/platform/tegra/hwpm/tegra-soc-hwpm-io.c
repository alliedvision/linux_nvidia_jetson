/*
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
 *
 * tegra-soc-hwpm-io.c:
 * This file contains register read/write functions for the Tegra SOC HWPM
 * driver.
 */

#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/io.h>

#include "tegra-soc-hwpm-io.h"
#include "tegra-soc-hwpm-log.h"
#include <hal/tegra-soc-hwpm-structures.h>
#include <hal/tegra_soc_hwpm_init.h>
#include <uapi/linux/tegra-soc-hwpm-uapi.h>

static u32 fake_readl(struct tegra_soc_hwpm *hwpm, u64 phys_addr)
{
	u32 reg_val = 0;
	u64 updated_pa = 0ULL;
	struct hwpm_resource_aperture *aperture = NULL;

	if (!hwpm->fake_registers_enabled) {
		tegra_soc_hwpm_err("Fake registers are disabled!");
		return 0;
	}

	aperture = tegra_soc_hwpm_find_aperture(hwpm, phys_addr, false, false, &updated_pa);
	if (!aperture) {
		tegra_soc_hwpm_err("Invalid reg op address(0x%llx)", phys_addr);
		return 0;
	}

	reg_val = aperture->fake_registers[(updated_pa - aperture->start_pa)/4];
	return reg_val;
}

static void fake_writel(struct tegra_soc_hwpm *hwpm,
			u64 phys_addr,
			u32 val)
{
	struct hwpm_resource_aperture *aperture = NULL;
	u64 updated_pa = 0ULL;

	if (!hwpm->fake_registers_enabled) {
		tegra_soc_hwpm_err("Fake registers are disabled!");
		return;
	}

	aperture = tegra_soc_hwpm_find_aperture(hwpm, phys_addr, false, false, &updated_pa);
	if (!aperture) {
		tegra_soc_hwpm_err("Invalid reg op address(0x%llx)", phys_addr);
		return;
	}

	aperture->fake_registers[(updated_pa - aperture->start_pa)/4] = val;
}

/* Read a HWPM (PERFMON, PMA, or RTR) register */
u32 hwpm_readl(struct tegra_soc_hwpm *hwpm, u32 dt_aperture, u32 reg_offset)
{
	if (!tegra_soc_hwpm_is_dt_aperture(dt_aperture)) {
		tegra_soc_hwpm_err("Invalid dt aperture(%d)", dt_aperture);
		return 0;
	}

	tegra_soc_hwpm_dbg(
		"dt_aperture(%d): dt_aperture addr(0x%llx) reg_offset(0x%x)",
		dt_aperture, hwpm->dt_apertures[dt_aperture], reg_offset);

	if (hwpm->fake_registers_enabled) {
		u64 base_pa = tegra_soc_hwpm_get_perfmon_base(dt_aperture);

		return fake_readl(hwpm, base_pa + reg_offset);
	} else {
		return readl(hwpm->dt_apertures[dt_aperture] + reg_offset);
	}
}

/* Write a HWPM (PERFMON, PMA, or RTR) register */
void hwpm_writel(struct tegra_soc_hwpm *hwpm, u32 dt_aperture,
		u32 reg_offset, u32 val)
{
	if (!tegra_soc_hwpm_is_dt_aperture(dt_aperture)) {
		tegra_soc_hwpm_err("Invalid dt aperture(%d)", dt_aperture);
		return;
	}

	tegra_soc_hwpm_dbg(
		"dt_aperture(%d): dt_aperture addr(0x%llx) "
		"reg_offset(0x%x), val(0x%x)",
		dt_aperture, hwpm->dt_apertures[dt_aperture], reg_offset, val);

	if (hwpm->fake_registers_enabled) {
		u64 base_pa = tegra_soc_hwpm_get_perfmon_base(dt_aperture);

		fake_writel(hwpm, base_pa + reg_offset, val);
	} else {
		writel(val, hwpm->dt_apertures[dt_aperture] + reg_offset);
	}
}

u32 ip_readl(struct tegra_soc_hwpm *hwpm, u64 phys_addr)
{
	tegra_soc_hwpm_dbg("reg read: phys_addr(0x%llx)", phys_addr);

	if (hwpm->fake_registers_enabled) {
		return fake_readl(hwpm, phys_addr);
	} else {
		u64 ip_start_pa = 0ULL;
		u32 reg_val = 0U;
		u32 dt_aperture = tegra_soc_hwpm_get_ip_aperture(hwpm,
			phys_addr, &ip_start_pa);
		struct tegra_soc_hwpm_ip_ops *ip_ops =
			dt_aperture == TEGRA_SOC_HWPM_DT_APERTURE_INVALID ?
			NULL : &hwpm->ip_info[dt_aperture];

		if (ip_ops && (*ip_ops->hwpm_ip_reg_op)) {
			int err = 0;

			tegra_soc_hwpm_dbg(
				"aperture: %d ip_ops offset(0x%llx)",
				dt_aperture, (phys_addr - ip_start_pa));
			err = (*ip_ops->hwpm_ip_reg_op)(ip_ops->ip_dev,
				TEGRA_SOC_HWPM_IP_REG_OP_READ,
				(phys_addr - ip_start_pa), &reg_val);
			if (err < 0) {
				tegra_soc_hwpm_err(
					"Failed to read ip register(0x%llx)",
					phys_addr);
				return 0U;
			}
		} else {
			/* Fall back to un-registered IP method */
			void __iomem *ptr = NULL;

			ptr = ioremap(phys_addr, 0x4);
			if (!ptr) {
				tegra_soc_hwpm_err(
					"Failed to map register(0x%llx)",
					phys_addr);
				return 0U;
			}
			reg_val = __raw_readl(ptr);
			iounmap(ptr);
		}
		return reg_val;
	}
}

void ip_writel(struct tegra_soc_hwpm *hwpm, u64 phys_addr, u32 reg_val)
{
	tegra_soc_hwpm_dbg("reg write: phys_addr(0x%llx), val(0x%x)",
			   phys_addr, reg_val);

	if (hwpm->fake_registers_enabled) {
		fake_writel(hwpm, phys_addr, reg_val);
	} else {
		u64 ip_start_pa = 0ULL;
		u32 dt_aperture = tegra_soc_hwpm_get_ip_aperture(hwpm,
			phys_addr, &ip_start_pa);
		struct tegra_soc_hwpm_ip_ops *ip_ops =
			dt_aperture == TEGRA_SOC_HWPM_DT_APERTURE_INVALID ?
			NULL : &hwpm->ip_info[dt_aperture];

		if (ip_ops && (*ip_ops->hwpm_ip_reg_op)) {
			int err = 0;

			tegra_soc_hwpm_dbg(
				"aperture: %d ip_ops offset(0x%llx)",
				dt_aperture, (phys_addr - ip_start_pa));
			err = (*ip_ops->hwpm_ip_reg_op)(ip_ops->ip_dev,
				TEGRA_SOC_HWPM_IP_REG_OP_WRITE,
				(phys_addr - ip_start_pa), &reg_val);
			if (err < 0) {
				tegra_soc_hwpm_err(
					"write ip reg(0x%llx) val 0x%x failed",
					phys_addr, reg_val);
				return;
			}
		} else {
			/* Fall back to un-registered IP method */
			void __iomem *ptr = NULL;

			ptr = ioremap(phys_addr, 0x4);
			if (!ptr) {
				tegra_soc_hwpm_err(
					"Failed to map register(0x%llx)",
					phys_addr);
				return;
			}
			__raw_writel(reg_val, ptr);
			iounmap(ptr);
		}
	}
}

/*
 * Read a register from the EXEC_REG_OPS IOCTL. It is assumed that the allowlist
 * check has been done before calling this function.
 */
u32 ioctl_readl(struct tegra_soc_hwpm *hwpm,
		struct hwpm_resource_aperture *aperture,
		u64 addr)
{
	u32 reg_val = 0;

	if (!aperture) {
		tegra_soc_hwpm_err("aperture is NULL");
		return 0;
	}

	if (aperture->is_ip) {
		reg_val = ip_readl(hwpm, addr);
	} else {
		reg_val = hwpm_readl(hwpm, aperture->dt_aperture,
				addr - aperture->start_pa);
	}
	return reg_val;
}

/*
 * Write a register from the EXEC_REG_OPS IOCTL. It is assumed that the
 * allowlist check has been done before calling this function.
 */
void ioctl_writel(struct tegra_soc_hwpm *hwpm,
		  struct hwpm_resource_aperture *aperture,
		  u64 addr,
		  u32 val)
{
	if (!aperture) {
		tegra_soc_hwpm_err("aperture is NULL");
		return;
	}

	if (aperture->is_ip) {
		ip_writel(hwpm, addr, val);
	} else {
		hwpm_writel(hwpm, aperture->dt_aperture,
			addr - aperture->start_pa, val);
	}
}

/* Read Modify Write register operation */
int reg_rmw(struct tegra_soc_hwpm *hwpm,
	    struct hwpm_resource_aperture *aperture,
	    u32 dt_aperture,
	    u64 addr,
	    u32 field_mask,
	    u32 field_val,
	    bool is_ioctl,
	    bool is_ip)
{
	u32 reg_val = 0;

	if (is_ioctl) {
		if (!aperture) {
			tegra_soc_hwpm_err("aperture is NULL");
			return -EIO;
		}
	}
	if (!is_ip) {
		if (!tegra_soc_hwpm_is_dt_aperture(dt_aperture)) {
			tegra_soc_hwpm_err("Invalid dt_aperture(%d)",
					   dt_aperture);
			return -EIO;
		}
	}

	/* Read current register value */
	if (is_ioctl)
		reg_val = ioctl_readl(hwpm, aperture, addr);
	else if (is_ip)
		reg_val = ip_readl(hwpm, addr);
	else
		reg_val = hwpm_readl(hwpm, dt_aperture, addr);

	/* Clear and write masked bits */
	reg_val &= ~field_mask;
	reg_val |=  field_val & field_mask;

	/* Write modified value to register */
	if (is_ioctl)
		ioctl_writel(hwpm, aperture, addr, reg_val);
	else if (is_ip)
		ip_writel(hwpm, addr, reg_val);
	else
		hwpm_writel(hwpm, dt_aperture, addr, reg_val);

	return 0;
}
