/*
 * tegra-soc-hwpm-debugfs.c:
 * This file adds debugfs nodes for the Tegra SOC HWPM driver.
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

#include <stddef.h>
#include <linux/debugfs.h>

#include <hal/tegra-soc-hwpm-structures.h>
#include "tegra-soc-hwpm-log.h"
#include "tegra-soc-hwpm.h"

/* FIXME: This is a placeholder for now. We can add debugfs nodes as needed. */
void tegra_soc_hwpm_debugfs_init(struct tegra_soc_hwpm *hwpm)
{
	if (!hwpm) {
		tegra_soc_hwpm_err("Invalid hwpm struct");
		return;
	}

	hwpm->debugfs_root = debugfs_create_dir(TEGRA_SOC_HWPM_MODULE_NAME, NULL);
	if (!hwpm->debugfs_root) {
		tegra_soc_hwpm_err("Failed to create debugfs root directory");
		goto fail;
	}

	return;

fail:
	debugfs_remove_recursive(hwpm->debugfs_root);
	hwpm->debugfs_root = NULL;
}

void tegra_soc_hwpm_debugfs_deinit(struct tegra_soc_hwpm *hwpm)
{
	if (!hwpm) {
		tegra_soc_hwpm_err("Invalid hwpm struct");
		return;
	}

	debugfs_remove_recursive(hwpm->debugfs_root);
	hwpm->debugfs_root = NULL;
}
