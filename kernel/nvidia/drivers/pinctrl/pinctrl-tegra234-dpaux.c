/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Prathamesh Shete <pshete@nvidia.com>
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

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinconf-generic.h>

#include "core.h"
#include "pinctrl-utils.h"
#define PADCTLREG_I2C_DPAUX		0x4000
#define I2C_SDA_INPUT			BIT(15)
#define I2C_SCL_INPUT			BIT(14)
#define MODE				BIT(11)
#define PAD_PWR				BIT(0)
#define SEL				BIT(31)

struct tegra_dpaux_function {
	const char *name;
	const char * const *groups;
	unsigned int ngroups;
};

struct tegra_dpaux_pingroup {
	const char *name;
	const unsigned int pins[1];
	u8 npins;
	u8 funcs[2];
};

struct dpaux_context {
	u32 val_padctl;
};

struct tegra_dpaux_pinctl {
	struct device *dev;
	void __iomem *regs;
	struct platform_device *pdev;

	struct pinctrl_desc desc;
	struct pinctrl_dev *pinctrl;

	const struct pinctrl_pin_desc *pins;
	unsigned int npins;
	const struct tegra_dpaux_function *functions;
	unsigned int nfunctions;
	const struct tegra_dpaux_pingroup *groups;
	unsigned int ngroups;
	struct dpaux_context dpaux_context;
};

struct tegra_dpaux_chip_data {
	const struct pinctrl_pin_desc *pins;
	u32 npins;
	const struct tegra_dpaux_pingroup *pin_group;
	u32 npin_groups;
	struct tegra_dpaux_function *functions;
	u32 nfunctions;
};

#define TEGRA_PIN_DPAUX_0 0

static const struct pinctrl_pin_desc tegra234_dpaux_pins[] = {
	PINCTRL_PIN(TEGRA_PIN_DPAUX_0, "dpaux-0"),
};

enum tegra_dpaux_mux {
	TEGRA_DPAUX_MUX_I2C,
	TEGRA_DPAUX_MUX_DISPLAY,
};

#define TEGRA234_PIN_NAMES "dpaux-0"

static const char * const tegra234_dpaux_pin_groups[] = {
	TEGRA234_PIN_NAMES
};

#define FUNCTION(fname, group)			\
	{					\
		.name = #fname,			\
		.groups = group,		\
		.ngroups = ARRAY_SIZE(group),	\
	}

static struct tegra_dpaux_function tegra234_dpaux_functions[] = {
	FUNCTION(i2c, tegra234_dpaux_pin_groups),
	FUNCTION(display, tegra234_dpaux_pin_groups),
};

#define PINGROUP(pg_name, pin_id, f0, f1)		\
	{						\
		.name = #pg_name,			\
		.pins = {TEGRA_PIN_##pin_id},		\
		.npins = 1,				\
		.funcs = {				\
			TEGRA_DPAUX_MUX_##f0,		\
			TEGRA_DPAUX_MUX_##f1,		\
		},					\
	}
static const struct tegra_dpaux_pingroup tegra234_dpaux_groups[] = {
	PINGROUP(dpaux_0, DPAUX_0, I2C, DISPLAY),
};

static struct tegra_dpaux_chip_data tegra234_dpaux_chip_data[] = {
	{
		.pins = tegra234_dpaux_pins,
		.npins = ARRAY_SIZE(tegra234_dpaux_pins),
		.pin_group = tegra234_dpaux_groups,
		.npin_groups = ARRAY_SIZE(tegra234_dpaux_groups),
		.functions = tegra234_dpaux_functions,
		.nfunctions = ARRAY_SIZE(tegra234_dpaux_functions),
	},
};

static void tegra_dpaux_update(struct tegra_dpaux_pinctl *tdp_aux,
				u32 reg_offset, u32 mask, u32 val)
{
	u32 rval;

	rval = __raw_readl(tdp_aux->regs + reg_offset);
	rval = (rval & ~mask) | (val & mask);
	__raw_writel(rval, tdp_aux->regs + reg_offset);
}

static int tegra_dpaux_pinctrl_set_mode(struct tegra_dpaux_pinctl *tdpaux_ctl,
					unsigned int function)
{
	u32 mask;

	mask = I2C_SDA_INPUT | I2C_SCL_INPUT | MODE;

	if (function == TEGRA_DPAUX_MUX_DISPLAY)
		tegra_dpaux_update(tdpaux_ctl, PADCTLREG_I2C_DPAUX, mask, 0);
	else if (function == TEGRA_DPAUX_MUX_I2C) {
		tegra_dpaux_update(tdpaux_ctl, PADCTLREG_I2C_DPAUX, SEL, SEL);
		tegra_dpaux_update(tdpaux_ctl, PADCTLREG_I2C_DPAUX, mask, mask);
	}

	tegra_dpaux_update(tdpaux_ctl, PADCTLREG_I2C_DPAUX, PAD_PWR, 0);

	return 0;
}

static int tegra_dpaux_pinctrl_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct tegra_dpaux_pinctl *padctl = pinctrl_dev_get_drvdata(pctldev);

	return padctl->npins;
}

static const char *tegra_dpaux_pinctrl_get_group_name(
	struct pinctrl_dev *pctldev, unsigned int group)
{
	struct tegra_dpaux_pinctl *padctl = pinctrl_dev_get_drvdata(pctldev);

	return padctl->pins[group].name;
}

static const struct pinctrl_ops tegra_dpaux_pinctrl_ops = {
	.get_groups_count = tegra_dpaux_pinctrl_get_groups_count,
	.get_group_name = tegra_dpaux_pinctrl_get_group_name,
	.dt_node_to_map = pinconf_generic_dt_node_to_map_pin,
	.dt_free_map = pinctrl_utils_free_map,
};

static int tegra234_dpaux_get_functions_count(struct pinctrl_dev *pctldev)
{
	struct tegra_dpaux_pinctl *padctl = pinctrl_dev_get_drvdata(pctldev);

	return padctl->nfunctions;
}

static const char *tegra234_dpaux_get_function_name(struct pinctrl_dev *pctldev,
						    unsigned int function)
{
	struct tegra_dpaux_pinctl *padctl = pinctrl_dev_get_drvdata(pctldev);

	return padctl->functions[function].name;
}

static int tegra234_dpaux_get_function_groups(struct pinctrl_dev *pctldev,
					      unsigned int function,
					      const char * const **groups,
					      unsigned * const num_groups)
{
	struct tegra_dpaux_pinctl *padctl = pinctrl_dev_get_drvdata(pctldev);

	*num_groups = padctl->functions[function].ngroups;
	*groups = padctl->functions[function].groups;

	return 0;
}

static int tegra_dpaux_pinctrl_set_mux(struct pinctrl_dev *pctldev,
				       unsigned int function,
				       unsigned int group)
{
	struct tegra_dpaux_pinctl *padctl = pinctrl_dev_get_drvdata(pctldev);
	const struct tegra_dpaux_pingroup *g;
	int i;

	g = &padctl->groups[group];
	for (i = 0; i < ARRAY_SIZE(g->funcs); i++) {
		if (g->funcs[i] == function)
			break;
	}
	if (i == ARRAY_SIZE(g->funcs))
		return -EINVAL;

	return tegra_dpaux_pinctrl_set_mode(padctl, function);
}

static const struct pinmux_ops tegra_dpaux_pinmux_ops = {
	.get_functions_count = tegra234_dpaux_get_functions_count,
	.get_function_name = tegra234_dpaux_get_function_name,
	.get_function_groups = tegra234_dpaux_get_function_groups,
	.set_mux = tegra_dpaux_pinctrl_set_mux,
};

static int tegra234_dpaux_pinctrl_probe(struct platform_device *pdev)
{
	struct tegra_dpaux_chip_data *cdata;
	struct tegra_dpaux_pinctl *tdpaux_ctl;
	int ret;

	tdpaux_ctl = devm_kzalloc(&pdev->dev, sizeof(*tdpaux_ctl), GFP_KERNEL);
	if (!tdpaux_ctl)
		return -ENOMEM;

	tdpaux_ctl->dev = &pdev->dev;
	cdata = (struct tegra_dpaux_chip_data *)
				of_device_get_match_data(&pdev->dev);

	if (!cdata) {
		dev_err(&pdev->dev, "no device match found for dpaux_pinctrl\n");
		return -EINVAL;
	}

	tdpaux_ctl->pdev = pdev;
	tdpaux_ctl->regs = devm_ioremap_resource(&pdev->dev,
			 platform_get_resource(pdev, IORESOURCE_MEM, 0));
	if (!tdpaux_ctl->regs) {
		dev_err(&pdev->dev, "Unable to map resource");
		return -EINVAL;
	}

	tdpaux_ctl->pins = cdata->pins;
	tdpaux_ctl->npins = cdata->npins;
	tdpaux_ctl->functions = cdata->functions;
	tdpaux_ctl->nfunctions = cdata->nfunctions;
	tdpaux_ctl->groups = cdata->pin_group;
	tdpaux_ctl->ngroups = cdata->npin_groups;

	memset(&tdpaux_ctl->desc, 0, sizeof(tdpaux_ctl->desc));
	tdpaux_ctl->desc.name = dev_name(&pdev->dev);
	tdpaux_ctl->desc.pins = tdpaux_ctl->pins;
	tdpaux_ctl->desc.npins = tdpaux_ctl->npins;
	tdpaux_ctl->desc.pctlops = &tegra_dpaux_pinctrl_ops;
	tdpaux_ctl->desc.pmxops = &tegra_dpaux_pinmux_ops;
	tdpaux_ctl->desc.owner = THIS_MODULE;
	platform_set_drvdata(pdev, tdpaux_ctl);

	tdpaux_ctl->pinctrl = devm_pinctrl_register(&pdev->dev,
				 &tdpaux_ctl->desc, tdpaux_ctl);
	if (IS_ERR(tdpaux_ctl->pinctrl)) {
		ret = PTR_ERR(tdpaux_ctl->pinctrl);
		dev_err(&pdev->dev, "Failed to register dpaux pinctrl: %d\n",
			ret);
		return ret;
	}

	printk("DP-AUX Probe successful");
	return 0;
}

static int tegra_dpaux_remove(struct platform_device *pdev)
{
	return 0;
}

static void tegra234_dpaux_save(struct tegra_dpaux_pinctl *dpaux_ctl)
{
	dpaux_ctl->dpaux_context.val_padctl =
		__raw_readl(dpaux_ctl->regs + PADCTLREG_I2C_DPAUX);
}

static void tegra234_dpaux_restore(struct tegra_dpaux_pinctl *dpaux_ctl)
{
	__raw_writel(dpaux_ctl->dpaux_context.val_padctl,
			dpaux_ctl->regs + PADCTLREG_I2C_DPAUX);
}

static int tegra234_dpaux_suspend(struct device *dev)
{
	struct tegra_dpaux_pinctl *dpaux_ctl = dev_get_drvdata(dev);

	tegra234_dpaux_save(dpaux_ctl);

	return 0;
}

static int tegra234_dpaux_resume(struct device *dev)
{
	struct tegra_dpaux_pinctl *dpaux_ctl = dev_get_drvdata(dev);

	tegra234_dpaux_restore(dpaux_ctl);

	return 0;
}

static const struct dev_pm_ops tegra234_dpaux_pm_ops = {
	.suspend = tegra234_dpaux_suspend,
	.resume = tegra234_dpaux_resume,
};

static const struct of_device_id tegra_dpaux_pinctl_of_match[] = {
	{.compatible = "nvidia,tegra194-misc-dpaux-padctl",
		.data = &tegra234_dpaux_chip_data[0]},
	{ }
};
MODULE_DEVICE_TABLE(of, tegra_dpaux_pinctl_of_match);

static struct platform_driver tegra234_dpaux_pinctrl = {
	.driver = {
		.name = "tegra234-dpaux-pinctrl",
		.of_match_table = tegra_dpaux_pinctl_of_match,
		.pm = &tegra234_dpaux_pm_ops,
	},
	.probe = tegra234_dpaux_pinctrl_probe,
	.remove = tegra_dpaux_remove,
};


module_platform_driver(tegra234_dpaux_pinctrl);

MODULE_DESCRIPTION("NVIDIA Tegra dpaux pinctrl driver");
MODULE_AUTHOR("Prathamesh Shete <pshete@nvidia.com>");
MODULE_ALIAS("platform:tegra234-dpaux");
MODULE_LICENSE("GPL v2");
