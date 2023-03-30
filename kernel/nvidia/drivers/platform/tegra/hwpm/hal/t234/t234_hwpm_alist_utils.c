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

#include <linux/bitops.h>
#include <uapi/linux/tegra-soc-hwpm-uapi.h>

#include <tegra_hwpm_log.h>
#include <tegra_hwpm_io.h>
#include <tegra_hwpm.h>
#include <tegra_hwpm_static_analysis.h>
#include <hal/t234/t234_hwpm_internal.h>
#include <hal/t234/t234_hwpm_regops_allowlist.h>

size_t t234_hwpm_get_alist_buf_size(struct tegra_soc_hwpm *hwpm)
{
	return sizeof(struct allowlist);
}

int t234_hwpm_zero_alist_regs(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_inst *ip_inst, struct hwpm_ip_aperture *aperture)
{
	u32 alist_idx = 0U;
	int err = 0;

	tegra_hwpm_fn(hwpm, " ");

	for (alist_idx = 0; alist_idx < aperture->alist_size; alist_idx++) {
		if (aperture->alist[alist_idx].zero_at_init) {
			err = tegra_hwpm_regops_writel(hwpm, ip_inst, aperture,
				tegra_hwpm_safe_add_u64(aperture->start_abs_pa,
					aperture->alist[alist_idx].reg_offset),
				0U);
			if (err != 0) {
				tegra_hwpm_err(hwpm, "zero alist regs failed");
				return err;
			}
		}
	}
	return 0;
}

int t234_hwpm_copy_alist(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_aperture *aperture, u64 *full_alist,
	u64 *full_alist_idx)
{
	u64 f_alist_idx = *full_alist_idx;
	u64 alist_idx = 0ULL;

	tegra_hwpm_fn(hwpm, " ");

	if (aperture->alist == NULL) {
		tegra_hwpm_err(hwpm, "NULL allowlist in aperture");
		return -EINVAL;
	}

	for (alist_idx = 0ULL; alist_idx < aperture->alist_size; alist_idx++) {
		if (f_alist_idx >= hwpm->full_alist_size) {
			tegra_hwpm_err(hwpm, "No space in full_alist");
			return -ENOMEM;
		}

		full_alist[f_alist_idx++] = tegra_hwpm_safe_add_u64(
			aperture->start_abs_pa,
			aperture->alist[alist_idx].reg_offset);
	}

	/* Store next available index */
	*full_alist_idx = f_alist_idx;

	return 0;
}

bool t234_hwpm_check_alist(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_aperture *aperture, u64 phys_addr)
{
	u32 alist_idx;
	u64 reg_offset;

	tegra_hwpm_fn(hwpm, " ");

	if (!aperture) {
		tegra_hwpm_err(hwpm, "Aperture is NULL");
		return false;
	}
	if (!aperture->alist) {
		tegra_hwpm_err(hwpm, "NULL allowlist in aperture");
		return false;
	}

	reg_offset = tegra_hwpm_safe_sub_u64(phys_addr, aperture->start_abs_pa);

	for (alist_idx = 0; alist_idx < aperture->alist_size; alist_idx++) {
		if (reg_offset == aperture->alist[alist_idx].reg_offset) {
			return true;
		}
	}
	return false;
}
