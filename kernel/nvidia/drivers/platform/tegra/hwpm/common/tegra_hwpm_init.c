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

#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/dma-buf.h>
#include <soc/tegra/fuse.h>
#include <uapi/linux/tegra-soc-hwpm-uapi.h>

#include <tegra_hwpm_log.h>
#include <tegra_hwpm_io.h>
#include <tegra_hwpm.h>
#include <tegra_hwpm_common.h>

#include <hal/t234/t234_hwpm_init.h>

static int tegra_hwpm_init_chip_info(struct tegra_soc_hwpm *hwpm)
{
	int err = -EINVAL;

	tegra_hwpm_fn(hwpm, " ");

	hwpm->device_info.chip = tegra_get_chip_id();
	hwpm->device_info.chip_revision = tegra_get_major_rev();
	hwpm->device_info.revision = tegra_chip_get_revision();
	hwpm->device_info.platform = tegra_get_platform();

	hwpm->dbg_mask = TEGRA_HWPM_DEFAULT_DBG_MASK;

	switch (hwpm->device_info.chip) {
	case 0x23:
		switch (hwpm->device_info.chip_revision) {
		case 0x4:
			err = t234_hwpm_init_chip_info(hwpm);
			break;
		default:
			tegra_hwpm_err(hwpm, "Chip 0x%x rev 0x%x not supported",
				hwpm->device_info.chip,
				hwpm->device_info.chip_revision);
			break;
		}
		break;
	default:
		tegra_hwpm_err(hwpm, "Chip 0x%x not supported",
			hwpm->device_info.chip);
		break;
	}

	if (err != 0) {
		tegra_hwpm_err(hwpm, "init_chip_info failed");
	}

	return err;
}

static int tegra_hwpm_init_chip_ip_structures(struct tegra_soc_hwpm *hwpm)
{
	int err = 0;

	tegra_hwpm_fn(hwpm, " ");

	err = tegra_hwpm_func_all_ip(hwpm, NULL, TEGRA_HWPM_INIT_IP_STRUCTURES);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "failed init IP structures");
		return err;
	}

	return err;
}

int tegra_hwpm_init_sw_components(struct tegra_soc_hwpm *hwpm)
{
	int err = 0;

	tegra_hwpm_fn(hwpm, " ");

	err = tegra_hwpm_init_chip_info(hwpm);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "Failed to initialize current chip info.");
		return err;
	}

	err = tegra_hwpm_init_chip_ip_structures(hwpm);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "IP structure init failed");
		return err;
	}

	err = tegra_hwpm_finalize_chip_info(hwpm);
	if (err < 0) {
		tegra_hwpm_err(hwpm, "Unable to initialize chip fs_info");
		return err;
	}

	return 0;
}

void tegra_hwpm_release_sw_components(struct tegra_soc_hwpm *hwpm)
{
	struct hwpm_ip_register_list *node = ip_register_list_head;
	struct hwpm_ip_register_list *tmp_node = NULL;

	tegra_hwpm_fn(hwpm, " ");

	hwpm->active_chip->release_sw_setup(hwpm);

	while (node != NULL) {
		tmp_node = node;
		node = tmp_node->next;
		kfree(tmp_node);
	}

	kfree(hwpm->active_chip->chip_ips);
	kfree(hwpm);
	tegra_soc_hwpm_pdev = NULL;
}

int tegra_hwpm_setup_sw(struct tegra_soc_hwpm *hwpm)
{
	int ret = 0;
	tegra_hwpm_fn(hwpm, " ");

	ret = hwpm->active_chip->validate_current_config(hwpm);
	if (ret != 0) {
		tegra_hwpm_err(hwpm, "Failed to validate current conifg");
		return ret;
	}

	ret = tegra_hwpm_func_all_ip(hwpm, NULL,
		TEGRA_HWPM_UPDATE_IP_INST_MASK);
	if (ret != 0) {
		tegra_hwpm_err(hwpm, "Failed to update IP fs_info");
		return ret;
	}

	/* Initialize SW state */
	hwpm->bind_completed = false;
	hwpm->full_alist_size = 0;

	return 0;
}

int tegra_hwpm_setup_hw(struct tegra_soc_hwpm *hwpm)
{
	int ret = 0;

	tegra_hwpm_fn(hwpm, " ");

	/*
	 * Map RTR aperture
	 * RTR is hwpm aperture which includes hwpm config registers.
	 * Map/reserve these apertures to get MMIO address required for hwpm
	 * configuration (following steps).
	 */
	ret = hwpm->active_chip->reserve_rtr(hwpm);
	if (ret < 0) {
		tegra_hwpm_err(hwpm, "Unable to reserve RTR aperture");
		goto fail;
	}

	/* Disable SLCG */
	ret = hwpm->active_chip->disable_slcg(hwpm);
	if (ret < 0) {
		tegra_hwpm_err(hwpm, "Unable to disable SLCG");
		goto fail;
	}

	/* Program PROD values */
	ret = hwpm->active_chip->init_prod_values(hwpm);
	if (ret < 0) {
		tegra_hwpm_err(hwpm, "Unable to set PROD values");
		goto fail;
	}

	return 0;
fail:
	return ret;
}

int tegra_hwpm_disable_triggers(struct tegra_soc_hwpm *hwpm)
{
	tegra_hwpm_fn(hwpm, " ");

	return hwpm->active_chip->disable_triggers(hwpm);
}

int tegra_hwpm_release_hw(struct tegra_soc_hwpm *hwpm)
{
	int ret = 0;

	tegra_hwpm_fn(hwpm, " ");

	/* Enable SLCG */
	ret = hwpm->active_chip->enable_slcg(hwpm);
	if (ret < 0) {
		tegra_hwpm_err(hwpm, "Unable to enable SLCG");
		goto fail;
	}

	/*
	 * Unmap RTR apertures
	 * Since, RTR hwpm apertures consist of hwpm config registers,
	 * these aperture mappings are required to reset hwpm config.
	 * Hence, explicitly unmap/release these apertures as a last step.
	 */
	ret = hwpm->active_chip->release_rtr(hwpm);
	if (ret < 0) {
		tegra_hwpm_err(hwpm, "Unable to release RTR aperture");
		goto fail;
	}

	return 0;
fail:
	return ret;
}

void tegra_hwpm_release_sw_setup(struct tegra_soc_hwpm *hwpm)
{
	int err = 0;

	tegra_hwpm_fn(hwpm, " ");

	err = tegra_hwpm_func_all_ip(hwpm, NULL,
		TEGRA_HWPM_RELEASE_IP_STRUCTURES);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "failed release IP structures");
		return;
	}

	return;
}
