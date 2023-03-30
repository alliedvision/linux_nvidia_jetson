// SPDX-License-Identifier: GPL-2.0+
/*
 * RTC driver for NVIDIA Voltage Regulator Power Sequencer
 *
 * Copyright (C) 2022 NVIDIA CORPORATION. All rights reserved.
 */

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/rtc.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/mfd/nvvrs-pseq.h>
#include <linux/irqdomain.h>
#include <linux/regmap.h>

#define ALARM_RESET_VAL		0xFFFFFFFF /* Alarm reset/disable value */
#define NVVRS_INT_RTC_INDEX	0	   /* Only one RTC interrupt register */
#define	REG_LEN_IN_BYTES	4

struct nvvrs_rtc_driver_data {
	/* Registers offset to I2C addresses map */
	const unsigned int      *map;
	/* RTC IRQ CHIP */
	const struct regmap_irq_chip *rtc_irq_chip;
};

struct nvvrs_rtc_info {
	struct device	   *dev;
	struct i2c_client  *client;
	struct rtc_device       *rtc_dev;
	struct regmap	   *regmap;
	struct regmap_irq_chip_data *rtc_irq_data;
	const struct nvvrs_rtc_driver_data *drv_data;
	struct mutex	    lock;
	unsigned int rtc_irq;
};

/* RTC Registers offsets */
enum nvvrs_rtc_reg_offset {
	RTC_T3 = 0,
	RTC_T2,
	RTC_T1,
	RTC_T0,
	RTC_A3,
	RTC_A2,
	RTC_A1,
	RTC_A0,
	CTL1_REG,
	CTL2_REG,
	RTC_END,
};

static const struct regmap_irq nvvrs_rtc_irq[] = {
	REGMAP_IRQ_REG(NVVRS_INT_RTC_INDEX, 0, NVVRS_PSEQ_INT_SRC1_RTC_MASK),
};

static const struct regmap_irq_chip nvvrs_rtc_irq_chip = {
	.name	   = "nvvrs-rtc",
	.status_base    = NVVRS_PSEQ_REG_INT_SRC1,
	.num_regs       = 1,
	.irqs	   = nvvrs_rtc_irq,
	.num_irqs       = ARRAY_SIZE(nvvrs_rtc_irq),
};

static const unsigned int rtc_map[RTC_END] = {
	[RTC_T3] = NVVRS_PSEQ_REG_RTC_T3,
	[RTC_T2] = NVVRS_PSEQ_REG_RTC_T2,
	[RTC_T1] = NVVRS_PSEQ_REG_RTC_T1,
	[RTC_T0] = NVVRS_PSEQ_REG_RTC_T0,
	[RTC_A3] = NVVRS_PSEQ_REG_RTC_A3,
	[RTC_A2] = NVVRS_PSEQ_REG_RTC_A2,
	[RTC_A1] = NVVRS_PSEQ_REG_RTC_A1,
	[RTC_A0] = NVVRS_PSEQ_REG_RTC_A0,
	[CTL1_REG]  = NVVRS_PSEQ_REG_CTL_1,
	[CTL2_REG]  = NVVRS_PSEQ_REG_CTL_2,
};

static const struct nvvrs_rtc_driver_data rtc_drv_data = {
	.map = rtc_map,
	.rtc_irq_chip = &nvvrs_rtc_irq_chip,
};

static int nvvrs_update_bits(struct nvvrs_rtc_info *info, u8 reg,
				u8 mask, u8 value)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0) {
		dev_err(info->dev, "Failed to read reg:0x%x ret:(%d)\n",
			reg, ret);
		return ret;
	}
	ret = ret & ~mask;

	if (value)
		ret |= mask & 0xFF;

	ret = i2c_smbus_write_byte_data(info->client, reg, ret);
	if (ret < 0)
		dev_err(info->dev, "Failed to write reg:0x%x ret:(%d)\n",
			reg, ret);

	return ret;
}

static int nvvrs_rtc_update_alarm_reg(struct i2c_client *client,
	struct nvvrs_rtc_info *info, u8 *time)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, info->drv_data->map[RTC_A3], time[3]);
	if (ret < 0) {
		dev_err(info->dev, "Failed to write alarm reg: 0x%x ret:(%d)\n",
			info->drv_data->map[RTC_A3], ret);
		goto out;
	}

	ret = i2c_smbus_write_byte_data(client, info->drv_data->map[RTC_A2], time[2]);
	if (ret < 0) {
		dev_err(info->dev, "Failed to write alarm reg: 0x%x ret:(%d)\n",
			info->drv_data->map[RTC_A2], ret);
		goto out;
	}

	ret = i2c_smbus_write_byte_data(client, info->drv_data->map[RTC_A1], time[1]);
	if (ret < 0) {
		dev_err(info->dev, "Failed to write alarm reg: 0x%x ret:(%d)\n",
			info->drv_data->map[RTC_A1], ret);
		goto out;
	}

	ret = i2c_smbus_write_byte_data(client, info->drv_data->map[RTC_A0], time[0]);
	if (ret < 0) {
		dev_err(info->dev, "Failed to write alarm reg: 0x%x ret:(%d)\n",
			info->drv_data->map[RTC_A0], ret);
		goto out;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int nvvrs_rtc_disable_alarm(struct nvvrs_rtc_info *info)
{
	struct i2c_client *client = info->client;
	u8 val[REG_LEN_IN_BYTES];
	int ret;

	/* Clear RTC_WAKE bit */
	ret = nvvrs_update_bits(info, info->drv_data->map[CTL2_REG],
				NVVRS_PSEQ_REG_CTL_2_RTC_WAKE, 0);
	if (ret < 0) {
		dev_err(info->dev, "Failed to clear RTC_WAKE bit (%d)\n", ret);
		goto out;
	}

	/* Clear RTC_PU bit */
	ret = nvvrs_update_bits(info, info->drv_data->map[CTL2_REG],
				NVVRS_PSEQ_REG_CTL_2_RTC_PU, 0);
	if (ret < 0) {
		dev_err(info->dev, "Failed to clear RTC_PU bit (%d)\n", ret);
		goto out;
	}

	/* Write ALARM_RESET_VAL in RTC Alarm register to disable alarm */
	val[0] = 0xFF;
	val[1] = 0xFF;
	val[2] = 0xFF;
	val[3] = 0xFF;

	ret = nvvrs_rtc_update_alarm_reg(client, info, val);
	if (ret < 0) {
		dev_err(info->dev, "Failed to disable Alarm (%d)\n", ret);
		goto out;
	}
out:
	return ret;
}

static irqreturn_t nvvrs_rtc_irq_handler(int irq, void *data)
{
	int ret;
	struct nvvrs_rtc_info *info = data;

	dev_dbg(info->dev, "RTC alarm IRQ: %d\n", irq);

	ret = nvvrs_rtc_disable_alarm(info);
	if (ret < 0)
		dev_err(info->dev, "Failed to disable alarm: ret %d\n", ret);

	rtc_update_irq(info->rtc_dev, 1, RTC_IRQF | RTC_AF);

	return IRQ_HANDLED;
}

static int nvvrs_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct nvvrs_rtc_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	time64_t secs = 0;
	int ret;

	mutex_lock(&info->lock);

	/* Multi-byte transfers are not supported with PEC enabled */
	/* Read MSB first to avoid coherency issues */
	ret = i2c_smbus_read_byte_data(client, info->drv_data->map[RTC_T3]);
	if (ret < 0) {
		dev_err(info->dev, "Failed to read time reg:0x%x ret:(%d)\n",
			info->drv_data->map[RTC_T3], ret);
		goto out;
	}
	secs |= ret << 24;

	ret = i2c_smbus_read_byte_data(client, info->drv_data->map[RTC_T2]);
	if (ret < 0) {
		dev_err(info->dev, "Failed to read time reg:0x%x ret:(%d)\n",
			info->drv_data->map[RTC_T2], ret);
		goto out;
	}
	secs |= ret << 16;

	ret = i2c_smbus_read_byte_data(client, info->drv_data->map[RTC_T1]);
	if (ret < 0) {
		dev_err(info->dev, "Failed to read time reg:0x%x ret:(%d)\n",
			info->drv_data->map[RTC_T1], ret);
		goto out;
	}
	secs |= ret << 8;

	ret = i2c_smbus_read_byte_data(client, info->drv_data->map[RTC_T0]);
	if (ret < 0) {
		dev_err(info->dev, "Failed to read time reg:0x%x ret:(%d)\n",
			info->drv_data->map[RTC_T0], ret);
		goto out;
	}
	secs |= ret;

	rtc_time64_to_tm(secs, tm);
out:
	mutex_unlock(&info->lock);
	return ret;
}

static int nvvrs_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct nvvrs_rtc_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 time[REG_LEN_IN_BYTES];
	unsigned long secs;
	int ret;

	mutex_lock(&info->lock);

	secs = rtc_tm_to_time64(tm);
	time[0] = secs & 0xFF;
	time[1] = (secs >> 8) & 0xFF;
	time[2] = (secs >> 16) & 0xFF;
	time[3] = (secs >> 24) & 0xFF;

	ret = i2c_smbus_write_byte_data(client, info->drv_data->map[RTC_T3], time[3]);
	if (ret < 0) {
		dev_err(info->dev, "Failed to write time reg: 0x%x ret:(%d)\n",
			info->drv_data->map[RTC_T3], ret);
		goto out;
	}

	ret = i2c_smbus_write_byte_data(client, info->drv_data->map[RTC_T2], time[2]);
	if (ret < 0) {
		dev_err(info->dev, "Failed to write time reg: 0x%x ret:(%d)\n",
			info->drv_data->map[RTC_T2], ret);
		goto out;
	}

	ret = i2c_smbus_write_byte_data(client, info->drv_data->map[RTC_T1], time[1]);
	if (ret < 0) {
		dev_err(info->dev, "Failed to write time reg: 0x%x ret:(%d)\n",
			info->drv_data->map[RTC_T1], ret);
		goto out;
	}

	ret = i2c_smbus_write_byte_data(client, info->drv_data->map[RTC_T0], time[0]);
	if (ret < 0) {
		dev_err(info->dev, "Failed to write time reg: 0x%x ret:(%d)\n",
			info->drv_data->map[RTC_T0], ret);
		goto out;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int nvvrs_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct nvvrs_rtc_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	unsigned int alarm_val = 0;
	int ret;

	mutex_lock(&info->lock);

	/* Multi-byte transfers are not supported with PEC enabled */
	ret = i2c_smbus_read_byte_data(client, info->drv_data->map[RTC_A3]);
	if (ret < 0) {
		dev_err(info->dev, "Failed to read alarm reg:0x%x ret:(%d)\n",
			info->drv_data->map[RTC_A3], ret);
		goto out;
	}
	alarm_val |= ret << 24;

	ret = i2c_smbus_read_byte_data(client, info->drv_data->map[RTC_A2]);
	if (ret < 0) {
		dev_err(info->dev, "Failed to read alarm reg:0x%x ret:(%d)\n",
			info->drv_data->map[RTC_A2], ret);
		goto out;
	}

	alarm_val |= ret << 16;
	ret = i2c_smbus_read_byte_data(client, info->drv_data->map[RTC_A1]);
	if (ret < 0) {
		dev_err(info->dev, "Failed to alarm time reg:0x%x ret:(%d)\n",
			info->drv_data->map[RTC_A1], ret);
		goto out;
	}

	alarm_val |= ret << 8;
	ret = i2c_smbus_read_byte_data(client, info->drv_data->map[RTC_A0]);
	if (ret < 0) {
		dev_err(info->dev, "Failed to alarm time reg:0x%x ret:(%d)\n",
			info->drv_data->map[RTC_A0], ret);
		goto out;
	}

	alarm_val |= ret;

	if (alarm_val == ALARM_RESET_VAL)
		alrm->enabled = 0;
	else
		alrm->enabled = 1;

	rtc_time64_to_tm((unsigned long)alarm_val, &alrm->time);
out:
	mutex_unlock(&info->lock);
	return ret;
}

static int nvvrs_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct nvvrs_rtc_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 time[REG_LEN_IN_BYTES];
	unsigned int secs;
	int ret;

	mutex_lock(&info->lock);

	alrm->enabled = 1;
	secs = rtc_tm_to_time64(&alrm->time);
	time[0] = secs & 0xFF;
	time[1] = (secs >> 8) & 0xFF;
	time[2] = (secs >> 16) & 0xFF;
	time[3] = (secs >> 24) & 0xFF;

	ret = nvvrs_rtc_update_alarm_reg(client, info, time);

	mutex_unlock(&info->lock);
	return ret;
}

static int nvvrs_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct nvvrs_rtc_info *info = dev_get_drvdata(dev);
	int ret = 0;

	if (enabled)
		dev_info(info->dev, "Alarm IRQ is already enabled\n");
	else
		ret = nvvrs_rtc_disable_alarm(info);

	return ret;
}

static const struct rtc_class_ops nvvrs_rtc_ops = {
	.read_time = nvvrs_rtc_read_time,
	.set_time = nvvrs_rtc_set_time,
	.read_alarm = nvvrs_rtc_read_alarm,
	.set_alarm = nvvrs_rtc_set_alarm,
	.alarm_irq_enable = nvvrs_rtc_alarm_irq_enable,
};

static int nvvrs_rtc_probe(struct platform_device *pdev)
{
	struct nvvrs_rtc_info *info;
	struct device_node *np;
	struct device *parent;
	struct i2c_client *client;
	int ret = 0;

	np = of_get_child_by_name(pdev->dev.parent->of_node, "rtc");
	if (np && !of_device_is_available(np)) {
		dev_err(&pdev->dev, "Missing rtc device node\n");
		return -ENODEV;
	}

	info = devm_kzalloc(&pdev->dev, sizeof(struct nvvrs_rtc_info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to get irq\n");
		return ret;
	}
	info->rtc_irq = ret;

	mutex_init(&info->lock);
	info->dev = &pdev->dev;
	parent = info->dev->parent;
	client = to_i2c_client(parent);
	client->flags |= I2C_CLIENT_PEC;
	i2c_set_clientdata(client, info);
	info->client = client;
	info->drv_data = &rtc_drv_data;
	platform_set_drvdata(pdev, info);

	/* Initialize regmap */
	info->regmap = dev_get_regmap(parent, NULL);
	if (!info->regmap) {
		dev_err(info->dev, "Failed to get RTC regmap\n");
		return -ENODEV;
	}

	/* RTC as a wakeup source */
	device_init_wakeup(info->dev, 1);

	/* Register RTC device */
	info->rtc_dev = devm_rtc_device_register(info->dev, "nvvrs-rtc",
					&nvvrs_rtc_ops, THIS_MODULE);

	if (IS_ERR(info->rtc_dev)) {
		ret = PTR_ERR(info->rtc_dev);
		dev_err(&pdev->dev, "Failed to register RTC device: %d\n", ret);
		if (ret == 0)
			ret = -EINVAL;
		return ret;
	}

	ret = request_threaded_irq(info->rtc_irq, NULL, nvvrs_rtc_irq_handler, 0,
					"rtc-alarm", info);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to request alarm IRQ: %d: %d\n",
			info->rtc_irq, ret);

	return ret;
}

static int nvvrs_rtc_remove(struct platform_device *pdev)
{
	struct nvvrs_rtc_info *info = platform_get_drvdata(pdev);

	free_irq(info->rtc_irq, info);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int nvvrs_rtc_suspend(struct device *dev)
{
	struct nvvrs_rtc_info *info = dev_get_drvdata(dev);
	int ret = 0;

	if (device_may_wakeup(dev)) {
		/* Set RTC_WAKE bit for auto wake system from suspend state */
		ret = nvvrs_update_bits(info, info->drv_data->map[CTL2_REG],
					NVVRS_PSEQ_REG_CTL_2_RTC_WAKE, 0x1);
		if (ret < 0) {
			dev_err(info->dev, "Failed to set RTC_WAKE bit (%d)\n", ret);
			return ret;
		}

		ret = enable_irq_wake(info->rtc_irq);
	}

	return ret;
}

static int nvvrs_rtc_resume(struct device *dev)
{
	struct nvvrs_rtc_info *info = dev_get_drvdata(dev);
	int ret = 0;

	if (device_may_wakeup(dev)) {
		/* Disable auto wake */
		ret = nvvrs_update_bits(info, info->drv_data->map[CTL2_REG],
					NVVRS_PSEQ_REG_CTL_2_RTC_WAKE, 0x0);
		if (ret < 0) {
			dev_err(info->dev, "Failed to clear RTC_WAKE bit (%d)\n", ret);
			return ret;
		}

		return disable_irq_wake(info->rtc_irq);
	}

	return ret;
}

#endif
static SIMPLE_DEV_PM_OPS(nvvrs_rtc_pm_ops, nvvrs_rtc_suspend, nvvrs_rtc_resume);

static void nvvrs_rtc_shutdown(struct platform_device *pdev)
{
	/* TODO */
}

static struct platform_driver nvvrs_rtc_driver = {
	.driver		= {
		.name   = "nvvrs-pseq-rtc",
		.pm     = &nvvrs_rtc_pm_ops,
	},
	.probe		= nvvrs_rtc_probe,
	.remove		= nvvrs_rtc_remove,
	.shutdown	= nvvrs_rtc_shutdown,
};

module_platform_driver(nvvrs_rtc_driver);

MODULE_DESCRIPTION("NVVRS PSEQ RTC driver");
MODULE_AUTHOR("Shubhi Garg <shgarg@nvidia.com>");
MODULE_LICENSE("GPL v2");
