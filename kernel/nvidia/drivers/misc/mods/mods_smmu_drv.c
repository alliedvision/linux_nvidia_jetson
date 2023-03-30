// SPDX-License-Identifier: GPL-2.0
/*
 * This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/dma-buf.h>

#include <linux/of.h>
#include <linux/of_platform.h>
#include "mods_internal.h"

#define MODS_MAX_SMMU_DEVICES 16
static struct mods_smmu_dev mods_smmu_devs[MODS_MAX_SMMU_DEVICES];
static int mods_smmu_dev_num;

struct mods_smmu_dev *get_mods_smmu_device(u32 index)
{
	struct mods_smmu_dev *pdev = NULL;

	if (index < mods_smmu_dev_num)
		pdev = &mods_smmu_devs[index];
	else
		mods_error_printk("mods smmu dev index %d error\n", index);
	return pdev;
}

int get_mods_smmu_device_index(const char *name)
{
	int idx;
	int dev_num = mods_smmu_dev_num;

	for (idx = 0; idx < dev_num; idx++) {
		if (strcmp(mods_smmu_devs[idx].dev_name, name) == 0)
			return idx;
	}
	mods_error_printk("mods smmu device %s not found\n", name);
	return -EINVAL;
}

static int mods_smmu_driver_probe(struct platform_device *pdev)
{
	struct device      *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	const char         *dev_name;
	int                err;
	int                dev_idx;

	LOG_ENT();

	err = of_property_read_string(node, "dev-names", &dev_name);
	if (err < 0) {
		mods_error_printk(
			"smmu probe failed to read dev-names, ret=%d\n", err);
		LOG_EXT();
		return err;
	}
	mods_debug_printk(DEBUG_MEM, "smmu probe: dev-names=%s, dev_idx=%d\n",
			  dev_name,
			  mods_smmu_dev_num);
	if (mods_smmu_dev_num < MODS_MAX_SMMU_DEVICES) {
		dev_idx = mods_smmu_dev_num;
		mods_smmu_dev_num++;
	} else {
		mods_error_printk("Max Number of MODS Smmu Device Reached\n");
		LOG_EXT();
		return -ENOMEM;
	}
	mods_smmu_devs[dev_idx].dev = &pdev->dev;
	strncpy(mods_smmu_devs[dev_idx].dev_name,
		dev_name,
		MAX_DT_SIZE - 1);
#ifdef MODS_ENABLE_BPMP_MRQ_API
	mods_smmu_devs[dev_idx].bpmp = tegra_bpmp_get(dev);
#endif


	LOG_EXT();
	return err;
}

static int mods_smmu_driver_remove(struct platform_device *pdev)
{
	mods_smmu_dev_num = 0;
	return 0;
}

static const struct of_device_id of_ids[] = {
	{ .compatible = "nvidia,mods_smmu" },
	{ }
};

static struct platform_driver mods_smmu_driver = {
	.probe  = mods_smmu_driver_probe,
	.remove = mods_smmu_driver_remove,
	.driver = {
		.name   = "mods_smmu",
		.owner  = THIS_MODULE,
		.of_match_table = of_ids,
	},
};

int smmu_driver_init(void)
{
	mods_smmu_dev_num = 0;
	memset(&mods_smmu_devs[0], 0, sizeof(mods_smmu_devs));
	return platform_driver_register(&mods_smmu_driver);
}

void smmu_driver_exit(void)
{
	platform_driver_unregister(&mods_smmu_driver);
}
