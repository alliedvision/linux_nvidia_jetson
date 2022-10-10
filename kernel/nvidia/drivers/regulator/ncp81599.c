// SPDX-License-Identifier: GPL-2.0
/*
 * NCP81599 I2C controlled driver
 *
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

#define NCP81599_EN_REG 	0x00
#define NCP81599_EN_MASK	BIT(2)
#define NCP81599_EN_INT		BIT(3)
#define NCP81599_ENABLE_VAL	(NCP81599_EN_MASK | NCP81599_EN_INT)
#define NCP81599_DISABLE_VAL	NCP81599_EN_MASK

struct ncp81599_regulator {
	struct regulator_desc desc;
	struct i2c_client *client;
	struct device *dev;
};

static int ncp81599_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct ncp81599_regulator *ncp_reg = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = i2c_smbus_read_byte_data(ncp_reg->client, NCP81599_EN_REG);

	if (ret == NCP81599_ENABLE_VAL)
		return 1;

	return 0;
}

static int ncp81599_regulator_enable(struct regulator_dev *rdev)
{
	struct ncp81599_regulator *ncp_reg = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = i2c_smbus_write_byte_data(ncp_reg->client, NCP81599_EN_REG,
						NCP81599_ENABLE_VAL);
	if (ret)
		dev_err(ncp_reg->dev, "Failed to enable regulator: %d\n", ret);

	return ret;
}

static int ncp81599_regulator_disable(struct regulator_dev *rdev)
{
	struct ncp81599_regulator *ncp_reg = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = i2c_smbus_write_byte_data(ncp_reg->client, NCP81599_EN_REG,
						NCP81599_DISABLE_VAL);
	if (ret)
		dev_err(ncp_reg->dev, "Failed to disable regulator: %d\n", ret);

	return ret;
}

static const struct regulator_init_data ncp81599_regulator_default = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
};

static const struct regulator_ops ncp81599_regulator_ops = {
	.is_enabled = ncp81599_regulator_is_enabled,
	.enable = ncp81599_regulator_enable,
	.disable = ncp81599_regulator_disable,
};

static int ncp81599_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct ncp81599_regulator *ncp_reg;
	struct regulator_dev *rdev = NULL;
	struct device *dev = &client->dev;
	struct device_node *np = dev->of_node;
	struct regulator_desc *rdesc;
	struct regulator_config config = { };
	int ret = 0;

	ncp_reg = devm_kzalloc(&client->dev, sizeof(*ncp_reg), GFP_KERNEL);
	if (!ncp_reg)
		return -ENOMEM;

	i2c_set_clientdata(client, ncp_reg);
	ncp_reg->client = client;
	ncp_reg->dev = &client->dev;

	rdesc = &ncp_reg->desc;
	rdesc->type = REGULATOR_VOLTAGE;
	rdesc->owner = THIS_MODULE;
	rdesc->ops = &ncp81599_regulator_ops;

	ret = of_property_read_string(np, "regulator-name", &rdesc->name);
	if (ret) {
		dev_err(dev, "read string regulator-name failed\n");
		return ret;
	}

	rdesc->supply_name = rdesc->name;
	config.dev = dev;
	config.driver_data = ncp_reg;
	config.of_node = client->dev.of_node;
	config.init_data = &ncp81599_regulator_default;

	rdev = devm_regulator_register(dev, rdesc, &config);
	if (IS_ERR(rdev)) {
		ret = PTR_ERR(rdev);
		dev_err(dev, "Regulator registration %s failed: %d\n",
			rdesc->name, ret);
		return ret;
	}

	dev_info(ncp_reg->dev, "NCP81599 probe is successful");
	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int ncp81599_i2c_suspend(struct device *dev)
{
	return 0;
}

static int ncp81599_i2c_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops ncp81599_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ncp81599_i2c_suspend, ncp81599_i2c_resume)
};

static const struct of_device_id ncp81599_dt_match[] = {
	{ .compatible = "nvidia,ncp81599" },
	{}
};

static struct i2c_driver ncp81599_driver = {
	.driver = {
		.name = "ncp81599",
		.pm = &ncp81599_pm_ops,
		.of_match_table = of_match_ptr(ncp81599_dt_match),
	},
	.probe = ncp81599_probe,
};

static int __init ncp81599_init(void)
{
	return i2c_add_driver(&ncp81599_driver);
}
module_init(ncp81599_init);

static void __exit ncp81599_exit(void)
{
	i2c_del_driver(&ncp81599_driver);
}
module_exit(ncp81599_exit);

MODULE_DESCRIPTION("NCP81599");
MODULE_AUTHOR("Shubhi Garg <shgarg@nvidia.com>");
MODULE_ALIAS("i2c:ncp81599");
MODULE_LICENSE("GPL v2");
