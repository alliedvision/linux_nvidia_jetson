// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include "mods_ras.h"

#include <linux/err.h>
#include <linux/platform/tegra/carmel_ras.h>
#include <linux/platform/tegra/tegra18_cpu_map.h>

/* Encodes requested core/cluster to report
 * ras for. If 0, refers to ccplex
 */
static u64 ras_ccplex_config;

void enable_cpu_core_reporting(u64 config)
{
	ras_ccplex_config = config;
}

void set_err_sel(u64 sel_val)
{
	u8 core, cluster, ccplex;
	u64 errx;

	ccplex = 0;
	core = ras_ccplex_config >> 1;
	cluster = ras_ccplex_config & 1;
	if (!ras_ccplex_config)
		ccplex = 1;

	if (ccplex) {
		errx = sel_val;
	} else {
		if (!cluster) {
			errx = (tegra18_logical_to_cluster(core) << 5) +
				(tegra18_logical_to_cpu(core) << 4) +
				sel_val;
		} else {
			errx = 512 + (tegra18_logical_to_cluster(core) << 4)
				+ sel_val;
		}
	}

	mods_debug_printk(DEBUG_FUNC, "ERR_SEL is %llu, Core is %u\n",
							errx, core);

	ras_write_errselr(errx);
}

void set_err_ctrl(u64 ctrl_val)
{
	u64 updated_val;

	ras_write_error_control(ctrl_val);
	updated_val = ras_read_error_control();
	mods_debug_printk(DEBUG_ALL, "ERR_CTRL updated value is %llu",
							updated_val);
}

u64 get_err_ctrl(void)
{
	return ras_read_error_control();
}
