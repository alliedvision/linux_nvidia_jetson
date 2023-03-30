/*
 * Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/tegra-ivc.h>
#include <linux/tegra-ivc-bus.h>
#include <linux/tegra-ivc-instance.h>

static int tegra_camera_diagnostics_probe(struct tegra_ivc_channel *ch)
{
	(void)ch;
	return 0;
}

static void tegra_camera_diagnostics_remove(struct tegra_ivc_channel *ch)
{
	(void)ch;
}

static const struct tegra_ivc_channel_ops
tegra_camera_diagnostics_channel_ops = {
	.probe	= tegra_camera_diagnostics_probe,
	.remove	= tegra_camera_diagnostics_remove,
};

static const struct of_device_id camera_diagnostics_of_match[] = {
	{ .compatible = "nvidia,tegra186-camera-diagnostics", },
	{ },
};

static struct tegra_ivc_driver camera_diagnostics_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.bus	= &tegra_ivc_bus_type,
		.name	= "tegra-camera-diagnostics",
		.of_match_table = camera_diagnostics_of_match,
	},
	.dev_type	= &tegra_ivc_channel_type,
	.ops.channel	= &tegra_camera_diagnostics_channel_ops,
};

tegra_ivc_subsys_driver_default(camera_diagnostics_driver);
MODULE_AUTHOR("Pekka Pessi <ppessi@nvidia.com>");
MODULE_DESCRIPTION("Dummy device driver for Camera Diagnostics IVC Channel");
MODULE_LICENSE("GPL v2");
