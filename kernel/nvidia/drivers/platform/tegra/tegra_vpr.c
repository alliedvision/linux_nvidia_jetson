/*
 * Copyright (c) 2016-2022 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/ote_protocol.h>
#include <linux/delay.h>
#include <linux/version.h>

#include <linux/platform/tegra/common.h>

phys_addr_t __weak tegra_vpr_start;
phys_addr_t __weak tegra_vpr_size;

EXPORT_SYMBOL(tegra_vpr_start);
EXPORT_SYMBOL(tegra_vpr_size);

static int tegra_vpr_arg(char *options)
{
	char *p = options;

	tegra_vpr_size = memparse(p, &p);
	if (*p == '@')
		tegra_vpr_start = memparse(p+1, &p);
	pr_info("Found vpr, start=0x%llx size=%llx",
		(u64)tegra_vpr_start, (u64)tegra_vpr_size);
	return 0;
}
early_param("vpr", tegra_vpr_arg);
