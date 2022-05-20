// SPDX-License-Identifier: GPL-2.0
/*
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

#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>

#define SAFETY_I2S_CTRL_0	(0x114)
#define   MASTER		(1 << 5)

struct tegra_uss_io_proxy {
	void __iomem *i2s8_base;
	struct reset_control *i2s8_reset;
	struct clk *uss_clk;

	int uss_reset_gpio;
	int vsup_dia_gpio;
	int vsup_latch_gpio;
	struct gpio_descs *g1_gpiods;
	struct gpio_descs *g2_gpiods;
	unsigned long g1_bitmap;
	unsigned long g2_bitmap;
};

static const struct of_device_id tegra_uss_io_proxy_of_match[] = {
	{ .compatible = "nvidia,uss-io-proxy", },
	{ },
};

static bool uss_clk_enabled;
static ssize_t uss_clk_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct tegra_uss_io_proxy *proxy = dev_get_drvdata(dev);
	int val, rc;
	u32 reg;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val == 0) {
		rc = reset_control_assert(proxy->i2s8_reset);
		if (rc) {
			dev_err(dev, "i2s8 reset assert failed: %d\n", rc);
			return -EIO;
		}
		clk_disable_unprepare(proxy->uss_clk);
		uss_clk_enabled = false;
	} else {
		rc = clk_prepare_enable(proxy->uss_clk);
		if (rc) {
			dev_err(dev, "i2s8 clock enable failed: %d\n", rc);
			return -EIO;
		}

		rc = reset_control_deassert(proxy->i2s8_reset);
		if (rc) {
			dev_err(dev, "i2s8 reset deassert failed: %d\n", rc);
			clk_disable_unprepare(proxy->uss_clk);
			return -EIO;
		}

		reg = ioread32(proxy->i2s8_base + SAFETY_I2S_CTRL_0);
		reg |= MASTER;
		iowrite32(reg, proxy->i2s8_base + SAFETY_I2S_CTRL_0);

		uss_clk_enabled = true;
	}

	return count;
}

static ssize_t uss_clk_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	if (uss_clk_enabled)
		return sprintf(buf, "1\n");
	else
		return sprintf(buf, "0\n");
}

static DEVICE_ATTR_RW(uss_clk);

static ssize_t uss_reset_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct tegra_uss_io_proxy *proxy = dev_get_drvdata(dev);
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val == 0)
		gpio_set_value(proxy->uss_reset_gpio, 1);
	else
		gpio_set_value(proxy->uss_reset_gpio, 0);

	return count;
}

static ssize_t uss_reset_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct tegra_uss_io_proxy *proxy = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", !gpio_get_value(proxy->uss_reset_gpio));
}

static DEVICE_ATTR_RW(uss_reset);

static ssize_t vsup_dia_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct tegra_uss_io_proxy *proxy = dev_get_drvdata(dev);
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val == 0)
		gpio_set_value(proxy->vsup_dia_gpio, 0);
	else
		gpio_set_value(proxy->vsup_dia_gpio, 1);

	return count;
}

static ssize_t vsup_dia_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct tegra_uss_io_proxy *proxy = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", gpio_get_value(proxy->vsup_dia_gpio));
}

static DEVICE_ATTR_RW(vsup_dia);

static ssize_t vsup_latch_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct tegra_uss_io_proxy *proxy = dev_get_drvdata(dev);
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val == 0)
		gpio_set_value(proxy->vsup_latch_gpio, 0);
	else
		gpio_set_value(proxy->vsup_latch_gpio, 1);

	return count;
}

static ssize_t vsup_latch_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct tegra_uss_io_proxy *proxy = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", gpio_get_value(proxy->vsup_latch_gpio));
}

static DEVICE_ATTR_RW(vsup_latch);

static ssize_t sensor_gpios_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct tegra_uss_io_proxy *proxy = dev_get_drvdata(dev);
	unsigned long bitmap;
	int rc;
	u32 val;
	u8 *b = (u8 *)&val;

	if (kstrtou32(buf, 16, &val))
		return -EINVAL;

	if (b[2] != 0) {
		bitmap = proxy->g1_bitmap & (~b[2]);
		bitmap |= (b[0] & b[2]);
		rc = gpiod_set_array_value_cansleep(proxy->g1_gpiods->ndescs,
						    proxy->g1_gpiods->desc,
						    proxy->g1_gpiods->info,
						    &bitmap);
		if (rc) {
			dev_err(dev, "set group-1 GPIOs failed %d\n", rc);
			return rc;
		}
		proxy->g1_bitmap = bitmap;
	}

	if (b[3] != 0) {
		bitmap = proxy->g2_bitmap & (~b[3]);
		bitmap |= (b[1] & b[3]);
		rc = gpiod_set_array_value_cansleep(proxy->g2_gpiods->ndescs,
						    proxy->g2_gpiods->desc,
						    proxy->g2_gpiods->info,
						    &bitmap);
		if (rc) {
			dev_err(dev, "set group-2 GPIOs failed %d\n", rc);
			return rc;
		}

		proxy->g2_bitmap = bitmap;
	}

	return count;
}

static ssize_t sensor_gpios_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct tegra_uss_io_proxy *proxy = dev_get_drvdata(dev);
	size_t n = PAGE_SIZE;
	int i;

	n -= snprintf(&buf[PAGE_SIZE - n], n, "Sensor Group 1\n");
	for (i = 0; i < 6; i++) {
		n -= snprintf(&buf[PAGE_SIZE - n], n, "\tSensor %d: %d\n",
			      i + 1, !!(proxy->g1_bitmap & (1 << i)));
	}
	n -= snprintf(&buf[PAGE_SIZE - n], n, "\tVSUP_EN1: %d\n",
		      !!(proxy->g1_bitmap & (1 << 6)));
	n -= snprintf(&buf[PAGE_SIZE - n], n, "\tVSUP_SEL1: %d\n",
		      !!(proxy->g1_bitmap & (1 << 7)));

	n -= snprintf(&buf[PAGE_SIZE - n], n, "Sensor Group 2\n");
	for (i = 0; i < 6; i++) {
		n -= snprintf(&buf[PAGE_SIZE - n], n, "\tSensor %d: %d\n",
			      i + 1, !!(proxy->g2_bitmap & (1 << i)));
	}
	n -= snprintf(&buf[PAGE_SIZE - n], n, "\tVSUP_EN2: %d\n",
		      !!(proxy->g2_bitmap & (1 << 6)));
	n -= snprintf(&buf[PAGE_SIZE - n], n, "\tVSUP_SEL2: %d\n",
		      !!(proxy->g2_bitmap & (1 << 7)));

	return PAGE_SIZE - n;
}

static DEVICE_ATTR_RW(sensor_gpios);

static int tegra_uss_create_dev_attrs(struct platform_device *pdev)
{
	int err;

	err = device_create_file(&pdev->dev, &dev_attr_uss_clk);
	if (err)
		return err;

	err = device_create_file(&pdev->dev, &dev_attr_uss_reset);
	if (err)
		goto err_reset;

	err = device_create_file(&pdev->dev, &dev_attr_vsup_dia);
	if (err)
		goto err_vsup_dia;

	err = device_create_file(&pdev->dev, &dev_attr_vsup_latch);
	if (err)
		goto err_vsup_latch;

	err = device_create_file(&pdev->dev, &dev_attr_sensor_gpios);
	if (err)
		goto err_sensor_gpios;

	return 0;


err_sensor_gpios:
	device_remove_file(&pdev->dev, &dev_attr_vsup_latch);
err_vsup_latch:
	device_remove_file(&pdev->dev, &dev_attr_vsup_dia);
err_vsup_dia:
	device_remove_file(&pdev->dev, &dev_attr_uss_reset);
err_reset:
	device_remove_file(&pdev->dev, &dev_attr_uss_clk);
	return err;
}

static void tegra_uss_remove_dev_attrs(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_uss_clk);
	device_remove_file(&pdev->dev, &dev_attr_uss_reset);
	device_remove_file(&pdev->dev, &dev_attr_vsup_dia);
	device_remove_file(&pdev->dev, &dev_attr_vsup_latch);
	device_remove_file(&pdev->dev, &dev_attr_sensor_gpios);
}

static int tegra_uss_request_gpio(struct platform_device *pdev, const char *name)
{
	struct device_node *np = pdev->dev.of_node;
	int gpio, rc;

	gpio = of_get_named_gpio(np, name, 0);
	if (gpio == -EPROBE_DEFER)
		return gpio;

	if (!gpio_is_valid(gpio)) {
		dev_err(&pdev->dev, "%s is invalid\n", name);
		return -EINVAL;
	}

	rc = devm_gpio_request(&pdev->dev, gpio, name);
	if (rc) {
		dev_err(&pdev->dev, "could not request %s %d\n", name, rc);
		return rc;
	}

	rc = gpio_direction_output(gpio, 0);
	if (rc) {
		dev_err(&pdev->dev, "could not set %s output %d\n", name, rc);
		return rc;
	}

	return gpio;
}

static int tegra_uss_io_proxy_probe(struct platform_device *pdev)
{
	struct tegra_uss_io_proxy *proxy;

	proxy = devm_kzalloc(&pdev->dev, sizeof(*proxy), GFP_KERNEL);
	if (unlikely(proxy == NULL))
		return -ENOMEM;

	proxy->i2s8_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(proxy->i2s8_base)) {
		return dev_err_probe(&pdev->dev, PTR_ERR(proxy->i2s8_base),
				     "failed to get I2S8 mmio\n");
	}

	proxy->uss_clk = devm_clk_get(&pdev->dev, "i2s8");
	if (IS_ERR(proxy->uss_clk)) {
		return dev_err_probe(&pdev->dev, PTR_ERR(proxy->uss_clk),
				     "failed to get I2S8 clock\n");
	}

	proxy->i2s8_reset = devm_reset_control_get(&pdev->dev, "i2s8");
	if (IS_ERR(proxy->i2s8_reset)) {
		return dev_err_probe(&pdev->dev, PTR_ERR(proxy->i2s8_reset),
				     "failed to get I2S8 reset\n");
	}

	proxy->uss_reset_gpio = tegra_uss_request_gpio(pdev, "uss-nres");
	if (proxy->uss_reset_gpio < 0) {
		return dev_err_probe(&pdev->dev, proxy->uss_reset_gpio,
				     "failed to get uss-nres GPIO\n");
	}

	proxy->vsup_dia_gpio = tegra_uss_request_gpio(pdev, "uss-vsup-dia");
	if (proxy->vsup_dia_gpio < 0) {
		return dev_err_probe(&pdev->dev, proxy->vsup_dia_gpio,
				     "failed to get vsup-dia GPIO\n");
	}

	proxy->vsup_latch_gpio = tegra_uss_request_gpio(pdev,
							"uss-vsup-latch");
	if (proxy->vsup_latch_gpio < 0) {
		return dev_err_probe(&pdev->dev, proxy->vsup_latch_gpio,
				     "failed to get vsup-dia GPIO\n");
	}

	proxy->g1_gpiods = devm_gpiod_get_array(&pdev->dev, "sensor-group-1",
						GPIOD_OUT_LOW);
	if (IS_ERR(proxy->g1_gpiods)) {
		return dev_err_probe(&pdev->dev, PTR_ERR(proxy->g1_gpiods),
			"failed to get sensor-group-1 GPIOs\n");
	}

	proxy->g2_gpiods = devm_gpiod_get_array(&pdev->dev, "sensor-group-2",
						GPIOD_OUT_LOW);
	if (IS_ERR(proxy->g2_gpiods)) {
		return dev_err_probe(&pdev->dev, PTR_ERR(proxy->g2_gpiods),
				     "failed to get sensor-group-2 GPIOs\n");
	}

	platform_set_drvdata(pdev, proxy);
	tegra_uss_create_dev_attrs(pdev);

	return 0;
}

static void tegra_uss_reset(struct platform_device *pdev)
{
	struct tegra_uss_io_proxy *proxy = platform_get_drvdata(pdev);
	unsigned long bitmap = 0;
	int rc;

	gpio_set_value(proxy->uss_reset_gpio, 0);

	rc = reset_control_assert(proxy->i2s8_reset);
	if (rc)
		dev_err(&pdev->dev, "i2s8 reset assert failed: %d\n", rc);

	clk_disable_unprepare(proxy->uss_clk);

	rc = gpiod_set_array_value_cansleep(proxy->g1_gpiods->ndescs,
					    proxy->g1_gpiods->desc,
					    proxy->g1_gpiods->info,
					    &bitmap);
	if (rc)
		dev_err(&pdev->dev, "set group-1 GPIOs failed %d\n", rc);

	proxy->g1_bitmap = 0;

	rc = gpiod_set_array_value_cansleep(proxy->g2_gpiods->ndescs,
					    proxy->g2_gpiods->desc,
					    proxy->g2_gpiods->info,
					    &bitmap);
	if (rc)
		dev_err(&pdev->dev, "set group-2 GPIOs failed %d\n", rc);

	proxy->g1_bitmap = 0;

}

static int tegra_uss_io_proxy_remove(struct platform_device *pdev)
{
	tegra_uss_remove_dev_attrs(pdev);
	tegra_uss_reset(pdev);
	return 0;
}

static struct platform_driver tegra_uss_io_proxy_driver = {
	.probe = tegra_uss_io_proxy_probe,
	.remove = tegra_uss_io_proxy_remove,
	.driver = {
		.name	= "tegra-uss-io-proxy",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tegra_uss_io_proxy_of_match),
	},
};

module_platform_driver(tegra_uss_io_proxy_driver);

MODULE_AUTHOR("JC Kuo <jckuo@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra USS IO-PROXY driver");
MODULE_LICENSE("GPL");
