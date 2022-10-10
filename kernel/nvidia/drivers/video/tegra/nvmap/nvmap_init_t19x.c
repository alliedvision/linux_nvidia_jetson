/*
 * drivers/video/tegra/nvmap/nvmap_init_t19x.c
 *
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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

#define pr_fmt(fmt)	"nvmap: %s() " fmt, __func__

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/nvmap_t19x.h>
#include <linux/kobject.h>
#include <linux/debugfs.h>
#include <linux/sysfs.h>
#include <linux/io.h>

#include "nvmap_priv.h"

bool nvmap_version_t19x;

const struct of_device_id nvmap_of_ids[] = {
	{ .compatible = "nvidia,carveouts" },
	{ .compatible = "nvidia,carveouts-t18x" },
	{ .compatible = "nvidia,carveouts-t19x" },
	{ }
};

int nvmap_register_cvsram_carveout(struct device *dma_dev,
		phys_addr_t base, size_t size, int (*busy)(void),
		int (*idle)(void))
{
	static struct nvmap_platform_carveout cvsram = {
		.name = "cvsram",
		.usage_mask = NVMAP_HEAP_CARVEOUT_CVSRAM,
		.disable_dynamic_dma_map = true,
		.no_cpu_access = true,
	};

	cvsram.pm_ops.busy = busy;
	cvsram.pm_ops.idle = idle;

	if (!base || !size || (base != PAGE_ALIGN(base)) ||
	    (size != PAGE_ALIGN(size)))
		return -EINVAL;
	cvsram.base = base;
	cvsram.size = size;

	cvsram.dma_dev = &cvsram.dev;
	return nvmap_create_carveout(&cvsram);
}
EXPORT_SYMBOL(nvmap_register_cvsram_carveout);
