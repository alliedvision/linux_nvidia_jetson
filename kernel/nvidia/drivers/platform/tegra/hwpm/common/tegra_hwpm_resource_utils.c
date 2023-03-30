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

#include <tegra_hwpm_log.h>
#include <tegra_hwpm.h>
#include <tegra_hwpm_common.h>
#include <tegra_hwpm_static_analysis.h>

int tegra_hwpm_reserve_rtr(struct tegra_soc_hwpm *hwpm)
{
	int err = 0;

	struct tegra_soc_hwpm_chip *active_chip = hwpm->active_chip;

	tegra_hwpm_fn(hwpm, " ");

	err = tegra_hwpm_func_single_ip(hwpm, NULL,
		TEGRA_HWPM_RESERVE_GIVEN_RESOURCE,
		active_chip->get_rtr_int_idx(hwpm));
	if (err != 0) {
		tegra_hwpm_err(hwpm, "failed to reserve IP %d",
			active_chip->get_rtr_int_idx(hwpm));
		return err;
	}
	return err;
}

int tegra_hwpm_release_rtr(struct tegra_soc_hwpm *hwpm)
{
	int err = 0;
	struct tegra_soc_hwpm_chip *active_chip = hwpm->active_chip;

	tegra_hwpm_fn(hwpm, " ");

	err = tegra_hwpm_func_single_ip(hwpm, NULL,
		TEGRA_HWPM_RELEASE_ROUTER,
		active_chip->get_rtr_int_idx(hwpm));
	if (err != 0) {
		tegra_hwpm_err(hwpm, "failed to release IP %d",
			active_chip->get_rtr_int_idx(hwpm));
		return err;
	}
	return err;
}

int tegra_hwpm_reserve_resource(struct tegra_soc_hwpm *hwpm, u32 resource)
{
	struct tegra_soc_hwpm_chip *active_chip = hwpm->active_chip;
	u32 ip_idx = TEGRA_SOC_HWPM_IP_INACTIVE;
	int err = 0;

	tegra_hwpm_fn(hwpm, " ");

	tegra_hwpm_dbg(hwpm, hwpm_info,
		"User requesting to reserve resource %d", resource);

	/* Translate resource to ip_idx */
	if (!active_chip->is_resource_active(hwpm, resource, &ip_idx)) {
		tegra_hwpm_err(hwpm, "Requested resource %d is unavailable",
			resource);
		return -EINVAL;
	}

	err = tegra_hwpm_func_single_ip(hwpm, NULL,
		TEGRA_HWPM_RESERVE_GIVEN_RESOURCE, ip_idx);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "failed to reserve IP %d", ip_idx);
		return err;
	}

	return 0;
}

int tegra_hwpm_bind_resources(struct tegra_soc_hwpm *hwpm)
{
	int err = 0;

	tegra_hwpm_fn(hwpm, " ");

	err = tegra_hwpm_func_all_ip(hwpm, NULL, TEGRA_HWPM_BIND_RESOURCES);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "failed to bind resources");
		return err;
	}

	return err;
}

int tegra_hwpm_release_resources(struct tegra_soc_hwpm *hwpm)
{
	int ret = 0;

	tegra_hwpm_fn(hwpm, " ");

	ret = tegra_hwpm_func_all_ip(hwpm, NULL, TEGRA_HWPM_RELEASE_RESOURCES);
	if (ret != 0) {
		tegra_hwpm_err(hwpm, "failed to release resources");
		return ret;
	}

	return 0;
}
