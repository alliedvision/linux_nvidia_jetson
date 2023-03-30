/*
 * SPDX-FileCopyrightText: Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <dt-bindings/thermal/tegra234-soctherm.h>
#include <linux/err.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <soc/tegra/bpmp-abi.h>
#include <soc/tegra/bpmp.h>

struct oc_soc_data {
	const struct attribute_group **attr_groups;
};

struct tegra23x_oc_event {
	struct device *hwmon;
	struct tegra_bpmp *bpmp;
	struct oc_soc_data soc_data;
};

static ssize_t throt_en_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	int err = 0;
	struct tegra23x_oc_event *tegra23x_oc = dev_get_drvdata(dev);
	struct sensor_device_attribute *sensor_attr =
		container_of(attr, struct sensor_device_attribute, dev_attr);
	struct mrq_oc_status_response resp;
	struct tegra_bpmp_message msg = {
		.mrq = MRQ_OC_STATUS,
		.rx = {
			.data = &resp,
			.size = sizeof(resp),
		},
	};

	if (sensor_attr->index < 0) {
		dev_err(dev, "Negative index for OC events\n");
		return -EDOM;
	}

	err = tegra_bpmp_transfer(tegra23x_oc->bpmp, &msg);
	if (err) {
		dev_err(dev, "Failed to transfer message: %d\n", err);
		return err;
	}

	if (msg.rx.ret < 0) {
		dev_err(dev, "Negative bpmp message return value: %d\n",
			msg.rx.ret);
		return -EINVAL;
	}

	return sprintf(buf, "%u\n", resp.throt_en[sensor_attr->index]);
}

static ssize_t event_cnt_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	int err = 0;
	struct tegra23x_oc_event *tegra23x_oc = dev_get_drvdata(dev);
	struct sensor_device_attribute *sensor_attr =
		container_of(attr, struct sensor_device_attribute, dev_attr);
	struct mrq_oc_status_response resp;
	struct tegra_bpmp_message msg = {
		.mrq = MRQ_OC_STATUS,
		.rx = {
			.data = &resp,
			.size = sizeof(resp),
		},
	};

	if (sensor_attr->index < 0) {
		dev_err(dev, "Negative index for OC events\n");
		return -EDOM;
	}

	err = tegra_bpmp_transfer(tegra23x_oc->bpmp, &msg);
	if (err) {
		dev_err(dev, "Failed to transfer message: %d\n", err);
		return err;
	}

	if (msg.rx.ret < 0) {
		dev_err(dev, "Negative bpmp message return value: %d\n",
			msg.rx.ret);
		return -EINVAL;
	}

	return sprintf(buf, "%u\n", resp.event_cnt[sensor_attr->index]);
}

static SENSOR_DEVICE_ATTR_RO(oc1_throt_en, throt_en, TEGRA234_SOCTHERM_EDP_OC1);
static SENSOR_DEVICE_ATTR_RO(oc1_event_cnt, event_cnt,
			     TEGRA234_SOCTHERM_EDP_OC1);
static SENSOR_DEVICE_ATTR_RO(oc2_throt_en, throt_en, TEGRA234_SOCTHERM_EDP_OC2);
static SENSOR_DEVICE_ATTR_RO(oc2_event_cnt, event_cnt,
			     TEGRA234_SOCTHERM_EDP_OC2);
static SENSOR_DEVICE_ATTR_RO(oc3_throt_en, throt_en, TEGRA234_SOCTHERM_EDP_OC3);
static SENSOR_DEVICE_ATTR_RO(oc3_event_cnt, event_cnt,
			     TEGRA234_SOCTHERM_EDP_OC3);

static struct attribute *t234_oc1_attrs[] = {
	&sensor_dev_attr_oc1_throt_en.dev_attr.attr,
	&sensor_dev_attr_oc1_event_cnt.dev_attr.attr,
	NULL,
};

static struct attribute *t234_oc2_attrs[] = {
	&sensor_dev_attr_oc2_throt_en.dev_attr.attr,
	&sensor_dev_attr_oc2_event_cnt.dev_attr.attr,
	NULL,
};

static struct attribute *t234_oc3_attrs[] = {
	&sensor_dev_attr_oc3_throt_en.dev_attr.attr,
	&sensor_dev_attr_oc3_event_cnt.dev_attr.attr,
	NULL,
};

static const struct attribute_group oc1_data = {
	.attrs = t234_oc1_attrs,
	NULL,
};

static const struct attribute_group oc2_data = {
	.attrs = t234_oc2_attrs,
	NULL,
};

static const struct attribute_group oc3_data = {
	.attrs = t234_oc3_attrs,
	NULL,
};

static const struct attribute_group *t234_oc_groups[] = {
	&oc1_data,
	&oc2_data,
	&oc3_data,
	NULL,
};

static const struct oc_soc_data t234_oc_soc_data = {
	.attr_groups = t234_oc_groups,
};

static const struct of_device_id of_tegra23x_oc_event_match[] = {
	{ .compatible = "nvidia,tegra234-oc-event",
	  .data = (void *)&t234_oc_soc_data },
	{},
};
MODULE_DEVICE_TABLE(of, of_tegra23x_oc_event_match);

static int tegra23x_oc_event_probe(struct platform_device *pdev)
{
	int err = 0;
	const struct of_device_id *match;
	struct tegra_bpmp *tb;
	struct tegra23x_oc_event *tegra23x_oc;

	match = of_match_node(of_tegra23x_oc_event_match, pdev->dev.of_node);
	if (!match)
		return -ENODEV;

	tb = tegra_bpmp_get(&pdev->dev);
	if (IS_ERR(tb))
		return PTR_ERR(tb);

	tegra23x_oc =
		devm_kzalloc(&pdev->dev, sizeof(*tegra23x_oc), GFP_KERNEL);
	if (!tegra23x_oc) {
		err = -ENOMEM;
		goto put_bpmp;
	}

	platform_set_drvdata(pdev, tegra23x_oc);
	tegra23x_oc->bpmp = tb;
	memcpy(&tegra23x_oc->soc_data, match->data, sizeof(struct oc_soc_data));
	tegra23x_oc->hwmon = devm_hwmon_device_register_with_groups(
		&pdev->dev, "soctherm_oc", tegra23x_oc,
		tegra23x_oc->soc_data.attr_groups);
	if (IS_ERR(tegra23x_oc->hwmon)) {
		dev_err(&pdev->dev, "Failed to register hwmon device\n");
		err = -EINVAL;
		goto free_mem;
	}

	dev_info(&pdev->dev, "Finished tegra23x overcurrent event probing\n");
	return err;

free_mem:
	devm_kfree(&pdev->dev, (void *)tegra23x_oc);
put_bpmp:
	tegra_bpmp_put(tb);

	return err;
}

static int tegra23x_oc_event_remove(struct platform_device *pdev)
{
	struct tegra23x_oc_event *tegra23x_oc = platform_get_drvdata(pdev);

	if (!tegra23x_oc)
		return -EINVAL;

	devm_hwmon_device_unregister(tegra23x_oc->hwmon);
	tegra_bpmp_put(tegra23x_oc->bpmp);
	devm_kfree(&pdev->dev, (void *)tegra23x_oc);
	return 0;
}

static struct platform_driver tegra23x_oc_event_driver = {
	.probe = tegra23x_oc_event_probe,
	.remove = tegra23x_oc_event_remove,
	.driver = {
		.name = "tegra23x-oc-event",
		.of_match_table = of_tegra23x_oc_event_match,
	},
};

module_platform_driver(tegra23x_oc_event_driver);

MODULE_AUTHOR("Yi-Wei Wang <yiweiw@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra23x Overcurrent Event Driver");
MODULE_LICENSE("GPL v2");
