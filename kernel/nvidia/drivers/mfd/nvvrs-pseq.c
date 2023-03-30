// SPDX-License-Identifier: GPL-2.0-only
/*
 * Voltage Regulator Specification: Power Sequencer MFD Driver
 *
 * Copyright (C) 2020-2023 NVIDIA CORPORATION. All rights reserved.
 */

#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/mfd/core.h>
#include <linux/mfd/nvvrs-pseq.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/err.h>

static const struct resource rtc_resources[] = {
	DEFINE_RES_IRQ(NVVRS_PSEQ_INT_SRC1_RTC),
};

static const struct resource wdt_resources[] = {
	DEFINE_RES_IRQ(NVVRS_PSEQ_INT_SRC1_WDT),
};

static const struct regmap_irq nvvrs_pseq_irqs[] = {
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_SRC1_RSTIRQ, 0, NVVRS_PSEQ_INT_SRC1_RSTIRQ_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_SRC1_OSC, 0, NVVRS_PSEQ_INT_SRC1_OSC_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_SRC1_EN, 0, NVVRS_PSEQ_INT_SRC1_EN_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_SRC1_RTC, 0, NVVRS_PSEQ_INT_SRC1_RTC_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_SRC1_PEC, 0, NVVRS_PSEQ_INT_SRC1_PEC_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_SRC1_WDT, 0, NVVRS_PSEQ_INT_SRC1_WDT_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_SRC1_EM_PD, 0, NVVRS_PSEQ_INT_SRC1_EM_PD_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_SRC1_INTERNAL, 0, NVVRS_PSEQ_INT_SRC1_INTERNAL_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_SRC2_PBSP, 1, NVVRS_PSEQ_INT_SRC2_PBSP_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_SRC2_ECC_DED, 1, NVVRS_PSEQ_INT_SRC2_ECC_DED_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_SRC2_TSD, 1, NVVRS_PSEQ_INT_SRC2_TSD_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_SRC2_LDO, 1, NVVRS_PSEQ_INT_SRC2_LDO_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_SRC2_BIST, 1, NVVRS_PSEQ_INT_SRC2_BIST_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_SRC2_RT_CRC, 1, NVVRS_PSEQ_INT_SRC2_RT_CRC_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_SRC2_VENDOR, 1, NVVRS_PSEQ_INT_SRC2_VENDOR_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_VENDOR0, 2, NVVRS_PSEQ_INT_VENDOR0_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_VENDOR1, 2, NVVRS_PSEQ_INT_VENDOR1_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_VENDOR2, 2, NVVRS_PSEQ_INT_VENDOR2_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_VENDOR3, 2, NVVRS_PSEQ_INT_VENDOR3_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_VENDOR4, 2, NVVRS_PSEQ_INT_VENDOR4_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_VENDOR5, 2, NVVRS_PSEQ_INT_VENDOR5_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_VENDOR6, 2, NVVRS_PSEQ_INT_VENDOR6_MASK),
	REGMAP_IRQ_REG(NVVRS_PSEQ_INT_VENDOR7, 2, NVVRS_PSEQ_INT_VENDOR7_MASK),
};

static const struct mfd_cell nvvrs_pseq_children[] = {
	{
		.name = "nvvrs-pseq-rtc",
		.resources = rtc_resources,
		.num_resources = ARRAY_SIZE(rtc_resources),
	},
};

static const struct regmap_range nvvrs_pseq_readable_ranges[] = {
	regmap_reg_range(NVVRS_PSEQ_REG_VENDOR_ID, NVVRS_PSEQ_REG_MODEL_REV),
	regmap_reg_range(NVVRS_PSEQ_REG_INT_SRC1, NVVRS_PSEQ_REG_LAST_RST),
	regmap_reg_range(NVVRS_PSEQ_REG_EN_ALT_F, NVVRS_PSEQ_REG_IEN_VENDOR),
	regmap_reg_range(NVVRS_PSEQ_REG_RTC_T3, NVVRS_PSEQ_REG_RTC_A0),
	regmap_reg_range(NVVRS_PSEQ_REG_WDT_CFG, NVVRS_PSEQ_REG_WDTKEY),
};

static const struct regmap_access_table nvvrs_pseq_readable_table = {
	.yes_ranges = nvvrs_pseq_readable_ranges,
	.n_yes_ranges = ARRAY_SIZE(nvvrs_pseq_readable_ranges),
};

static const struct regmap_range nvvrs_pseq_writable_ranges[] = {
	regmap_reg_range(NVVRS_PSEQ_REG_INT_SRC1, NVVRS_PSEQ_REG_INT_VENDOR),
	regmap_reg_range(NVVRS_PSEQ_REG_GP_OUT, NVVRS_PSEQ_REG_IEN_VENDOR),
	regmap_reg_range(NVVRS_PSEQ_REG_RTC_T3, NVVRS_PSEQ_REG_RTC_A0),
	regmap_reg_range(NVVRS_PSEQ_REG_WDT_CFG, NVVRS_PSEQ_REG_WDTKEY),
};

static const struct regmap_access_table nvvrs_pseq_writable_table = {
	.yes_ranges = nvvrs_pseq_writable_ranges,
	.n_yes_ranges = ARRAY_SIZE(nvvrs_pseq_writable_ranges),
};

static const struct regmap_config nvvrs_pseq_regmap_config = {
	.name = "power-slave",
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = NVVRS_PSEQ_REG_WDTKEY + 1,
	.cache_type = REGCACHE_RBTREE,
	.rd_table = &nvvrs_pseq_readable_table,
	.wr_table = &nvvrs_pseq_writable_table,
};

static int nvvrs_pseq_irq_clear(void *irq_drv_data)
{
	struct nvvrs_pseq_chip *chip = (struct nvvrs_pseq_chip *)irq_drv_data;
	struct i2c_client *client = chip->client;
	unsigned int reg, val;
	int ret = 0, i;

	/* Write 1 to clear the interrupt bit in the Interrupt
	 * Source Register, writing 0 has no effect, writing 1 to a bit
	 * which is already at 0 has no effect
	 */

	for (i = 0; i < chip->irq_chip->num_regs; i++) {
		reg = chip->irq_chip->status_base + i;
		ret = i2c_smbus_read_byte_data(client, reg);
		if (ret < 0) {
			dev_err(chip->dev, "Failed to read interrupt register: %u, \
				ret=%d\n", reg, ret);
			return -EINVAL;
		} else if (ret > 0) {
			val = (unsigned int)ret;
			dev_info(chip->dev, "CAUTION: interrupt status reg:0x%x set to 0x%x\n", \
				reg, val);
			dev_info(chip->dev, "Clearing interrupts\n");

			/* Clear interrupt */
			ret = i2c_smbus_write_byte_data(client, reg, val);
			if (ret < 0) {
				dev_err(chip->dev, "Failed to write interrupt register: %u, \
					ret= %d\n", reg, ret);
				return ret;
			}
		}
	}

	return ret;
}

static struct regmap_irq_chip nvvrs_pseq_irq_chip = {
	.name = "nvvrs-pseq-irq",
	.irqs = nvvrs_pseq_irqs,
	.num_irqs = ARRAY_SIZE(nvvrs_pseq_irqs),
	.num_regs = 3,
	.status_base = NVVRS_PSEQ_REG_INT_SRC1,
	.handle_post_irq = nvvrs_pseq_irq_clear,
};

static int nvvrs_pseq_configure(struct nvvrs_pseq_chip *chip)
{
	/* This function is kept empty, PSEQ will be configured
	 * according to requirements
	 */
	return 0;
}

static int nvvrs_pseq_vendor_info(struct nvvrs_pseq_chip *chip)
{
	struct i2c_client *client = chip->client;
	unsigned int vendor_id, model_rev;
	int ret;

	ret = i2c_smbus_read_byte_data(client, NVVRS_PSEQ_REG_VENDOR_ID);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read Vendor ID: %d\n", ret);
		return -EINVAL;
	}

	vendor_id = (unsigned int)ret;
	dev_info(chip->dev, "NVVRS Vendor ID: 0x%X \n", vendor_id);

	ret = i2c_smbus_read_byte_data(client, NVVRS_PSEQ_REG_MODEL_REV);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read Model Rev: %d\n", ret);
		return -EINVAL;
	}

	model_rev = (unsigned int)ret;

	if (model_rev < 0x40) {
		dev_info(chip->dev, "NVVRS Chip Rev: 0x%X which is < 0x40\n", model_rev);
		dev_info(chip->dev, "Silicon issues thus exiting...\n");
		return -EINVAL;
	}

	dev_info(chip->dev, "NVVRS Model Rev: 0x%X\n", model_rev);

	return 0;
}

static int nvvrs_pseq_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	const struct regmap_config *rmap_config;
	struct nvvrs_pseq_chip *nvvrs_chip;
	const struct mfd_cell *mfd_cells;
	int n_mfd_cells;
	int ret;

	nvvrs_chip = devm_kzalloc(&client->dev, sizeof(*nvvrs_chip), GFP_KERNEL);
	if (!nvvrs_chip)
		return -ENOMEM;

	/* Set PEC flag for SMBUS transfer with PEC enabled */
	client->flags |= I2C_CLIENT_PEC;

	i2c_set_clientdata(client, nvvrs_chip);
	nvvrs_chip->client = client;
	nvvrs_chip->dev = &client->dev;
	nvvrs_chip->chip_irq = client->irq;
	mfd_cells = nvvrs_pseq_children;
	n_mfd_cells = ARRAY_SIZE(nvvrs_pseq_children);
	rmap_config = &nvvrs_pseq_regmap_config;
	nvvrs_chip->irq_chip = &nvvrs_pseq_irq_chip;

	nvvrs_chip->rmap = devm_regmap_init_i2c(client, rmap_config);
	if (IS_ERR(nvvrs_chip->rmap)) {
		ret = PTR_ERR(nvvrs_chip->rmap);
		dev_err(nvvrs_chip->dev, "Failed to initialise regmap: %d\n", ret);
		return ret;
	}

	ret = nvvrs_pseq_vendor_info(nvvrs_chip);
	if (ret < 0) {
		dev_err(nvvrs_chip->dev, "Invalid vendor info: %d\n", ret);
		return ret;
	}

	/* When battery mounted, the chip may have IRQ asserted. */
	/* Clear it before IRQ requested. */
	ret = nvvrs_pseq_irq_clear(nvvrs_chip);
	if (ret < 0) {
		dev_err(nvvrs_chip->dev, "Failed to clear IRQ: %d\n", ret);
		return ret;
	}

	nvvrs_pseq_irq_chip.irq_drv_data = nvvrs_chip;
	ret = devm_regmap_add_irq_chip(nvvrs_chip->dev, nvvrs_chip->rmap, client->irq,
				       IRQF_ONESHOT | IRQF_SHARED, 0,
				       &nvvrs_pseq_irq_chip,
				       &nvvrs_chip->irq_data);
	if (ret < 0) {
		dev_err(nvvrs_chip->dev, "Failed to add regmap irq: %d\n", ret);
		return ret;
	}

	ret = nvvrs_pseq_configure(nvvrs_chip);
	if (ret < 0)
		return ret;

	ret =  devm_mfd_add_devices(nvvrs_chip->dev, PLATFORM_DEVID_NONE,
				    mfd_cells, n_mfd_cells, NULL, 0,
				    regmap_irq_get_domain(nvvrs_chip->irq_data));
	if (ret < 0) {
		dev_err(nvvrs_chip->dev, "Failed to add MFD children: %d\n", ret);
		return ret;
	}

	dev_info(nvvrs_chip->dev, "NVVRS PSEQ probe successful");
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int nvvrs_pseq_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	/*
	 * IRQ must be disabled during suspend because if it happens
	 * while suspended it will be handled before resuming I2C.
	 *
	 * When device is woken up from suspend (e.g. by RTC wake alarm),
	 * an interrupt occurs before resuming I2C bus controller.
	 * Interrupt handler tries to read registers but this read
	 * will fail because I2C is still suspended.
	 */
	disable_irq(client->irq);

	return 0;
}

static int nvvrs_pseq_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	enable_irq(client->irq);
	return 0;
}
#endif

static const struct dev_pm_ops nvvrs_pseq_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(nvvrs_pseq_i2c_suspend, nvvrs_pseq_i2c_resume)
};

static const struct of_device_id nvvrs_dt_match[] = {
	{ .compatible = "nvidia,vrs-pseq" },
	{}
};

static struct i2c_driver nvvrs_pseq_driver = {
	.driver = {
		.name = "nvvrs_pseq",
		.pm = &nvvrs_pseq_pm_ops,
		.of_match_table = of_match_ptr(nvvrs_dt_match),
	},
	.probe = nvvrs_pseq_probe,
};

static int __init nvvrs_pseq_init(void)
{
	return i2c_add_driver(&nvvrs_pseq_driver);
}
subsys_initcall(nvvrs_pseq_init);

static void __exit nvvrs_pseq_exit(void)
{
	i2c_del_driver(&nvvrs_pseq_driver);
}
module_exit(nvvrs_pseq_exit);

MODULE_DESCRIPTION("Voltage Regulator Spec Power Sequencer Multi Function Device Core Driver");
MODULE_AUTHOR("Shubhi Garg <shgarg@nvidia.com>");
MODULE_ALIAS("i2c:nvvrs-pseq");
MODULE_LICENSE("GPL v2");
