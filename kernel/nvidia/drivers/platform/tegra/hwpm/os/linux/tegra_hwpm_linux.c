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

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/dma-buf.h>
#include <linux/debugfs.h>

#include <soc/tegra/fuse.h>

#include <tegra_hwpm_log.h>
#include <tegra_hwpm_debugfs.h>
#include <tegra_hwpm.h>
#include <tegra_hwpm_common.h>

static const struct of_device_id tegra_soc_hwpm_of_match[] = {
	{
		.compatible     = "nvidia,t234-soc-hwpm",
	}, {
	},
};
MODULE_DEVICE_TABLE(of, tegra_soc_hwpm_of_match);

static char *tegra_hwpm_get_devnode(struct device *dev, umode_t *mode)
{
	if (!mode) {
		return NULL;
	}

	/* Allow root:debug ownership */
	*mode = 0660;

	return NULL;
}

static bool tegra_hwpm_read_support_soc_tools_prop(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	bool allow_node = of_property_read_bool(np, "support-soc-tools");

	if (!allow_node) {
		tegra_hwpm_err(NULL, "support-soc-tools is absent");
	}

	return allow_node;
}

static int tegra_hwpm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = NULL;
	struct tegra_soc_hwpm *hwpm = NULL;

	if (!pdev) {
		tegra_hwpm_err(NULL, "Invalid platform device");
		ret = -ENODEV;
		goto fail;
	}

	if (!tegra_hwpm_read_support_soc_tools_prop(pdev)) {
		tegra_hwpm_err(NULL, "SOC HWPM not supported in this config");
		ret = -ENODEV;
		goto fail;
	}

	hwpm = kzalloc(sizeof(struct tegra_soc_hwpm), GFP_KERNEL);
	if (!hwpm) {
		tegra_hwpm_err(hwpm, "Couldn't allocate memory for hwpm struct");
		ret = -ENOMEM;
		goto fail;
	}
	hwpm->pdev = pdev;
	hwpm->dev = &pdev->dev;
	hwpm->np = pdev->dev.of_node;
	hwpm->class.owner = THIS_MODULE;
	hwpm->class.name = TEGRA_SOC_HWPM_MODULE_NAME;

	/* Create device node */
	ret = class_register(&hwpm->class);
	if (ret) {
		tegra_hwpm_err(hwpm, "Failed to register class");
		goto class_register;
	}

	/* Set devnode to retrieve device permissions */
	hwpm->class.devnode = tegra_hwpm_get_devnode;

	ret = alloc_chrdev_region(&hwpm->dev_t, 0, 1, dev_name(hwpm->dev));
	if (ret) {
		tegra_hwpm_err(hwpm, "Failed to allocate device region");
		goto alloc_chrdev_region;
	}

	cdev_init(&hwpm->cdev, &tegra_soc_hwpm_ops);
	hwpm->cdev.owner = THIS_MODULE;

	ret = cdev_add(&hwpm->cdev, hwpm->dev_t, 1);
	if (ret) {
		tegra_hwpm_err(hwpm, "Failed to add cdev");
		goto cdev_add;
	}

	dev = device_create(&hwpm->class,
				NULL,
				hwpm->dev_t,
				NULL,
				TEGRA_SOC_HWPM_MODULE_NAME);
	if (IS_ERR(dev)) {
		tegra_hwpm_err(hwpm, "Failed to create device");
		ret = PTR_ERR(dev);
		goto device_create;
	}

	(void) dma_set_mask_and_coherent(hwpm->dev, DMA_BIT_MASK(39));

	if (tegra_platform_is_silicon()) {
		hwpm->la_clk = devm_clk_get(hwpm->dev, "la");
		if (IS_ERR(hwpm->la_clk)) {
			tegra_hwpm_err(hwpm, "Missing la clock");
			ret = PTR_ERR(hwpm->la_clk);
			goto clock_reset_fail;
		}

		hwpm->la_parent_clk = devm_clk_get(hwpm->dev, "parent");
		if (IS_ERR(hwpm->la_parent_clk)) {
			tegra_hwpm_err(hwpm, "Missing la parent clk");
			ret = PTR_ERR(hwpm->la_parent_clk);
			goto clock_reset_fail;
		}

		hwpm->la_rst = devm_reset_control_get(hwpm->dev, "la");
		if (IS_ERR(hwpm->la_rst)) {
			tegra_hwpm_err(hwpm, "Missing la reset");
			ret = PTR_ERR(hwpm->la_rst);
			goto clock_reset_fail;
		}

		hwpm->hwpm_rst = devm_reset_control_get(hwpm->dev, "hwpm");
		if (IS_ERR(hwpm->hwpm_rst)) {
			tegra_hwpm_err(hwpm, "Missing hwpm reset");
			ret = PTR_ERR(hwpm->hwpm_rst);
			goto clock_reset_fail;
		}
	}

	tegra_hwpm_debugfs_init(hwpm);
	ret = tegra_hwpm_init_sw_components(hwpm);
	if (ret != 0) {
		tegra_hwpm_err(hwpm, "Failed to init sw components");
		goto init_sw_components_fail;
	}

	/*
	 * Currently VDK doesn't have a fmodel for SOC HWPM. Therefore, we
	 * enable fake registers on VDK for minimal testing.
	 */
	if (tegra_platform_is_vdk())
		hwpm->fake_registers_enabled = true;
	else
		hwpm->fake_registers_enabled = false;

	platform_set_drvdata(pdev, hwpm);
	tegra_soc_hwpm_pdev = pdev;

	tegra_hwpm_dbg(hwpm, hwpm_info, "Probe successful!");
	goto success;

init_sw_components_fail:
clock_reset_fail:
	if (tegra_platform_is_silicon()) {
		if (hwpm->la_clk)
			devm_clk_put(hwpm->dev, hwpm->la_clk);
		if (hwpm->la_parent_clk)
			devm_clk_put(hwpm->dev, hwpm->la_parent_clk);
		if (hwpm->la_rst)
			reset_control_assert(hwpm->la_rst);
		if (hwpm->hwpm_rst)
			reset_control_assert(hwpm->hwpm_rst);
	}
device_create:
	cdev_del(&hwpm->cdev);
cdev_add:
	unregister_chrdev_region(hwpm->dev_t, 1);
alloc_chrdev_region:
	class_unregister(&hwpm->class);
class_register:
	kfree(hwpm);
fail:
	tegra_hwpm_err(NULL, "Probe failed!");
success:
	return ret;
}

static int tegra_hwpm_remove(struct platform_device *pdev)
{
	struct tegra_soc_hwpm *hwpm = NULL;

	if (!pdev) {
		tegra_hwpm_err(hwpm, "Invalid platform device");
		return -ENODEV;
	}

	hwpm = platform_get_drvdata(pdev);
	if (!hwpm) {
		tegra_hwpm_err(hwpm, "Invalid hwpm struct");
		return -ENODEV;
	}

	if (tegra_platform_is_silicon()) {
		if (hwpm->la_clk)
			devm_clk_put(hwpm->dev, hwpm->la_clk);
		if (hwpm->la_parent_clk)
			devm_clk_put(hwpm->dev, hwpm->la_parent_clk);
		if (hwpm->la_rst)
			reset_control_assert(hwpm->la_rst);
		if (hwpm->hwpm_rst)
			reset_control_assert(hwpm->hwpm_rst);
	}

	device_destroy(&hwpm->class, hwpm->dev_t);
	cdev_del(&hwpm->cdev);
	unregister_chrdev_region(hwpm->dev_t, 1);
	class_unregister(&hwpm->class);

	tegra_hwpm_debugfs_deinit(hwpm);
	tegra_hwpm_release_sw_components(hwpm);

	return 0;
}

static struct platform_driver tegra_soc_hwpm_pdrv = {
	.probe		= tegra_hwpm_probe,
	.remove		= tegra_hwpm_remove,
	.driver		= {
		.name	= TEGRA_SOC_HWPM_MODULE_NAME,
		.of_match_table = of_match_ptr(tegra_soc_hwpm_of_match),
	},
};

static int __init tegra_hwpm_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&tegra_soc_hwpm_pdrv);
	if (ret < 0)
		tegra_hwpm_err(NULL, "Platform driver register failed");

	return ret;
}

static void __exit tegra_hwpm_exit(void)
{
	platform_driver_unregister(&tegra_soc_hwpm_pdrv);
}

postcore_initcall(tegra_hwpm_init);
module_exit(tegra_hwpm_exit);

MODULE_ALIAS(TEGRA_SOC_HWPM_MODULE_NAME);
MODULE_DESCRIPTION("Tegra SOC HWPM Driver");
MODULE_LICENSE("GPL v2");
