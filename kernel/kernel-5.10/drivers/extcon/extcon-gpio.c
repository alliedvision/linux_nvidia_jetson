// SPDX-License-Identifier: GPL-2.0-only
/*
 * extcon_gpio.c - Single-state GPIO extcon driver based on extcon class
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * Modified by MyungJoo Ham <myungjoo.ham@samsung.com> to support extcon
 * (originally switch class is supported)
 */

#include <linux/extcon-provider.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "extcon.h"
/**
 * struct gpio_extcon_data - A simple GPIO-controlled extcon device state container.
 * @edev:		Extcon device.
 * @work:		Work fired by the interrupt.
 * @debounce_jiffies:	Number of jiffies to wait for the GPIO to stabilize, from the debounce
 *			value.
 * @gpiod:		GPIO descriptor for this external connector.
 * @extcon_id:		The unique id of specific external connector.
 * @debounce:		Debounce time for GPIO IRQ in ms.
 * @check_on_resume:	Boolean describing whether to check the state of gpio
 *			while resuming from sleep.
 */
struct gpio_extcon_data {
	const char *name;
	unsigned int supported_cable[2];
	int gpio;
	bool gpio_active_low;
	unsigned long irq_flags;
	bool default_state;

	struct device *dev;
	struct extcon_dev *edev;
	struct delayed_work work;
	unsigned long debounce_jiffies;
	struct gpio_desc *gpiod;
	unsigned int extcon_id;
	unsigned long debounce;
	bool check_on_resume;
};

static void gpio_extcon_work(struct work_struct *work)
{
	int state;
	int ret;
	struct gpio_extcon_data	*data =
		container_of(to_delayed_work(work), struct gpio_extcon_data,
			     work);

	if (gpio_is_valid(data->gpio)) {
		state = gpiod_get_value_cansleep(data->gpiod);
		if (data->gpio_active_low)
			state = !state;
	} else {
		state = data->default_state;
	}
	ret = extcon_set_state(data->edev,
			       data->supported_cable[0], state);
	if (ret < 0)
		dev_err(data->dev, "cable state update failed\n");
}

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
	struct gpio_extcon_data *data = dev_id;

	queue_delayed_work(system_power_efficient_wq, &data->work,
			   data->debounce_jiffies);
	return IRQ_HANDLED;
}

static int of_get_platform_data(struct platform_device *pdev,
				struct gpio_extcon_data *data)
{
	struct device_node *np = pdev->dev.of_node;
	int gpio;
	u32 pval;
	int ret;

	ret = of_property_read_string(np, "extcon-gpio,name", &data->name);
	if (ret < 0)
		data->name = np->name;

	ret = of_property_read_u32_array(np, "extcon-gpio,supported-cable",
					 data->supported_cable, 2);
	if (ret)
		return -EINVAL;

	gpio = of_get_named_gpio(np, "gpio", 0);
	if (gpio < 0 && (gpio != -ENOENT))
		return gpio;

	if (gpio == -ENOENT)
		data->gpio = -1;
	else
		data->gpio = gpio;

	if (data->gpio < 0)
		data->default_state = of_property_read_bool(np,
					"extcon-gpio,default-connected");

	ret = of_property_read_u32(np, "extcon-gpio,irq-flags", &pval);
	if (!ret)
		data->irq_flags = pval;
	else
		data->irq_flags = IRQF_TRIGGER_RISING |
				  IRQF_TRIGGER_FALLING;

	ret = of_property_read_u32(np, "extcon-gpio,debounce", &pval);
	if (!ret)
		data->debounce = pval;

	data->gpio_active_low = of_property_read_bool(np,
				"extcon-gpio,connection-state-low");

	return 0;
}

static int gpio_extcon_probe(struct platform_device *pdev)
{
	struct gpio_extcon_data *data;
	struct device *dev = &pdev->dev;
	unsigned long irq_flags;
	int irq;
	int ret;

	data = devm_kzalloc(dev, sizeof(struct gpio_extcon_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	if (!data && dev->of_node) {
		ret = of_get_platform_data(pdev, data);
		if (ret < 0)
			return ret;
	}

	/*
	 * FIXME: extcon_id represents the unique identifier of external
	 * connectors such as EXTCON_USB, EXTCON_DISP_HDMI and so on. extcon_id
	 * is necessary to register the extcon device. But, it's not yet
	 * developed to get the extcon id from device-tree or others.
	 * On later, it have to be solved.
	 */
	if (data->extcon_id > EXTCON_NONE)
		return -EINVAL;

	data->gpiod = devm_gpiod_get(dev, "extcon", GPIOD_IN);
	if (IS_ERR(data->gpiod))
		return PTR_ERR(data->gpiod);
	irq = gpiod_to_irq(data->gpiod);
	if (irq <= 0)
		return irq;

	/*
	 * It is unlikely that this is an acknowledged interrupt that goes
	 * away after handling, what we are looking for are falling edges
	 * if the signal is active low, and rising edges if the signal is
	 * active high.
	 */
	if (gpiod_is_active_low(data->gpiod))
		irq_flags = IRQF_TRIGGER_FALLING;
	else
		irq_flags = IRQF_TRIGGER_RISING;

	/* Allocate the memory of extcon devie and register extcon device */
	data->edev = devm_extcon_dev_allocate(&pdev->dev,
					      data->supported_cable);
	if (IS_ERR(data->edev)) {
		dev_err(dev, "failed to allocate extcon device\n");
		return -ENOMEM;
	}

	data->edev->name = data->name;
	ret = devm_extcon_dev_register(dev, data->edev);
	if (ret < 0)
		return ret;

	INIT_DELAYED_WORK(&data->work, gpio_extcon_work);

	if (!gpio_is_valid(data->gpio))
		goto skip_gpio;

	/*
	 * Request the interrupt of gpio to detect whether external connector
	 * is attached or detached.
	 */
	ret = devm_request_any_context_irq(dev, irq,
					gpio_irq_handler, irq_flags,
					pdev->name, data);
	if (ret < 0)
		return ret;

skip_gpio:
	platform_set_drvdata(pdev, data);
	/* Perform initial detection */
	gpio_extcon_work(&data->work.work);

	return 0;
}

static int gpio_extcon_remove(struct platform_device *pdev)
{
	struct gpio_extcon_data *data = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&data->work);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int gpio_extcon_resume(struct device *dev)
{
	struct gpio_extcon_data *data;

	data = dev_get_drvdata(dev);
	if (data->check_on_resume)
		queue_delayed_work(system_power_efficient_wq,
				   &data->work, data->debounce_jiffies);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(gpio_extcon_pm_ops, NULL, gpio_extcon_resume);

static const struct of_device_id of_extcon_gpio_tbl[] = {
	{ .compatible = "extcon-gpio", },
	{ /* end */ }
};
MODULE_DEVICE_TABLE(of, of_extcon_gpio_tbl);

static struct platform_driver gpio_extcon_driver = {
	.probe		= gpio_extcon_probe,
	.remove		= gpio_extcon_remove,
	.driver		= {
		.name	= "extcon-gpio",
		.pm	= &gpio_extcon_pm_ops,
		.of_match_table = of_extcon_gpio_tbl,
	},
};

module_platform_driver(gpio_extcon_driver);

MODULE_AUTHOR("Mike Lockwood <lockwood@android.com>");
MODULE_DESCRIPTION("GPIO extcon driver");
MODULE_LICENSE("GPL");
