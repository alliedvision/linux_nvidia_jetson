/*
 * Copyright (c) 2015-2022, NVIDIA CORPORATION.  All rights reserved.
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
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/reset.h>
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#define DRIVER_NAME "pwm_tach"

/* Since oscillator clock (38.4MHz) serves as a clock source for
 * the tach input controller, 1.0105263MHz (i.e. 38.4/38) has to be
 * used as a clock value in the RPM calculations
 */
#define TACH_COUNTER_CLK				1010526

#define TACH_FAN_TACH0					0x0
#define TACH_FAN_TACH0_PERIOD_MASK			0x7FFFF
#define TACH_FAN_TACH0_PERIOD_MAX			0x7FFFF
#define TACH_FAN_TACH0_PERIOD_MIN			0x0
#define TACH_FAN_TACH0_WIN_LENGTH_SHIFT			25
#define TACH_FAN_TACH0_WIN_LENGTH_MASK			0x3
#define TACH_FAN_TACH0_OVERFLOW_MASK			BIT(24)

#define TACH_FAN_TACH1					0x4
#define TACH_FAN_TACH1_HI_MASK				0x7FFFF

#define TACH_FAN_TACH_UPPER_THRESHOLD_0			0x8
#define TACH_UPPER_THRESHOLD_MASK			0xffffff
#define TACH_UPPER_THRESHOLD_SHIFT			0

#define TACH_FAN_TACH_LOWER_THRESHOLD_0			0xc
#define TACH_LOWER_THRESHOLD_MASK			0xffffff
#define TACH_LOWER_THRESHOLD_SHIFT			0

#define DEFAULT_UPPER_THRESHOLD				4
#define DEFAULT_LOWER_THRESHOLD				1

#define TACH_FAN_TACH_INTERRUPT_ENABLE_0		0x10
#define TACH_FAN_TACH_INTR_OVERRUN			BIT(0)
#define TACH_FAN_TACH_INTR_UNDERRUN			BIT(1)
#define TACH_FAN_TACH_INTR_CNT_OVERFLOW			BIT(2)
#define TACH_FAN_ENABLE_INTERRUPT_VAL	(TACH_FAN_TACH_INTR_OVERRUN | \
					TACH_FAN_TACH_INTR_UNDERRUN	| \
					TACH_FAN_TACH_INTR_CNT_OVERFLOW)
#define TACH_FAN_ENABLE_INTERRUPT_MASK			0x7
#define TACH_FAN_ENABLE_INTERRUPT_SHIFT			0
#define TACH_FAN_TACH_INTERRUPT_DISABLE			0x0

#define	TACH_FAN_TACH_CONTROL_0				0x14
#define	TACH_FAN_LOAD_CONFIG				BIT(0)
#define	TACH_FAN_STOP_ON_ERR				BIT(1)
#define	TACH_FAN_ERR_CONFIG					BIT(2)
#define TACH_FAN_MONITOR_TIME_MASK				0xffffff00

#define TACH_FAN_TACH_CONTROL_0_MASK			1
#define TACH_FAN_TACH_CONTROL_0_SHIFT			0
#define TACH_ERR_CONFIG_MONITOR_PERIOD_VAL		1
#define TACH_ERR_CONFIG_MONITOR_PULSES_VAL		1

#define TACH_FAN_TACH_ERR_STATUS_0			0x18
#define	TACH_FAN_ERR_OVERRUN				BIT(0)
#define	TACH_FAN_ERR_UNDERRUN				BIT(1)
#define TACH_FAN_ERR_MASK					0x3
#define	TACH_FAN_ERR_PERIOD_MASK			0xFFFFFF00
#define TACH_FAN_ERR_PERIOD_SHIFT			0x8
#define TACH_FAN_INTERRUPT_ENABLE			0x1


struct pwm_tegra_tach_soc_data {
	bool has_interrupt_support;
};

struct pwm_tegra_tach {
	struct device		*dev;
	void __iomem		*regs;
	struct clk		*clk;
	struct reset_control	*rst;
	unsigned int		pulse_per_rev;
	int			irq;
	unsigned int		capture_win_len;
	unsigned int		upper_threshold;
	unsigned int		lower_threshold;
	struct pwm_chip		chip;
	const struct pwm_tegra_tach_soc_data	*soc_data;
};

static ssize_t rpm_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct pwm_tegra_tach *ptt = dev_get_drvdata(dev);
	struct pwm_device *pwm = &ptt->chip.pwms[0];
	struct pwm_capture result;
	unsigned int rpm = 0;
	int ret;

	ret = pwm_capture(pwm, &result, 0);
	if (ret < 0) {
		dev_err(ptt->dev, "Failed to capture PWM: %d\n", ret);
		return ret;
	}

	if (result.period)
		rpm = DIV_ROUND_CLOSEST_ULL(60ULL * NSEC_PER_SEC,
					    result.period);

	return sprintf(buf, "%u\n", rpm);
}

static DEVICE_ATTR_RO(rpm);

static struct attribute *pwm_tach_attrs[] = {
	&dev_attr_rpm.attr,
	NULL,
};

ATTRIBUTE_GROUPS(pwm_tach);

static struct pwm_tegra_tach *to_tegra_pwm_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct pwm_tegra_tach, chip);
}

static u32 tachometer_readl(struct pwm_tegra_tach *ptt, unsigned long reg)
{
	return readl(ptt->regs + reg);
}

static inline void tachometer_writel(struct pwm_tegra_tach *ptt, u32 val,
				     unsigned long reg)
{
	writel(val, ptt->regs + reg);
}

static int tegra_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			    int duty_ns, int period_ns)
{
	/* Dummy implementation for avoiding error from core */
	return 0;
}

static int tegra_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	/* Dummy implementation for avoiding error from core */
	return 0;
}

static void tegra_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	/* Dummy implementation for avoiding error from core */
}

static inline void tach_update_mask(struct pwm_tegra_tach *ptt,
					u32 val, u32 reg_offset, u32 mask,
						u32 bit_offset)
{
	u32 update_val;

	update_val = tachometer_readl(ptt, reg_offset);
	update_val = ((update_val & (~mask)) | (val << bit_offset));
	tachometer_writel(ptt, update_val, reg_offset);
}

static int pwm_tegra_tacho_set_wlen(struct pwm_tegra_tach *ptt,
					int window_length)
{
	u32 tach0, wlen;

	wlen = ffs(window_length) - 1;
	tach0 = tachometer_readl(ptt, TACH_FAN_TACH0);
	tach0 &= ~(TACH_FAN_TACH0_WIN_LENGTH_MASK <<
			TACH_FAN_TACH0_WIN_LENGTH_SHIFT);
	tach0 |= wlen << TACH_FAN_TACH0_WIN_LENGTH_SHIFT;
	tachometer_writel(ptt, tach0, TACH_FAN_TACH0);

	return 0;
}

static int pwm_tegra_tacho_set_capture_wlen(struct pwm_chip *chip,
					    struct pwm_device *pwm,
					    int window_length)
{
	struct pwm_tegra_tach *ptt = to_tegra_pwm_chip(chip);

	if (hweight8(window_length) != 1) {
		dev_err(ptt->dev,
			"Invalid window length,valid values {1, 2, 4 or 8}\n");
		return -EINVAL;
	}

	if (ptt->pulse_per_rev > window_length) {
		dev_err(ptt->dev, "Window length must be pulse per rev (%d)\n",
			ptt->pulse_per_rev);
		return -EINVAL;
	}

	pwm_tegra_tacho_set_wlen(ptt, window_length);

	ptt->capture_win_len = window_length;

	return 0;
}

static void pwm_tegra_tacho_set_threshold(struct pwm_chip *chip)
{
	struct pwm_tegra_tach *ptt = to_tegra_pwm_chip(chip);

	tach_update_mask(ptt, ptt->upper_threshold,
			TACH_FAN_TACH_UPPER_THRESHOLD_0,
			TACH_UPPER_THRESHOLD_MASK,
			TACH_UPPER_THRESHOLD_SHIFT);

	tach_update_mask(ptt, ptt->lower_threshold,
			TACH_FAN_TACH_LOWER_THRESHOLD_0,
			TACH_LOWER_THRESHOLD_MASK,
			TACH_LOWER_THRESHOLD_SHIFT);

}

static int pwm_tegra_tacho_capture(struct pwm_chip *chip,
				   struct pwm_device *pwm,
				   struct pwm_capture *result,
				   unsigned long timeout)
{
	struct pwm_tegra_tach *ptt = to_tegra_pwm_chip(chip);
	unsigned long period;
	u32 tach0;

	tach0 = tachometer_readl(ptt, TACH_FAN_TACH1);
	result->duty_cycle = (tach0 & TACH_FAN_TACH1_HI_MASK);

	tach0 = tachometer_readl(ptt, TACH_FAN_TACH0);
	if (tach0 & TACH_FAN_TACH0_OVERFLOW_MASK) {
		/* Fan is stalled, clear overflow state by writing 1 */
		dev_info(ptt->dev, "Tachometer Overflow is detected\n");
		tachometer_writel(ptt, tach0, TACH_FAN_TACH0);
	}

	period = tach0 & TACH_FAN_TACH0_PERIOD_MASK;
	if ((period == TACH_FAN_TACH0_PERIOD_MIN) ||
	    (period == TACH_FAN_TACH0_PERIOD_MAX)) {
		dev_dbg(ptt->dev, "Period set to min/max (0x%lx), Invalid RPM\n",
			period);
		result->period = 0;
		result->duty_cycle = 0;
		return 0;
	}

	period = period + 1; /* Bug 200046190 */

	period = DIV_ROUND_CLOSEST_ULL(period * ptt->pulse_per_rev * 1000000ULL,
				       ptt->capture_win_len * TACH_COUNTER_CLK);

	/*
	 * period & duty cycles are in units of micro seconds. Hence,
	 * convert them into nano seconds and store it in result.
	 */
	result->period = period * 1000;
	result->duty_cycle = result->duty_cycle * 1000;

	return 0;
}

static irqreturn_t tegra_pwm_tach_irq(int irq, void *dev)
{
	struct pwm_tegra_tach *ptt = dev;
	u32 tach0, period_val;

	/* Read tachometer error status reg to know the status of error */
	tach0 = tachometer_readl(ptt, TACH_FAN_TACH_ERR_STATUS_0);

	/* Clear Interrupts */
	tachometer_writel(ptt, TACH_FAN_ERR_MASK, TACH_FAN_TACH_ERR_STATUS_0);

	/* Disable Interrupt */
	tachometer_writel(ptt, TACH_FAN_TACH_INTERRUPT_DISABLE, TACH_FAN_TACH_INTERRUPT_ENABLE_0);

	/* Get period value captured by TACH controller when the err occurred */
	period_val  = (tach0 >> TACH_FAN_ERR_PERIOD_SHIFT);
	if (tach0 & TACH_FAN_ERR_OVERRUN)
		dev_err(ptt->dev, "Tach overrun error. Period value: 0x%x \n",
			period_val);

	if (tach0 & TACH_FAN_ERR_UNDERRUN)
		dev_err(ptt->dev, "Tach underrun error. Period value: 0x%x \n",
			period_val);

	return IRQ_HANDLED;
}

static const struct pwm_ops pwm_tegra_tach_ops = {
	.config = tegra_pwm_config,
	.enable = tegra_pwm_enable,
	.disable = tegra_pwm_disable,
	.capture = pwm_tegra_tacho_capture,
	.set_capture_window_length = pwm_tegra_tacho_set_capture_wlen,
	.owner = THIS_MODULE,
};

static void pwm_tegra_tach_read_platform_data(struct pwm_tegra_tach *ptt)
{
	struct device_node *np = ptt->dev->of_node;
	u32 pval;
	int ret;

	ret = of_property_read_u32(np, "pulse-per-rev", &pval);
	if (!ret)
		ptt->pulse_per_rev = pval;

	ret = of_property_read_u32(np, "capture-window-length", &pval);
	if (!ret)
		ptt->capture_win_len = pval;

	if (ptt->soc_data->has_interrupt_support) {
	/* get the threshold values only in case of t234 based on chipdata */
		ret = of_property_read_u32(np, "upper-threshold", &pval);
		ptt->upper_threshold = (ret == 0) ? pval : DEFAULT_UPPER_THRESHOLD;

		ret = of_property_read_u32(np, "lower-threshold", &pval);
		ptt->lower_threshold = (ret == 0) ? pval : DEFAULT_LOWER_THRESHOLD;
	}
}

static int pwm_tegra_tach_probe(struct platform_device *pdev)
{
	struct pwm_tegra_tach *ptt;
	struct pwm_device *pwm;
	struct device *hwmon;
	struct resource *r;
	int ret;

	ptt = devm_kzalloc(&pdev->dev, sizeof(*ptt), GFP_KERNEL);
	if (!ptt)
		return -ENOMEM;

	ptt->dev = &pdev->dev;

	ptt->soc_data = of_device_get_match_data(&pdev->dev);
	if (!ptt->soc_data) {
		dev_err(&pdev->dev, "unsupported tegra\n");
		return -ENODEV;
	}

	pwm_tegra_tach_read_platform_data(ptt);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ptt->regs = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(ptt->regs))
		return PTR_ERR(ptt->regs);

	platform_set_drvdata(pdev, ptt);

	ptt->clk = devm_clk_get(&pdev->dev, "tach");
	if (IS_ERR(ptt->clk)) {
		ret = PTR_ERR(ptt->clk);
		if (ret != -EPROBE_DEFER) {
			dev_err(&pdev->dev,
				"Tachometer clock get failed: %d\n", ret);
		}
		return PTR_ERR(ptt->clk);
	}

	ptt->rst = devm_reset_control_get(&pdev->dev, "tach");
	if (IS_ERR(ptt->rst)) {
		ret = PTR_ERR(ptt->rst);
		dev_err(&pdev->dev, "Reset control is not found: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(ptt->clk);
	if (ret) {
		dev_err(&pdev->dev, "Failed to prepare clock: %d\n", ret);
		return ret;
	}

	ret = clk_set_rate(ptt->clk, TACH_COUNTER_CLK);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to set clock rate %d: %d\n",
			TACH_COUNTER_CLK, ret);
		goto clk_unprep;
	}

	ret = reset_control_reset(ptt->rst);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to reset: %d\n", ret);
		goto clk_unprep;
	}

	if (ptt->soc_data->has_interrupt_support) {
		ptt->irq = platform_get_irq(pdev, 0);
		if (ptt->irq < 0) {
			dev_err(&pdev->dev, "platform_get_irq failed\n");
			goto clk_unprep;
		}

		ret = devm_request_irq(&pdev->dev, ptt->irq, tegra_pwm_tach_irq, 0,
			       DRIVER_NAME, ptt);
		if (ret) {
			dev_err(&pdev->dev, "request_irq failed - irq[%d] err[%d]\n",
					ptt->irq, ret);
			goto clk_unprep;
		}
	}

	ptt->chip.dev = &pdev->dev;
	ptt->chip.ops = &pwm_tegra_tach_ops;
	ptt->chip.base = -1;
	ptt->chip.npwm = 1;

	ret = pwmchip_add(&ptt->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to add tachometer PWM: %d\n", ret);
		goto reset_assert;
	}

	/* As per spec, the WIN_LENGTH value should be greater than or equal to
	 * Pulse Per Revolution Value to measure the accurate time period values
	 */

	pwm = &ptt->chip.pwms[0];
	if (ptt->pulse_per_rev > ptt->capture_win_len)
		ptt->capture_win_len = ptt->pulse_per_rev;

	ret = pwm_tegra_tacho_set_capture_wlen(&ptt->chip, pwm, ptt->capture_win_len);
	if (ret < 0) {
		dev_err(ptt->dev, "Failed to set window length: %d\n", ret);
		goto pwm_remove;
	}

	if (ptt->soc_data->has_interrupt_support) {
		/* set upper and lower threshold values */
		pwm_tegra_tacho_set_threshold(&ptt->chip);

		/* program tach fan control reg */
		tach_update_mask(ptt, TACH_ERR_CONFIG_MONITOR_PERIOD_VAL,
				TACH_FAN_TACH_CONTROL_0,
				TACH_FAN_TACH_CONTROL_0_MASK,
				TACH_FAN_TACH_CONTROL_0_SHIFT);

		/* enable interrupts in interrupt enable register */
		tach_update_mask(ptt, TACH_FAN_ENABLE_INTERRUPT_VAL,
				TACH_FAN_TACH_INTERRUPT_ENABLE_0,
				TACH_FAN_ENABLE_INTERRUPT_MASK,
				TACH_FAN_ENABLE_INTERRUPT_SHIFT);

	}

	hwmon = devm_hwmon_device_register_with_groups(&pdev->dev, DRIVER_NAME, ptt, pwm_tach_groups);
	if (IS_ERR(hwmon)) {
		dev_warn(&pdev->dev, "Failed to register hwmon device: %d\n", PTR_ERR_OR_ZERO(hwmon));
		dev_warn(&pdev->dev, "Tegra Tachometer got registered witout hwmon sysfs support\n");
	}

	return 0;

pwm_remove:
	pwmchip_remove(&ptt->chip);

reset_assert:
	reset_control_assert(ptt->rst);

clk_unprep:
	clk_disable_unprepare(ptt->clk);

	return ret;
}

static int pwm_tegra_tach_remove(struct platform_device *pdev)
{
	struct pwm_tegra_tach *ptt = platform_get_drvdata(pdev);

	if (WARN_ON(!ptt))
		return -ENODEV;

	reset_control_assert(ptt->rst);

	clk_disable_unprepare(ptt->clk);

	return pwmchip_remove(&ptt->chip);
}

static int pwm_tegra_tach_suspend(struct device *dev)
{
	return 0;
}

static int pwm_tegra_tach_resume(struct device *dev)
{
	struct pwm_tegra_tach *ptt = dev_get_drvdata(dev);

	pwm_tegra_tacho_set_wlen(ptt, ptt->capture_win_len);

	return 0;
}

static const struct dev_pm_ops pwm_tegra_tach_pm_ops = {
	.suspend        = pwm_tegra_tach_suspend,
	.resume         = pwm_tegra_tach_resume,
};


static struct pwm_tegra_tach_soc_data tegra186_tach_soc_data = {
		.has_interrupt_support = false,
};

static struct pwm_tegra_tach_soc_data tegra194_tach_soc_data = {
		.has_interrupt_support = false,
};

static struct pwm_tegra_tach_soc_data tegra234_tach_soc_data = {
		.has_interrupt_support = false,
};

static const struct of_device_id pwm_tegra_tach_of_match[] = {
	{ .compatible = "nvidia,pwm-tegra186-tachometer",
	  .data =	&tegra186_tach_soc_data,
	},
	{ .compatible = "nvidia,pwm-tegra194-tachometer",
	  .data =	&tegra194_tach_soc_data,
	},
	{ .compatible = "nvidia,pwm-tegra234-tachometer",
	  .data =	&tegra234_tach_soc_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, tegra_tach_of_match);

static struct platform_driver tegra_tach_driver = {
	.driver = {
		.name = "pwm-tegra-tachometer",
		.of_match_table = pwm_tegra_tach_of_match,
		.pm = &pwm_tegra_tach_pm_ops,
	},
	.probe = pwm_tegra_tach_probe,
	.remove = pwm_tegra_tach_remove,
};

module_platform_driver(tegra_tach_driver);

MODULE_DESCRIPTION("PWM based NVIDIA Tegra Tachometer driver");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_AUTHOR("R Raj Kumar <rrajk@nvidia.com>");
MODULE_AUTHOR("Vishwaroop A <va@nvidia.com>");
MODULE_LICENSE("GPL v2");
