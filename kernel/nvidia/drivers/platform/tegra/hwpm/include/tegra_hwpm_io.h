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

#ifndef TEGRA_HWPM_IO_H
#define TEGRA_HWPM_IO_H

/**
 * Sets a particular field value in input data.
 *
 * Uses mask to clear specific bit positions in curr_val. field_val
 * is used to set the bits in curr_val to be returned.
 * Note: Function does not perform any validation of input parameters.
 *
 * curr_val [in] Current input data value.
 *
 * mask [in] Mask of the bits to be updated.
 *
 * field_val [in] Value to change the mask bits to.
 *
 * Returns updated value.
 */
static inline u32 set_field(u32 curr_val, u32 mask, u32 field_val)
{
	return ((curr_val & ~mask) | field_val);
}

/**
 * Retrieve value of specific bits from input data.
 * Note: Function does not perform any validation of input parameters.
 *
 * input_data [in] Data to retrieve value from.
 *
 * mask [in] Mask of the bits to get value from.
 *
 * Return value from input_data corresponding to mask bits.
 */
static inline u32 get_field(u32 input_data, u32 mask)
{
	return (input_data & mask);
}


struct tegra_soc_hwpm;
struct hwpm_ip_inst;
struct hwpm_ip_aperture;

int tegra_hwpm_read_sticky_bits(struct tegra_soc_hwpm *hwpm,
	u64 reg_base, u64 reg_offset, u32 *val);
int tegra_hwpm_readl(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_aperture *aperture, u64 addr, u32 *val);
int tegra_hwpm_writel(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_aperture *aperture, u64 addr, u32 val);
int tegra_hwpm_regops_readl(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_inst *ip_inst, struct hwpm_ip_aperture *aperture,
	u64 addr, u32 *val);
int tegra_hwpm_regops_writel(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_inst *ip_inst, struct hwpm_ip_aperture *aperture,
	u64 addr, u32 val);

#endif /* TEGRA_HWPM_IO_H */
