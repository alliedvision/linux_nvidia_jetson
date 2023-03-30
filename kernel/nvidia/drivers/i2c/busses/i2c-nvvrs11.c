// SPDX-License-Identifier: GPL-2.0-only
/*
 * Voltage Regulator Specification: VRS11 High Current Voltage Regulator
 *
 * Copyright (C) 2022 NVIDIA CORPORATION. All rights reserved.
 */

#include <linux/i2c.h>
#include <linux/i2c-nvvrs11.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/err.h>

#define VOLTAGE_OFFSET		200 // 0.2V
#define VOLTAGE_SCALE		5   // 5mV

static const struct regmap_range nvvrs11_readable_ranges[] = {
	regmap_reg_range(NVVRS11_REG_VENDOR_ID, NVVRS11_REG_MODEL_REV),
	regmap_reg_range(NVVRS11_REG_VOUT_A, NVVRS11_REG_TEMP_B),
};

static const struct regmap_access_table nvvrs11_readable_table = {
	.yes_ranges = nvvrs11_readable_ranges,
	.n_yes_ranges = ARRAY_SIZE(nvvrs11_readable_ranges),
};

static const struct regmap_config nvvrs11_regmap_config = {
	.name = "nvvrs11",
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = NVVRS11_REG_TEMP_B + 1,
	.cache_type = REGCACHE_RBTREE,
	.rd_table = &nvvrs11_readable_table,
};

static ssize_t show_loopA_rail_name(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct nvvrs11_chip *nvvrs_chip = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", nvvrs_chip->loopA_rail_name);
}

static ssize_t show_loopA_rail_voltage(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct nvvrs11_chip *nvvrs_chip = dev_get_drvdata(dev);
	int voltage_A;

	voltage_A = i2c_smbus_read_byte_data(nvvrs_chip->client, NVVRS11_REG_VOUT_A);
	if (voltage_A < 0)
		return voltage_A;

	voltage_A *= VOLTAGE_SCALE;
	voltage_A += VOLTAGE_OFFSET;

	return sprintf(buf, "%u mV\n", voltage_A);
}

static ssize_t show_loopA_rail_current(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct nvvrs11_chip *nvvrs_chip = dev_get_drvdata(dev);
	int current_A;

	current_A = i2c_smbus_read_byte_data(nvvrs_chip->client, NVVRS11_REG_IOUT_A);
	if (current_A < 0)
		return current_A;

	return sprintf(buf, "%u A\n", current_A);
}

static ssize_t show_loopA_rail_power(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct nvvrs11_chip *nvvrs_chip = dev_get_drvdata(dev);
	unsigned int power;
	int voltage_A;
	int current_A;

	voltage_A = i2c_smbus_read_byte_data(nvvrs_chip->client, NVVRS11_REG_VOUT_A);
	if (voltage_A < 0)
		return voltage_A;

	voltage_A *= VOLTAGE_SCALE;
	voltage_A += VOLTAGE_OFFSET;

	current_A = i2c_smbus_read_byte_data(nvvrs_chip->client, NVVRS11_REG_IOUT_A);
	if (current_A < 0)
		return current_A;

	power = (voltage_A * current_A)/1000;
	return sprintf(buf, "%u W\n", power);
}

static ssize_t show_loopB_rail_name(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct nvvrs11_chip *nvvrs_chip = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", nvvrs_chip->loopB_rail_name);
}

static ssize_t show_loopB_rail_voltage(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct nvvrs11_chip *nvvrs_chip = dev_get_drvdata(dev);
	int voltage_B;

	voltage_B = i2c_smbus_read_byte_data(nvvrs_chip->client, NVVRS11_REG_VOUT_B);
	if (voltage_B < 0)
		return voltage_B;

	voltage_B *= VOLTAGE_SCALE;
	voltage_B += VOLTAGE_OFFSET;

	return sprintf(buf, "%u mV\n", voltage_B);
}

static ssize_t show_loopB_rail_current(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct nvvrs11_chip *nvvrs_chip = dev_get_drvdata(dev);
	int current_B;

	current_B = i2c_smbus_read_byte_data(nvvrs_chip->client, NVVRS11_REG_IOUT_B);
	if (current_B < 0)
		return current_B;

	return sprintf(buf, "%u A\n", current_B);
}

static ssize_t show_loopB_rail_power(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct nvvrs11_chip *nvvrs_chip = dev_get_drvdata(dev);
	unsigned int power;
	int voltage_B;
	int current_B;

	voltage_B = i2c_smbus_read_byte_data(nvvrs_chip->client, NVVRS11_REG_VOUT_B);
	if (voltage_B < 0)
		return voltage_B;

	voltage_B *= VOLTAGE_SCALE;
	voltage_B += VOLTAGE_OFFSET;

	current_B = i2c_smbus_read_byte_data(nvvrs_chip->client, NVVRS11_REG_IOUT_B);
	if (current_B < 0)
		return current_B;

	power = (voltage_B * current_B)/1000;

	return sprintf(buf, "%u W\n", power);
}

static DEVICE_ATTR(loopA_rail_name, S_IRUGO, show_loopA_rail_name, NULL);
static DEVICE_ATTR(loopA_rail_voltage, S_IRUGO, show_loopA_rail_voltage, NULL);
static DEVICE_ATTR(loopA_rail_current, S_IRUGO, show_loopA_rail_current, NULL);
static DEVICE_ATTR(loopA_rail_power, S_IRUGO, show_loopA_rail_power, NULL);
static DEVICE_ATTR(loopB_rail_name, S_IRUGO, show_loopB_rail_name, NULL);
static DEVICE_ATTR(loopB_rail_voltage, S_IRUGO, show_loopB_rail_voltage, NULL);
static DEVICE_ATTR(loopB_rail_current, S_IRUGO, show_loopB_rail_current, NULL);
static DEVICE_ATTR(loopB_rail_power, S_IRUGO, show_loopB_rail_power, NULL);

static struct attribute *nvvrs11_attr[] = {
	&dev_attr_loopA_rail_name.attr,
	&dev_attr_loopA_rail_voltage.attr,
	&dev_attr_loopA_rail_current.attr,
	&dev_attr_loopA_rail_power.attr,
	&dev_attr_loopB_rail_name.attr,
	&dev_attr_loopB_rail_voltage.attr,
	&dev_attr_loopB_rail_current.attr,
	&dev_attr_loopB_rail_power.attr,
	NULL
};

static const struct attribute_group nvvrs11_attr_group = {
	.attrs = nvvrs11_attr,
};

int nvvrs11_create_sys_files(struct device *dev)
{
	return sysfs_create_group(&dev->kobj, &nvvrs11_attr_group);
}

void nvvrs11_delete_sys_files(struct device *dev)
{
	return sysfs_remove_group(&dev->kobj, &nvvrs11_attr_group);
}

static int nvvrs11_vendor_info(struct nvvrs11_chip *chip)
{
	struct i2c_client *client = chip->client;
	int vendor_id, model_rev;

	vendor_id = i2c_smbus_read_byte_data(client, NVVRS11_REG_VENDOR_ID);
	if (vendor_id < 0) {
		dev_err(chip->dev, "Failed to read Vendor ID: %d\n", vendor_id);
		return -EINVAL;
	}

	dev_info(chip->dev, "NVVRS11 Vendor ID: 0x%X \n", vendor_id);

	model_rev = i2c_smbus_read_byte_data(client, NVVRS11_REG_MODEL_REV);
	if (model_rev < 0) {
		dev_err(chip->dev, "Failed to read Model Rev: %d\n", model_rev);
		return -EINVAL;
	}

	dev_info(chip->dev, "NVVRS11 Model Rev: 0x%X\n", model_rev);

	return 0;
}

static int nvvrs11_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	const struct regmap_config *rmap_config;
	struct nvvrs11_chip *nvvrs_chip;
	struct device *dev = &client->dev;
	struct device_node *np = dev->of_node;
	int ret;

	nvvrs_chip = devm_kzalloc(&client->dev, sizeof(*nvvrs_chip), GFP_KERNEL);
	if (!nvvrs_chip)
		return -ENOMEM;

	/* Set PEC flag for SMBUS transfer with PEC enabled */
	client->flags |= I2C_CLIENT_PEC;

	i2c_set_clientdata(client, nvvrs_chip);
	nvvrs_chip->client = client;
	nvvrs_chip->dev = &client->dev;
	rmap_config = &nvvrs11_regmap_config;

	nvvrs_chip->rmap = devm_regmap_init_i2c(client, rmap_config);
	if (IS_ERR(nvvrs_chip->rmap)) {
		ret = PTR_ERR(nvvrs_chip->rmap);
		dev_err(nvvrs_chip->dev, "Failed to initialise regmap: %d\n", ret);
		return ret;
	}

	/* Read device tree node info */
	nvvrs_chip->loopA_rail_name = of_get_property(np, "rail-name-loopA", NULL);
	if (!(nvvrs_chip->loopA_rail_name)) {
		dev_info(nvvrs_chip->dev, "loopA rail does not exist\n");
		nvvrs_chip->loopA_rail_name = "LoopA";
	}

	nvvrs_chip->loopB_rail_name = of_get_property(np, "rail-name-loopB", NULL);
	if (!(nvvrs_chip->loopB_rail_name)) {
		dev_info(nvvrs_chip->dev, "loopB rail does not exist\n");
		nvvrs_chip->loopB_rail_name = "LoopB";
	}

	ret = nvvrs11_create_sys_files(nvvrs_chip->dev);
	if (ret) {
		dev_err(nvvrs_chip->dev, "Failed to add sysfs entries: %d\n", ret);
		return ret;
	}

	ret = nvvrs11_vendor_info(nvvrs_chip);
	if (ret < 0) {
		dev_err(nvvrs_chip->dev, "Failed to read vendor info: %d\n", ret);
		goto exit;
	}

	dev_info(nvvrs_chip->dev, "NVVRS11 probe successful");
	return 0;

exit:
	nvvrs11_delete_sys_files(nvvrs_chip->dev);
	dev_info(nvvrs_chip->dev, "NVVRS11 probe failed");
	return ret;
}

static int nvvrs11_remove(struct i2c_client *client)
{
	nvvrs11_delete_sys_files(&client->dev);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int nvvrs11_i2c_suspend(struct device *dev)
{
	return 0;
}

static int nvvrs11_i2c_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops nvvrs11_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(nvvrs11_i2c_suspend, nvvrs11_i2c_resume)
};

static const struct of_device_id nvvrs_dt_match[] = {
	{ .compatible = "nvidia,vrs11" },
	{}
};

static struct i2c_driver nvvrs11_driver = {
	.driver = {
		.name = "nvvrs11",
		.pm = &nvvrs11_pm_ops,
		.of_match_table = of_match_ptr(nvvrs_dt_match),
	},
	.probe = nvvrs11_probe,
	.remove = nvvrs11_remove,
};

static int __init nvvrs11_init(void)
{
	return i2c_add_driver(&nvvrs11_driver);
}
module_init(nvvrs11_init);

static void __exit nvvrs11_exit(void)
{
	i2c_del_driver(&nvvrs11_driver);
}
module_exit(nvvrs11_exit);

MODULE_DESCRIPTION("Nvidia VRS11: High Current Voltage Regulator Spec");
MODULE_AUTHOR("Shubhi Garg <shgarg@nvidia.com>");
MODULE_ALIAS("i2c:nvvrs11");
MODULE_LICENSE("GPL v2");
