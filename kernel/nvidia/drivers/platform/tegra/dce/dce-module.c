/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <dce.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>

/**
 * The following platform info is needed for backdoor
 * booting of dce.
 */
static const struct dce_platform_data t234_dce_platform_data = {
	.dce_stream_id = 0x08,
	.phys_stream_id = 0x7f,
	.fw_carveout_id = 9,
	.fw_vmindex = 0,
	.fw_name = "dce.bin",
	.fw_dce_addr = 0x40000000,
	.fw_info_valid = true,
	.use_physical_id = false,
};

static const struct of_device_id tegra_dce_of_match[] = {
	{
		.compatible = "nvidia,tegra234-dce",
		.data = (struct dce_platform_data *)&t234_dce_platform_data
	},
	{ },
};

/**
 * dce_set_pdata_dce - inline function to set the tegra_dce pointer in
 *			pdata point to actual tegra_dce data structure.
 *
 * @pdev : Pointer to the platform device data structure.
 * @d : Pointer pointing to actual tegra_dce data structure.
 *
 * Return :  Void.
 */
static inline void dce_set_pdata_dce(struct platform_device *pdev,
					struct tegra_dce *d)
{
	((struct dce_platform_data *)dev_get_drvdata(&pdev->dev))->d = d;
}

/**
 * dce_get_pdata_dce - inline function to get the tegra_dce pointer
 *						from platform devicve.
 *
 * @pdev : Pointer to the platform device data structure.
 * @d : Pointer pointing to actual tegra_dce data structure.
 *
 * Return :  Void.
 */
static inline struct tegra_dce *dce_get_pdata_dce(struct platform_device *pdev)
{
	return (((struct dce_platform_data *)dev_get_drvdata(&pdev->dev))->d);
}

/**
 * dce_init_dev_data - Function to initialize the dce device data structure.
 *
 * @pdev : Pointer to Linux's platform device used for registering DCE.
 *
 * Primarily used during initialization sequence and is expected to be called
 * from probe only.
 *
 * Return : 0 if success else the corresponding error value.
 */
static int dce_init_dev_data(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dce_device *d_dev = NULL;

	d_dev = devm_kzalloc(dev, sizeof(*d_dev), GFP_KERNEL);
	if (!d_dev)
		return -ENOMEM;

	dce_set_pdata_dce(pdev, &d_dev->d);
	d_dev->dev = dev;
	d_dev->regs = of_iomap(dev->of_node, 0);
	if (!d_dev->regs) {
		dev_err(dev, "failed to map dce cluster IO space\n");
		return -EINVAL;
	}


	return 0;
}
/**
 * dce_isr - Handles dce interrupts
 */
static irqreturn_t dce_isr(int irq, void *data)
{
	struct tegra_dce *d = data;

	dce_mailbox_isr(d);

	return IRQ_HANDLED;
}

static void dce_set_irqs(struct platform_device *pdev, bool en)
{
	int i = 0;
	struct tegra_dce *d;
	struct dce_platform_data *pdata = NULL;

	pdata = dev_get_drvdata(&pdev->dev);
	d = pdata->d;

	for (i = 0; i < pdata->max_cpu_irqs; i++) {
		if (en)
			enable_irq(d->irq[i]);
		else
			disable_irq(d->irq[i]);
	}
}

/**
 * dce_req_interrupts - function to initialize CPU irqs for DCE cpu driver.
 *
 * @pdev : Pointet to Dce Linux Platform Device.
 *
 * Return : 0 if success else the corresponding error value.
 */
static int dce_req_interrupts(struct platform_device *pdev)
{
	int i = 0;
	int ret = 0;
	int no_ints = 0;
	struct tegra_dce *d;
	struct dce_platform_data *pdata = NULL;

	pdata = dev_get_drvdata(&pdev->dev);
	d = pdata->d;
	no_ints = of_irq_count(pdev->dev.of_node);
	if (no_ints == 0) {
		dev_err(&pdev->dev,
			"Invalid number of interrupts configured = %d",
			no_ints);
		return -EINVAL;
	}

	pdata->max_cpu_irqs = no_ints;

	for (i = 0; i < no_ints; i++) {
		ret = of_irq_get(pdev->dev.of_node, i);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Getting dce intr lines failed with ret = %d",
				ret);
			return ret;
		}
		d->irq[i] = ret;
		ret = devm_request_threaded_irq(&pdev->dev, d->irq[i],
				NULL, dce_isr, IRQF_ONESHOT, "tegra_dce_isr",
				d);
		if (ret) {
			dev_err(&pdev->dev,
				"failed to request irq @ with ret = %d\n",
				ret);
		}
		disable_irq(d->irq[i]);
	}
	return ret;
}

static int tegra_dce_probe(struct platform_device *pdev)
{
	int err = 0;
	struct tegra_dce *d = NULL;
	struct device *dev = &pdev->dev;
	struct dce_platform_data *pdata = NULL;
	const struct of_device_id *match = NULL;

	match = of_match_device(tegra_dce_of_match, dev);
	pdata = (struct dce_platform_data *)match->data;

	WARN_ON(!pdata);
	if (!pdata) {
		dev_info(dev, "no platform data\n");
		err = -ENODATA;
		goto err_get_pdata;
	}
	dev_set_drvdata(dev, pdata);

	err = dce_init_dev_data(pdev);
	if (err) {
		dev_err(dev, "failed to init device data with err = %d\n",
			err);
		goto os_init_err;
	}

	err = dce_req_interrupts(pdev);
	if (err) {
		dev_err(dev, "failed to get interrupts with err = %d\n",
			err);
		goto req_intr_err;
	}

	d = pdata->d;

	err = dce_driver_init(d);
	if (err) {
		dce_err(d, "DCE Driver Init Failed");
		goto err_driver_init;
	}

	dce_set_irqs(pdev, true);

#ifdef CONFIG_DEBUG_FS
	dce_init_debug(d);
#endif
	return 0;

req_intr_err:
os_init_err:
err_get_pdata:
err_driver_init:
	return err;
}

static int tegra_dce_remove(struct platform_device *pdev)
{
	/* TODO */
	struct tegra_dce *d =
			dce_get_pdata_dce(pdev);
	dce_set_irqs(pdev, false);
	dce_driver_deinit(d);
	return 0;
}

static struct platform_driver tegra_dce_driver = {
	.driver = {
		.name   = "tegra-dce",
		.of_match_table =
			of_match_ptr(tegra_dce_of_match),
	},
	.probe = tegra_dce_probe,
	.remove = tegra_dce_remove,
};
module_platform_driver(tegra_dce_driver);

MODULE_DESCRIPTION("DCE Linux driver");
MODULE_AUTHOR("NVIDIA");
MODULE_LICENSE("GPL v2");
