// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
 */

#define pr_fmt(fmt) "mc-hwpm: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#include "linux/platform/tegra/mc.h"
#include "linux/platform/tegra/mc_utils.h"



#ifdef CONFIG_TEGRA_SOC_HWPM
#include <uapi/linux/tegra-soc-hwpm-uapi.h>
#endif

/* Total channels = Broadcast channel + MC_MAX_CHANNELS */
#define TOTAL_CHANNELS (1 + MC_MAX_CHANNELS)
static void __iomem *memctlr_regs[TOTAL_CHANNELS];
static int dram_channels;
/**
 * Read from the MC.
 *
 * @idx The MC channel to read from.
 * @reg The offset of the register to read.
 *
 * Read from the specified MC channel: 0 -> Broadcast channel/ Global channel
 * 1 -> MC0, 16 -> MC16, etc. If @idx
 * corresponds to a non-existent channel then 0 is returned.
 */
static u32 memctrl_readl(u32 chnl_no, u32 reg)
{
	static bool warned;

	if (is_tegra_safety_build()) {
		if (!warned) {
			pr_warn("WARNING: VM isn't allowed to read MC register space in Safety Build");
			warned = true;
		}
		return 0xffff;
	}

	if (chnl_no > dram_channels)
		return 0;

	return readl(memctlr_regs[chnl_no] + reg);
}

/**
 * Write to the MC.
 *
 * @idx The MC channel to write to.
 * @val Value to write.
 * @reg The offset of the register to write.
 *
 * Write to the specified MC channel: 0 -> MC0, 1 -> MC1, etc. For writes there
 * is a special channel, %MC_BROADCAST_CHANNEL, which writes to all channels. If
 * @idx corresponds to a non-existent channel then the write is dropped.
 */
static void memctrl_writel(u32 chnl_no, u32 val, u32 reg)
{
	static bool warned;

	if (is_tegra_safety_build()) {
		if (!warned) {
			pr_warn("WARNING: VM isn't allowed to write into MC register space in Safety Build");
			warned = true;
		}
		return;
	}

	if (chnl_no > dram_channels) {
		pr_err("Incorrect channel number: %u\n", chnl_no);
		return;
	}

	writel(val, memctlr_regs[chnl_no] + reg);
}

static int tegra_mc_hwpm_reg_op(void *ip_dev,
	enum tegra_soc_hwpm_ip_reg_op reg_op,
	u32 inst_element_index, u64 reg_offset, u32 *reg_data)
{
	if (reg_offset > 0x10000) {
		pr_err("Incorrect reg offset: %llu\n", reg_offset);
		return -EPERM;
	}

	if (inst_element_index > get_dram_num_channels()) {
		pr_err("Incorrect channel number: %u\n", inst_element_index);
		return -EPERM;
	}

	if (reg_op == TEGRA_SOC_HWPM_IP_REG_OP_READ)
		*reg_data = memctrl_readl(inst_element_index, (u32)reg_offset);
	else if (reg_op == TEGRA_SOC_HWPM_IP_REG_OP_WRITE)
		memctrl_writel(inst_element_index, *reg_data, (u32)reg_offset);

	return 0;
}

/*
 * Map an MC register space. Each MC has a set of register ranges which must
 * be parsed. The first starting address in the set of ranges is returned as
 * it is expected that the DT file has the register ranges in ascending
 * order.
 *
 * device 0 = global channel.
 * device n = specific channel device-1, e.g device = 1 ==> channel 0.
 */
static void __iomem *tegra_mc_hwpm_map_regs(struct platform_device *pdev, int device)
{
	struct resource res;
	const void *prop;
	void __iomem *regs;
	void __iomem *regs_start = NULL;
	u32 reg_ranges;
	int i, start;

	prop = of_get_property(pdev->dev.of_node, "reg-ranges", NULL);
	if (!prop) {
		pr_err("Failed to get MC MMIO region\n");
		pr_err("  device = %d: missing reg-ranges\n", device);
		return NULL;
	}

	reg_ranges = be32_to_cpup(prop);
	start = device * reg_ranges;

	for (i = 0; i < reg_ranges; i++) {
		regs = of_iomap(pdev->dev.of_node, start + i);
		if (!regs) {
			pr_err("Failed to get MC MMIO region\n");
			pr_err("  device = %d, range = %u\n", device, i);
			return NULL;
		}

		if (i == 0)
			regs_start = regs;
	}

	if (of_address_to_resource(pdev->dev.of_node, start, &res))
		return NULL;

	pr_debug("mapped MMIO address: 0x%p -> 0x%lx\n",
		regs_start, (unsigned long)res.start);

	return regs_start;
}

static struct tegra_soc_hwpm_ip_ops hwpm_ip_ops;

const struct of_device_id mc_hwpm_of_ids[] = {
	{ .compatible = "nvidia,tegra-t23x-mc-hwpm" },
	{ }
};

/*
 * MC driver init.
 */
static int tegra_mc_hwpm_hwpm_probe(struct platform_device *pdev)
{

	const struct of_device_id *match;
	const void *prop;
	int i;

	pr_debug("%s:%u\n", __func__, __LINE__);
	if (!pdev->dev.of_node)
		return -EINVAL;

	match = of_match_device(mc_hwpm_of_ids, &pdev->dev);
	if (!match) {
		pr_err("Missing DT entry!\n");
		return -EINVAL;
	}

	/*
	 * Channel count.
	 */
	prop = of_get_property(pdev->dev.of_node, "channels", NULL);
	if (!prop)
		dram_channels = 1;
	else
		dram_channels = be32_to_cpup(prop);

	if (dram_channels > MC_MAX_CHANNELS || mc_channels < 1) {
		pr_err("Invalid number of memory channels: %d\n", dram_channels);
		return -EINVAL;
	}

	/*
	 * Store reg mapping for broadcast channel
	 */
	memctlr_regs[0] = tegra_mc_hwpm_map_regs(pdev, 0);
	if (!memctlr_regs[0])
		return -ENOMEM;

	/* Populate the rest of the channels... */
	if (dram_channels > 1) {
		for (i = 1; i <= dram_channels; i++) {
			memctlr_regs[i] = tegra_mc_hwpm_map_regs(pdev, i);
			if (!memctlr_regs[i])
				return -ENOMEM;
		}
	}

	hwpm_ip_ops.ip_dev = (void *)pdev;
	hwpm_ip_ops.resource_enum = TEGRA_SOC_HWPM_RESOURCE_MSS_CHANNEL;
	hwpm_ip_ops.ip_base_address = pdev->resource[0].start;
	hwpm_ip_ops.hwpm_ip_reg_op = &tegra_mc_hwpm_reg_op;
	tegra_soc_hwpm_ip_register(&hwpm_ip_ops);

	return 0;
}

static int tegra_mc_hwpm_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver mc_hwpm_driver = {
	.driver = {
		.name	= "tegra-mc-hwpm",
		.of_match_table = mc_hwpm_of_ids,
		.owner	= THIS_MODULE,
	},

	.probe		= tegra_mc_hwpm_hwpm_probe,
	.remove		= tegra_mc_hwpm_remove,
};

static int __init tegra_mc_hwpm_init(void)
{
	int ret;

	ret = platform_driver_register(&mc_hwpm_driver);
	if (ret)
		return ret;

	return 0;
}
module_init(tegra_mc_hwpm_init);

static void __exit tegra_mc_hwpm_fini(void)
{
}
module_exit(tegra_mc_hwpm_fini);
