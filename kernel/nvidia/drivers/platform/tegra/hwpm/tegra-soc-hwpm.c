/*
 * tegra-soc-hwpm.c:
 * This is Tegra's driver for programming the SOC HWPM path.
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

#include "tegra-soc-hwpm.h"
#include "tegra-soc-hwpm-log.h"
#include <hal/tegra_soc_hwpm_init.h>
#include <hal/tegra-soc-hwpm-structures.h>

static const struct of_device_id tegra_soc_hwpm_of_match[] = {
	{
		.compatible     = "nvidia,t234-soc-hwpm",
	}, {
	},
};
MODULE_DEVICE_TABLE(of, tegra_soc_hwpm_of_match);

static int tegra_soc_hwpm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = NULL;
	struct tegra_soc_hwpm *hwpm = NULL;

	if (!pdev) {
		tegra_soc_hwpm_err("Invalid platform device");
		ret = -ENODEV;
		goto fail;
	}

	hwpm = kzalloc(sizeof(struct tegra_soc_hwpm), GFP_KERNEL);
	if (!hwpm) {
		tegra_soc_hwpm_err("Couldn't allocate memory for hwpm struct");
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
		tegra_soc_hwpm_err("Failed to register class");
		goto class_register;
	}

	ret = alloc_chrdev_region(&hwpm->dev_t, 0, 1, dev_name(hwpm->dev));
	if (ret) {
		tegra_soc_hwpm_err("Failed to allocate device region");
		goto alloc_chrdev_region;
	}

	cdev_init(&hwpm->cdev, &tegra_soc_hwpm_ops);
	hwpm->cdev.owner = THIS_MODULE;

	ret = cdev_add(&hwpm->cdev, hwpm->dev_t, 1);
	if (ret) {
		tegra_soc_hwpm_err("Failed to add cdev");
		goto cdev_add;
	}

	dev = device_create(&hwpm->class,
				NULL,
				hwpm->dev_t,
				NULL,
				TEGRA_SOC_HWPM_MODULE_NAME);
	if (IS_ERR(dev)) {
		tegra_soc_hwpm_err("Failed to create device");
		ret = PTR_ERR(dev);
		goto device_create;
	}

	(void) dma_set_mask_and_coherent(hwpm->dev, DMA_BIT_MASK(39));

	if (tegra_platform_is_silicon()) {
		hwpm->la_clk = devm_clk_get(hwpm->dev, "la");
		if (IS_ERR(hwpm->la_clk)) {
			tegra_soc_hwpm_err("Missing la clock");
			ret = PTR_ERR(hwpm->la_clk);
			goto clock_reset_fail;
		}

		hwpm->la_parent_clk = devm_clk_get(hwpm->dev, "parent");
		if (IS_ERR(hwpm->la_parent_clk)) {
			tegra_soc_hwpm_err("Missing la parent clk");
			ret = PTR_ERR(hwpm->la_parent_clk);
			goto clock_reset_fail;
		}

		hwpm->la_rst = devm_reset_control_get(hwpm->dev, "la");
		if (IS_ERR(hwpm->la_rst)) {
			tegra_soc_hwpm_err("Missing la reset");
			ret = PTR_ERR(hwpm->la_rst);
			goto clock_reset_fail;
		}

		hwpm->hwpm_rst = devm_reset_control_get(hwpm->dev, "hwpm");
		if (IS_ERR(hwpm->hwpm_rst)) {
			tegra_soc_hwpm_err("Missing hwpm reset");
			ret = PTR_ERR(hwpm->hwpm_rst);
			goto clock_reset_fail;
		}
	}

	tegra_soc_hwpm_debugfs_init(hwpm);
	hwpm->dt_apertures = tegra_soc_hwpm_init_dt_apertures();
	hwpm->ip_info = tegra_soc_hwpm_init_ip_ops_info();

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

	tegra_soc_hwpm_dbg("Probe successful!");
	goto success;


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
	tegra_soc_hwpm_err("Probe failed!");
success:
	return ret;
}

static int tegra_soc_hwpm_remove(struct platform_device *pdev)
{
	struct tegra_soc_hwpm *hwpm = NULL;

	if (!pdev) {
		tegra_soc_hwpm_err("Invalid platform device");
		return -ENODEV;
	}

	hwpm = platform_get_drvdata(pdev);
	if (!hwpm) {
		tegra_soc_hwpm_err("Invalid hwpm struct");
		return -ENODEV;
	}

	tegra_soc_hwpm_debugfs_deinit(hwpm);

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

	kfree(hwpm);
	tegra_soc_hwpm_pdev = NULL;

	return 0;
}

static struct platform_driver tegra_soc_hwpm_pdrv = {
	.probe		= tegra_soc_hwpm_probe,
	.remove		= tegra_soc_hwpm_remove,
	.driver		= {
		.name	= TEGRA_SOC_HWPM_MODULE_NAME,
		.of_match_table = of_match_ptr(tegra_soc_hwpm_of_match),
	},
};

static int __init tegra_soc_hwpm_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&tegra_soc_hwpm_pdrv);
	if (ret < 0)
		tegra_soc_hwpm_err("Platform driver register failed");

	return ret;
}

static void __exit tegra_soc_hwpm_exit(void)
{
	tegra_soc_hwpm_dbg("Unloading the Tegra SOC HWPM driver");
	platform_driver_unregister(&tegra_soc_hwpm_pdrv);
}

postcore_initcall(tegra_soc_hwpm_init);
module_exit(tegra_soc_hwpm_exit);

MODULE_ALIAS(TEGRA_SOC_HWPM_MODULE_NAME);
MODULE_DESCRIPTION("Tegra SOC HWPM Driver");
MODULE_LICENSE("GPL v2");
