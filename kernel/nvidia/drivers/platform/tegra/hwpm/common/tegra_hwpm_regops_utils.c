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

#include <soc/tegra/fuse.h>

#include <uapi/linux/tegra-soc-hwpm-uapi.h>

#include <tegra_hwpm.h>
#include <tegra_hwpm_io.h>
#include <tegra_hwpm_log.h>
#include <tegra_hwpm_common.h>
#include <tegra_hwpm_static_analysis.h>

static int tegra_hwpm_exec_reg_ops(struct tegra_soc_hwpm *hwpm,
	struct tegra_soc_hwpm_reg_op *reg_op)
{
	bool found = false;
	u32 ip_idx = TEGRA_SOC_HWPM_IP_INACTIVE;
	u32 inst_idx = 0U, element_idx = 0U;
	u32 a_type = 0U;
	u32 reg_val = 0U;
	u64 addr_hi = 0ULL;
	int err = 0;
	enum tegra_hwpm_element_type element_type = HWPM_ELEMENT_INVALID;
	struct tegra_soc_hwpm_chip *active_chip = hwpm->active_chip;
	struct hwpm_ip *chip_ip = NULL;
	struct hwpm_ip_inst_per_aperture_info *inst_a_info = NULL;
	struct hwpm_ip_inst *ip_inst = NULL;
	struct hwpm_ip_element_info *e_info = NULL;
	struct hwpm_ip_aperture *element = NULL;

	tegra_hwpm_fn(hwpm, " ");

	/* Find IP aperture containing phys_addr in allowlist */
	found = tegra_hwpm_aperture_for_address(hwpm,
		TEGRA_HWPM_FIND_GIVEN_ADDRESS, reg_op->phys_addr,
		&ip_idx, &inst_idx, &element_idx, &element_type);
	if (!found) {
		/* Silent failure as regops can continue on error */
		tegra_hwpm_dbg(hwpm, hwpm_dbg_regops,
			"Phys addr 0x%llx not available in any IP",
			reg_op->phys_addr);
		reg_op->status = TEGRA_SOC_HWPM_REG_OP_STATUS_INVALID_ADDR;
		return -EINVAL;
	}

	tegra_hwpm_dbg(hwpm, hwpm_dbg_regops,
		"Found addr 0x%llx IP %d inst_idx %d element_idx %d e_type %d",
		reg_op->phys_addr, ip_idx, inst_idx, element_idx, element_type);

	switch (element_type) {
	case HWPM_ELEMENT_PERFMON:
		a_type = TEGRA_HWPM_APERTURE_TYPE_PERFMON;
		break;
	case HWPM_ELEMENT_PERFMUX:
	case IP_ELEMENT_PERFMUX:
		a_type = TEGRA_HWPM_APERTURE_TYPE_PERFMUX;
		break;
	case IP_ELEMENT_BROADCAST:
		a_type = TEGRA_HWPM_APERTURE_TYPE_BROADCAST;
		break;
	case HWPM_ELEMENT_INVALID:
	default:
		tegra_hwpm_err(hwpm, "Invalid element type %d", element_type);
		return -EINVAL;
	}

	chip_ip = active_chip->chip_ips[ip_idx];
	inst_a_info = &chip_ip->inst_aperture_info[a_type];
	ip_inst = inst_a_info->inst_arr[inst_idx];
	e_info = &ip_inst->element_info[a_type];
	element = e_info->element_arr[element_idx];

	switch (reg_op->cmd) {
	case TEGRA_SOC_HWPM_REG_OP_CMD_RD32:
		err = tegra_hwpm_regops_readl(hwpm, ip_inst, element,
			reg_op->phys_addr, &reg_op->reg_val_lo);
		if (err != 0) {
			reg_op->status = TEGRA_SOC_HWPM_REG_OP_STATUS_RD_FAILED;
			break;
		}
		reg_op->status = TEGRA_SOC_HWPM_REG_OP_STATUS_SUCCESS;
		break;

	case TEGRA_SOC_HWPM_REG_OP_CMD_RD64:
		addr_hi = tegra_hwpm_safe_add_u64(reg_op->phys_addr, 4ULL);
		err = tegra_hwpm_regops_readl(hwpm, ip_inst, element,
			reg_op->phys_addr, &reg_op->reg_val_lo);
		if (err != 0) {
			reg_op->status = TEGRA_SOC_HWPM_REG_OP_STATUS_RD_FAILED;
			break;
		}
		err = tegra_hwpm_regops_readl(hwpm, ip_inst, element,
			addr_hi, &reg_op->reg_val_hi);
		if (err != 0) {
			reg_op->status = TEGRA_SOC_HWPM_REG_OP_STATUS_RD_FAILED;
			break;
		}
		reg_op->status = TEGRA_SOC_HWPM_REG_OP_STATUS_SUCCESS;
		break;

	/* Read Modify Write operation */
	case TEGRA_SOC_HWPM_REG_OP_CMD_WR32:
		err = tegra_hwpm_regops_readl(hwpm, ip_inst, element,
			reg_op->phys_addr, &reg_val);
		if (err != 0) {
			reg_op->status = TEGRA_SOC_HWPM_REG_OP_STATUS_RD_FAILED;
			break;
		}
		reg_val = set_field(reg_val, reg_op->mask_lo,
				reg_op->reg_val_lo);
		err = tegra_hwpm_regops_writel(hwpm, ip_inst, element,
			reg_op->phys_addr, reg_val);
		if (err != 0) {
			reg_op->status = TEGRA_SOC_HWPM_REG_OP_STATUS_WR_FAILED;
			break;
		}
		reg_op->status = TEGRA_SOC_HWPM_REG_OP_STATUS_SUCCESS;
		break;

	/* Read Modify Write operation */
	case TEGRA_SOC_HWPM_REG_OP_CMD_WR64:
		addr_hi = tegra_hwpm_safe_add_u64(reg_op->phys_addr, 4ULL);

		/* Lower 32 bits */
		err = tegra_hwpm_regops_readl(hwpm, ip_inst, element,
			reg_op->phys_addr, &reg_val);
		if (err != 0) {
			reg_op->status = TEGRA_SOC_HWPM_REG_OP_STATUS_RD_FAILED;
			break;
		}
		reg_val = set_field(reg_val, reg_op->mask_lo,
				reg_op->reg_val_lo);
		err = tegra_hwpm_regops_writel(hwpm, ip_inst, element,
			reg_op->phys_addr, reg_val);
		if (err != 0) {
			reg_op->status = TEGRA_SOC_HWPM_REG_OP_STATUS_WR_FAILED;
			break;
		}

		/* Upper 32 bits */
		err = tegra_hwpm_regops_readl(hwpm, ip_inst, element,
			addr_hi, &reg_val);
		if (err != 0) {
			reg_op->status = TEGRA_SOC_HWPM_REG_OP_STATUS_RD_FAILED;
			break;
		}
		reg_val = set_field(reg_val, reg_op->mask_hi,
				reg_op->reg_val_hi);
		err = tegra_hwpm_regops_writel(hwpm, ip_inst, element,
			reg_op->phys_addr, reg_val);
		if (err != 0) {
			reg_op->status = TEGRA_SOC_HWPM_REG_OP_STATUS_WR_FAILED;
			break;
		}
		reg_op->status = TEGRA_SOC_HWPM_REG_OP_STATUS_SUCCESS;
		break;

	default:
		tegra_hwpm_err(hwpm, "Invalid reg op command(%u)", reg_op->cmd);
		reg_op->status = TEGRA_SOC_HWPM_REG_OP_STATUS_INVALID_CMD;
		return -EINVAL;
		break;
	}

	return 0;
}

int tegra_hwpm_exec_regops(struct tegra_soc_hwpm *hwpm,
	struct tegra_soc_hwpm_exec_reg_ops *exec_reg_ops)
{
	int op_idx = 0;
	int ret = 0;
	struct tegra_soc_hwpm_reg_op *reg_op = NULL;

	tegra_hwpm_fn(hwpm, " ");

	switch (exec_reg_ops->mode) {
	case TEGRA_SOC_HWPM_REG_OP_MODE_FAIL_ON_FIRST:
	case TEGRA_SOC_HWPM_REG_OP_MODE_CONT_ON_ERR:
		break;

	default:
		tegra_hwpm_err(hwpm, "Invalid reg ops mode(%u)",
				   exec_reg_ops->mode);
		return -EINVAL;
	}

	if (exec_reg_ops->op_count > TEGRA_SOC_HWPM_REG_OPS_SIZE) {
		tegra_hwpm_err(hwpm, "Reg_op count=%d exceeds max count",
				   exec_reg_ops->op_count);
		return -EINVAL;
	}

	/*
	 * Initialize flag to true assuming all regops will pass
	 * If any regop fails, the flag will be reset to false.
	 */
	exec_reg_ops->b_all_reg_ops_passed = true;

	for (op_idx = 0; op_idx < exec_reg_ops->op_count; op_idx++) {
		reg_op = &(exec_reg_ops->ops[op_idx]);
		tegra_hwpm_dbg(hwpm, hwpm_dbg_regops,
			"reg op: idx(%d), phys(0x%llx), cmd(%u)",
			op_idx, reg_op->phys_addr, reg_op->cmd);

		ret = tegra_hwpm_exec_reg_ops(hwpm, reg_op);
		if (ret < 0) {
			tegra_hwpm_err(hwpm, "exec_reg_ops %d failed", op_idx);
			exec_reg_ops->b_all_reg_ops_passed = false;
			if (exec_reg_ops->mode ==
				TEGRA_SOC_HWPM_REG_OP_MODE_FAIL_ON_FIRST) {
				return -EINVAL;
			}
		}
	}

	return 0;
}
