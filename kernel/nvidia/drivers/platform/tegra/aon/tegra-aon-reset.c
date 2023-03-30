/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/io.h>

#include <aon.h>

enum pm_controls {
	FW_LOAD_HALTED,
	FW_LOAD_DONE
};

/**
 * aon_evp_set_reset_addr - Writes to the evp reset addr register.
 *
 * @d : Pointer to struct tegra_aon
 * @addr : 32bit address
 *
 * Return : Void
 */
static inline void tegra_aon_evp_set_reset_addr(struct tegra_aon *aon, u32 addr)
{
	void __iomem *reg;

	reg = aon_reg(aon, evp_reset_addr_r());

	writel(addr, reg);
}

/**
 * aon_pm_set_pm_ctrl - Writes to the reset control register.
 *
 * @d : Pointer to struct tegra_aon
 * @val : Value to programmed to the register
 *
 * Return : Void
 */
static void tegra_aon_set_pm_ctrl(struct tegra_aon *aon, enum pm_controls val)
{
	void __iomem *reg;

	switch (val) {
	case FW_LOAD_DONE:
		reg = aon_reg(aon, pm_r5_ctrl_r());
		writel(pm_r5_ctrl_fwloaddone_done_f(), reg);
		break;
	case FW_LOAD_HALTED:
		reg = aon_reg(aon, pm_r5_ctrl_r());
		writel(pm_r5_ctrl_fwloaddone_halted_f(), reg);
		break;
	default:
		break;
	}
}

/**
 * tegra_aon_reset - Configures the pertinent registers in
 *					SPE cluser to reset SPE.
 *
 * @d : Pointer to tegra_aon struct.
 *
 * Return : 0 if success
 */
int tegra_aon_reset(struct tegra_aon *aon)
{
	u32 fw_aon_addr;

	if (!aon->fw->data) {
		dev_err(aon->dev, "No fw_data present");
		return -1;
	}

	fw_aon_addr = tegra_aon_get_fw_addr(aon);
	tegra_aon_evp_set_reset_addr(aon, fw_aon_addr);

	tegra_aon_set_pm_ctrl(aon, FW_LOAD_DONE);

	return 0;
}
