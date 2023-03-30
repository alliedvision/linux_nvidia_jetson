/*
 * CDI GPIO driver
 *
 * Copyright (c) 2017-2020, NVIDIA Corporation. All Rights Reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/nospec.h>

#include "cdi-gpio-priv.h"

#define MAX_STR_SIZE 255

static int of_cdi_gpio_pdata(struct platform_device *pdev,
				struct cdi_gpio_plat_data *pdata)
{
	struct device_node *np = pdev->dev.of_node;
	int err;

	err = of_property_read_string(np, "parent-gpio-chip",
					&pdata->gpio_prnt_chip);
	if (err < 0)
		return err;

	err = of_property_read_u32(pdev->dev.of_node,
					"max-gpios", &pdata->max_gpio);

	return err;
}

static int cdi_gpio_chip_match(struct gpio_chip *chip, void *data)
{
	return !strcmp(chip->label, data);
}

static struct gpio_chip *cdi_gpio_get_chip(struct platform_device *pdev,
						struct cdi_gpio_plat_data *pd)
{
	struct gpio_chip *gc = NULL;
	char name[MAX_STR_SIZE];

	if (strlen(pd->gpio_prnt_chip) > MAX_STR_SIZE) {
		dev_err(&pdev->dev, "%s: gpio chip name is too long: %s\n",
			__func__, pd->gpio_prnt_chip);
		return NULL;
	}
	strcpy(name, pd->gpio_prnt_chip);

	gc = gpiochip_find(name, cdi_gpio_chip_match);
	if (!gc) {
		dev_err(&pdev->dev, "%s: unable to find gpio parent chip %s\n",
			__func__, pd->gpio_prnt_chip);
		return NULL;
	}

	return gc;
}

static int cdi_gpio_init_desc(struct platform_device *pdev,
				struct cdi_gpio_priv *cdi_gpio)
{
	struct cdi_gpio_desc *desc = NULL;
	u32 i;

	desc = devm_kzalloc(&pdev->dev,
			(sizeof(struct cdi_gpio_desc) *
				cdi_gpio->pdata.max_gpio),
			GFP_KERNEL);
	if (!desc) {
		dev_err(&pdev->dev, "Unable to allocate memory!\n");
		return -ENOMEM;
	}

	for (i = 0; i < cdi_gpio->pdata.max_gpio; i++) {
		desc[i].gpio = 0;
		atomic_set(&desc[i].ref_cnt, 0);
	}

	cdi_gpio->gpios = desc;
	return 0;
}

static int cdi_gpio_get_index(struct device *dev,
				struct cdi_gpio_priv *cdi_gpio, u32 gpio)
{
	u32 i;
	int idx = -1;

	/* find gpio in array */
	for (i = 0; i < cdi_gpio->num_gpio; i++) {
		if (cdi_gpio->gpios[i].gpio == gpio) {
			idx = i;
			break;
		}
	}

	/* gpio exists, return idx */
	if (idx >= 0)
		return idx;

	/* add gpio if it doesn't exist and there is memory available */
	if (cdi_gpio->num_gpio < cdi_gpio->pdata.max_gpio) {
		idx = cdi_gpio->num_gpio;
		cdi_gpio->gpios[idx].gpio = gpio;
		cdi_gpio->num_gpio++;

		return idx;
	}

	dev_err(dev, "%s: Unable to add gpio to desc\n", __func__);
	return -EFAULT;
}

static int cdi_gpio_direction_input(struct gpio_chip *gc, unsigned int off)
{
	struct gpio_chip *tgc = NULL;
	struct cdi_gpio_priv *cdi_gpio = NULL;
	int err;

	cdi_gpio = gpiochip_get_data(gc);
	if (!cdi_gpio)
		return -EFAULT;

	mutex_lock(&cdi_gpio->mutex);

	tgc = cdi_gpio->tgc;
	err = tgc->direction_input(tgc, off);

	mutex_unlock(&cdi_gpio->mutex);

	return err;
}

static int cdi_gpio_direction_output(struct gpio_chip *gc, unsigned int off,
					int val)
{
	struct gpio_chip *tgc = NULL;
	struct cdi_gpio_priv *cdi_gpio = NULL;
	int err;

	cdi_gpio = gpiochip_get_data(gc);
	if (!cdi_gpio)
		return -EFAULT;

	mutex_lock(&cdi_gpio->mutex);

	tgc = cdi_gpio->tgc;
	err = tgc->direction_output(tgc, off, val);

	mutex_unlock(&cdi_gpio->mutex);

	return err;
}

static int cdi_gpio_get_value(struct gpio_chip *gc, unsigned int off)
{
	int gpio_val;
	struct gpio_chip *tgc = NULL;
	struct cdi_gpio_priv *cdi_gpio = NULL;

	cdi_gpio = gpiochip_get_data(gc);
	if (!cdi_gpio)
		return -EFAULT;

	mutex_lock(&cdi_gpio->mutex);

	tgc = cdi_gpio->tgc;
	gpio_val = tgc->get(tgc, off);

	mutex_unlock(&cdi_gpio->mutex);

	return gpio_val;
}

static void cdi_gpio_set_value(struct gpio_chip *gc, unsigned int off, int val)
{
	int idx;
	struct gpio_chip *tgc = NULL;
	struct cdi_gpio_priv *cdi_gpio = NULL;
	atomic_t *ref_cnt;
	struct device *dev = NULL;

	cdi_gpio = gpiochip_get_data(gc);
	if (!cdi_gpio)
		return;

	mutex_lock(&cdi_gpio->mutex);

	dev = cdi_gpio->pdev;
	tgc = cdi_gpio->tgc;

	idx = cdi_gpio_get_index(dev, cdi_gpio, off);
	if (idx < 0) {
		mutex_unlock(&cdi_gpio->mutex);
		return;
	}
	idx = array_index_nospec(idx, cdi_gpio->pdata.max_gpio);

	/* set gpio value based on refcount */
	ref_cnt = &cdi_gpio->gpios[idx].ref_cnt;
	switch (val) {
	case 0:
		if ((atomic_read(ref_cnt) > 0) &&
			atomic_dec_and_test(ref_cnt)) {
			tgc->set(tgc, off, val);
		}
		dev_info(dev, "%s: gpio idx: %d, val to set: %d, refcount: %d\n",
			__func__, idx, val, atomic_read(ref_cnt));
		break;
	case 1:
		if (!atomic_inc_and_test(ref_cnt))
			tgc->set(tgc, off, val);

		dev_info(dev, "%s: gpio idx: %d, val to set: %d, refcount: %d\n",
			__func__, idx, val, atomic_read(ref_cnt));
		break;
	default:
		dev_err(dev, "%s: Invalid gpio value provided\n",
		__func__);
		break;
	}

	mutex_unlock(&cdi_gpio->mutex);
}

static int cdi_gpio_probe(struct platform_device *pdev)
{
	struct cdi_gpio_priv *cdi_gpio;
	struct cdi_gpio_plat_data *pd = NULL;
	struct gpio_chip *tgc, *gc;
	int err;

	dev_info(&pdev->dev, "probing %s...\n", __func__);

	cdi_gpio = devm_kzalloc(&pdev->dev,
				sizeof(struct cdi_gpio_priv),
				GFP_KERNEL);
	if (!cdi_gpio) {
		dev_err(&pdev->dev, "Unable to allocate memory!\n");
		return -ENOMEM;
	}

	/* get platform data from device tree */
	err = of_cdi_gpio_pdata(pdev, &cdi_gpio->pdata);
	if (err < 0)
		return err;

	pd = &cdi_gpio->pdata;

	/* get tegra gpio chip */
	tgc = cdi_gpio_get_chip(pdev, pd);
	if (!tgc)
		return -ENXIO;

	cdi_gpio->tgc = tgc;

	/* initialize gpio desc */
	err = cdi_gpio_init_desc(pdev, cdi_gpio);
	if (err < 0)
		return err;

	cdi_gpio->num_gpio = 0;

	/* setup gpio chip */
	gc = &cdi_gpio->gpio_chip;
	gc->direction_input  = cdi_gpio_direction_input;
	gc->direction_output = cdi_gpio_direction_output;
	gc->get = cdi_gpio_get_value;
	gc->set = cdi_gpio_set_value;

	gc->can_sleep = false;
	gc->base = -1;
	gc->ngpio = pd->max_gpio;
	gc->label = "cdi-gpio";
	gc->of_node = pdev->dev.of_node;
	gc->owner = THIS_MODULE;

	err = gpiochip_add_data(gc, cdi_gpio);
	if (err) {
		dev_err(&pdev->dev, "failed to add GPIO controller\n");
		return err;
	}

	mutex_init(&cdi_gpio->mutex);
	cdi_gpio->pdev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, cdi_gpio);

	dev_info(&pdev->dev, "%s: successfully registered gpio device\n",
			__func__);
	return 0;
}

static int cdi_gpio_remove(struct platform_device *pdev)
{
	struct cdi_gpio_priv *cdi_gpio = platform_get_drvdata(pdev);

	gpiochip_remove(&cdi_gpio->gpio_chip);

	return 0;
}

static const struct of_device_id cdi_gpio_dt_ids[] = {
	{ .compatible = "nvidia,cdi-gpio", },
	{},
};
MODULE_DEVICE_TABLE(of, cdi_gpio_dt_ids);

static struct platform_driver cdi_gpio_driver = {
	.probe = cdi_gpio_probe,
	.remove = cdi_gpio_remove,
	.driver = {
		.name = "cdi-gpio",
		.of_match_table = cdi_gpio_dt_ids,
		.owner = THIS_MODULE,
	}
};

static int __init cdi_gpio_init(void)
{
	return platform_driver_register(&cdi_gpio_driver);
}

static void __exit cdi_gpio_exit(void)
{
	platform_driver_unregister(&cdi_gpio_driver);
}

/* call in subsys so that this module loads before cdi-mgr driver */
subsys_initcall(cdi_gpio_init);
module_exit(cdi_gpio_exit);

MODULE_DESCRIPTION("Tegra Auto CDI GPIO Driver");
MODULE_AUTHOR("Anurag Dosapati <adosapati@nvidia.com>");
MODULE_LICENSE("GPL v2");
