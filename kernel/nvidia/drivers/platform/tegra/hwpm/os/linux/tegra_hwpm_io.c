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

#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/io.h>

#include <uapi/linux/tegra-soc-hwpm-uapi.h>

#include <tegra_hwpm.h>
#include <tegra_hwpm_io.h>
#include <tegra_hwpm_log.h>
#include <tegra_hwpm_static_analysis.h>

int tegra_hwpm_read_sticky_bits(struct tegra_soc_hwpm *hwpm,
	u64 reg_base, u64 reg_offset, u32 *val)
{
	void __iomem *ptr = NULL;
	u64 reg_addr = tegra_hwpm_safe_add_u64(reg_base, reg_offset);

	ptr = ioremap(reg_addr, 0x4);
	if (!ptr) {
		tegra_hwpm_err(hwpm, "Failed to map register(0x%llx)",
			reg_addr);
		return -ENODEV;
	}
	*val = __raw_readl(ptr);
	iounmap(ptr);

	return 0;
}

static int fake_readl(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_aperture *aperture, u64 offset, u32 *val)
{
	if (!hwpm->fake_registers_enabled) {
		tegra_hwpm_err(hwpm, "Fake registers are disabled!");
		return -ENODEV;
	}

	*val = aperture->fake_registers[offset];
	return 0;
}

static int fake_writel(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_aperture *aperture, u64 offset, u32 val)
{
	if (!hwpm->fake_registers_enabled) {
		tegra_hwpm_err(hwpm, "Fake registers are disabled!");
		return -ENODEV;
	}

	aperture->fake_registers[offset] = val;
	return 0;
}

/*
 * Read IP domain registers
 * IP(except PMA and RTR) perfmux fall in this category
 */
static int ip_readl(struct tegra_soc_hwpm *hwpm, struct hwpm_ip_inst *ip_inst,
	struct hwpm_ip_aperture *aperture, u64 offset, u32 *val)
{
	tegra_hwpm_dbg(hwpm, hwpm_register,
		"Aperture (0x%llx-0x%llx) offset(0x%llx)",
		aperture->start_abs_pa, aperture->end_abs_pa, offset);

	if (hwpm->fake_registers_enabled) {
		return fake_readl(hwpm, aperture, offset, val);
	} else {
		struct tegra_hwpm_ip_ops *ip_ops_ptr = &ip_inst->ip_ops;
		if (ip_ops_ptr->hwpm_ip_reg_op != NULL) {
			int err = 0;

			err = (*ip_ops_ptr->hwpm_ip_reg_op)(ip_ops_ptr->ip_dev,
				TEGRA_SOC_HWPM_IP_REG_OP_READ,
				aperture->dt_index, offset, val);
			if (err < 0) {
				tegra_hwpm_err(hwpm, "Aperture (0x%llx-0x%llx) "
					"read offset(0x%llx) failed",
					aperture->start_abs_pa,
					aperture->end_abs_pa, offset);
				return err;
			}
		} else {
			/* Fall back to un-registered IP method */
			void __iomem *ptr = NULL;
			u64 reg_addr = tegra_hwpm_safe_add_u64(
				aperture->start_abs_pa, offset);

			ptr = ioremap(reg_addr, 0x4);
			if (!ptr) {
				tegra_hwpm_err(hwpm,
					"Failed to map register(0x%llx)",
					reg_addr);
				return -ENODEV;
			}
			*val = __raw_readl(ptr);
			iounmap(ptr);
		}
	}
	return 0;
}

/*
 * Write to IP domain registers
 * IP(except PMA and RTR) perfmux fall in this category
 */
static int ip_writel(struct tegra_soc_hwpm *hwpm, struct hwpm_ip_inst *ip_inst,
	struct hwpm_ip_aperture *aperture, u64 offset, u32 val)
{
	tegra_hwpm_dbg(hwpm, hwpm_register,
		"Aperture (0x%llx-0x%llx) offset(0x%llx) val(0x%x)",
		aperture->start_abs_pa, aperture->end_abs_pa, offset, val);

	if (hwpm->fake_registers_enabled) {
		return fake_writel(hwpm, aperture, offset, val);
	} else {
		struct tegra_hwpm_ip_ops *ip_ops_ptr = &ip_inst->ip_ops;
		if (ip_ops_ptr->hwpm_ip_reg_op != NULL) {
			int err = 0;

			err = (*ip_ops_ptr->hwpm_ip_reg_op)(ip_ops_ptr->ip_dev,
				TEGRA_SOC_HWPM_IP_REG_OP_WRITE,
				aperture->dt_index, offset, &val);
			if (err < 0) {
				tegra_hwpm_err(hwpm, "Aperture (0x%llx-0x%llx) "
					"write offset(0x%llx) val 0x%x failed",
					aperture->start_abs_pa,
					aperture->end_abs_pa, offset, val);
				return err;
			}
		} else {
			/* Fall back to un-registered IP method */
			void __iomem *ptr = NULL;
			u64 reg_addr = tegra_hwpm_safe_add_u64(
				aperture->start_abs_pa, offset);

			ptr = ioremap(reg_addr, 0x4);
			if (!ptr) {
				tegra_hwpm_err(hwpm,
					"Failed to map register(0x%llx)",
					reg_addr);
				return -ENODEV;
			}
			__raw_writel(val, ptr);
			iounmap(ptr);
		}
	}
	return 0;
}

/*
 * Read HWPM domain registers
 * PERFMONs, PMA and RTR registers fall in this category
 */
static int hwpm_readl(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_aperture *aperture,  u64 offset, u32 *val)
{
	tegra_hwpm_dbg(hwpm, hwpm_register,
		"Aperture (0x%llx-0x%llx) offset(0x%llx)",
		aperture->start_abs_pa, aperture->end_abs_pa, offset);

	if (aperture->dt_mmio == NULL) {
		tegra_hwpm_err(hwpm, "aperture is not iomapped as expected");
		return -ENODEV;
	}

	if (hwpm->fake_registers_enabled) {
		return fake_readl(hwpm, aperture, offset, val);
	} else {
		*val = readl(aperture->dt_mmio + offset);
	}

	return 0;
}

/*
 * Write to HWPM domain registers
 * PERFMONs, PMA and RTR registers fall in this category
 */
static int hwpm_writel(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_aperture *aperture, u64 offset, u32 val)
{
	int err = 0;

	tegra_hwpm_dbg(hwpm, hwpm_register,
		"Aperture (0x%llx-0x%llx) offset(0x%llx) val(0x%x)",
		aperture->start_abs_pa, aperture->end_abs_pa, offset, val);

	if (aperture->dt_mmio == NULL) {
		tegra_hwpm_err(hwpm, "aperture is not iomapped as expected");
		return -ENODEV;
	}

	if (hwpm->fake_registers_enabled) {
		err = fake_writel(hwpm, aperture, offset, val);
	} else {
		writel(val, aperture->dt_mmio + offset);
	}

	return err;
}

/*
 * Read a HWPM domain register. It is assumed that valid aperture
 * is passed to the function.
 */
int tegra_hwpm_readl(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_aperture *aperture, u64 addr, u32 *val)
{
	tegra_hwpm_fn(hwpm, " ");

	if (!aperture) {
		tegra_hwpm_err(hwpm, "aperture is NULL");
		return -ENODEV;
	}

	if ((aperture->element_type == HWPM_ELEMENT_PERFMON) ||
		(aperture->element_type == HWPM_ELEMENT_PERFMUX)) {
		u64 reg_offset = tegra_hwpm_safe_sub_u64(
					addr, aperture->base_pa);
		/* HWPM domain registers */
		return hwpm_readl(hwpm, aperture, reg_offset, val);
	} else {
		tegra_hwpm_err(hwpm, "IP aperture read is not expected");
		return -EINVAL;
	}
	return 0;
}

/*
 * Write to a HWPM domain register. It is assumed that valid aperture
 * is passed to the function.
 */
int tegra_hwpm_writel(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_aperture *aperture, u64 addr, u32 val)
{
	tegra_hwpm_fn(hwpm, " ");

	if (!aperture) {
		tegra_hwpm_err(hwpm, "aperture is NULL");
		return -ENODEV;
	}

	if ((aperture->element_type == HWPM_ELEMENT_PERFMON) ||
		(aperture->element_type == HWPM_ELEMENT_PERFMUX)) {
		u64 reg_offset = tegra_hwpm_safe_sub_u64(
					addr, aperture->base_pa);
		/* HWPM domain internal registers */
		return hwpm_writel(hwpm, aperture, reg_offset, val);
	} else {
		tegra_hwpm_err(hwpm, "IP aperture write is not expected");
		return -EINVAL;
	}
	return 0;
}

/*
 * Read a register from the EXEC_REG_OPS IOCTL. It is assumed that the allowlist
 * check has been done before calling this function.
 */
int tegra_hwpm_regops_readl(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_inst *ip_inst, struct hwpm_ip_aperture *aperture,
	u64 addr, u32 *val)
{
	u64 reg_offset = 0ULL;
	int err = 0;

	tegra_hwpm_fn(hwpm, " ");

	if (!aperture) {
		tegra_hwpm_err(hwpm, "aperture is NULL");
		return -ENODEV;
	}

	reg_offset = tegra_hwpm_safe_sub_u64(addr, aperture->start_abs_pa);

	if ((aperture->element_type == HWPM_ELEMENT_PERFMON) ||
		(aperture->element_type == HWPM_ELEMENT_PERFMUX)) {
		err = hwpm_readl(hwpm, aperture, reg_offset, val);
	} else {
		err = ip_readl(hwpm, ip_inst, aperture, reg_offset, val);
	}
	return err;
}

/*
 * Write a register from the EXEC_REG_OPS IOCTL. It is assumed that the
 * allowlist check has been done before calling this function.
 */
int tegra_hwpm_regops_writel(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_inst *ip_inst, struct hwpm_ip_aperture *aperture,
	u64 addr, u32 val)
{
	u64 reg_offset = 0ULL;
	int err = 0;

	tegra_hwpm_fn(hwpm, " ");

	if (!aperture) {
		tegra_hwpm_err(hwpm, "aperture is NULL");
		return -ENODEV;
	}

	reg_offset = tegra_hwpm_safe_sub_u64(addr, aperture->start_abs_pa);

	if ((aperture->element_type == HWPM_ELEMENT_PERFMON) ||
		(aperture->element_type == HWPM_ELEMENT_PERFMUX)) {
		err = hwpm_writel(hwpm, aperture, reg_offset, val);
	} else {
		err = ip_writel(hwpm, ip_inst, aperture, reg_offset, val);
	}
	return err;
}
