/*
 * VI5 driver
 *
 * Copyright (c) 2017-2022, NVIDIA Corporation.  All rights reserved.
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

#include <asm/ioctls.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <media/fusa-capture/capture-vi-channel.h>
#include <soc/tegra/camrtc-capture.h>
#include <linux/version.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif

#include "vi5.h"
#include "dev.h"
#include "bus_client.h"
#include "nvhost_acm.h"
#include "capture/capture-support.h"

#include "t194/t194.h"
#if IS_ENABLED(CONFIG_TEGRA_T23X_GRHOST)
#include "t23x/t23x.h"
#endif
#include <media/vi.h>
#include <media/mc_common.h>
#include <media/tegra_camera_platform.h>
#include <uapi/linux/nvhost_vi_ioctl.h>
#include <linux/platform/tegra/latency_allowance.h>

/* HW capability, pixels per clock */
#define NUM_PPC		8
/* 15% bus protocol overhead */
/* + 5% SW overhead */
#define VI_OVERHEAD	20

struct host_vi5 {
	struct platform_device *pdev;
	struct platform_device *vi_thi;
	struct vi vi_common;

	/* Debugfs */
	struct vi5_debug {
		struct debugfs_regset32 ch0;
	} debug;

	/* WAR: Adding a temp flags to avoid registering to V4L2 and
	 * tegra camera platform device.
	 */
	bool skip_v4l2_init;
};

static int vi5_init_debugfs(struct host_vi5 *vi5);
static void vi5_remove_debugfs(struct host_vi5 *vi5);

static int vi5_alloc_syncpt(struct platform_device *pdev,
			const char *name,
			uint32_t *syncpt_id)
{
	struct host_vi5 *vi5 = nvhost_get_private_data(pdev);

	return capture_alloc_syncpt(vi5->vi_thi, name, syncpt_id);
}

int nvhost_vi5_aggregate_constraints(struct platform_device *dev,
				int clk_index,
				unsigned long floor_rate,
				unsigned long pixelrate,
				unsigned long bw_constraint)
{
	struct nvhost_device_data *pdata = nvhost_get_devdata(dev);

	if (!pdata) {
		dev_err(&dev->dev,
			"No platform data, fall back to default policy\n");
		return 0;
	}

	if (clk_index != 0)
		return 0;
	/*
	 * SCF and V4l2 send request using NVHOST_CLK through tegra_camera_platform,
	 * which is calculated in floor_rate.
	 */
	return floor_rate + (pixelrate / pdata->num_ppc);
}

static void vi5_release_syncpt(struct platform_device *pdev, uint32_t id)
{
	struct host_vi5 *vi5 = nvhost_get_private_data(pdev);

	capture_release_syncpt(vi5->vi_thi, id);
}

static void vi5_get_gos_table(struct platform_device *pdev, int *count,
			const dma_addr_t **table)
{
	struct host_vi5 *vi5 = nvhost_get_private_data(pdev);

	capture_get_gos_table(vi5->vi_thi, count, table);
}

static int vi5_get_syncpt_gos_backing(struct platform_device *pdev,
			uint32_t id,
			dma_addr_t *syncpt_addr,
			uint32_t *gos_index,
			uint32_t *gos_offset)
{
	struct host_vi5 *vi5 = nvhost_get_private_data(pdev);

	return capture_get_syncpt_gos_backing(vi5->vi_thi, id,
				syncpt_addr, gos_index, gos_offset);
}

static struct vi_channel_drv_ops vi5_channel_drv_ops = {
	.alloc_syncpt = vi5_alloc_syncpt,
	.release_syncpt = vi5_release_syncpt,
	.get_gos_table = vi5_get_gos_table,
	.get_syncpt_gos_backing = vi5_get_syncpt_gos_backing,
};

int vi5_priv_early_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct nvhost_device_data *info;
	struct device_node *thi_np;
	struct platform_device *thi = NULL;
	struct host_vi5 *vi5;
	int err = 0;

	info = (void *)of_device_get_match_data(dev);
	if (unlikely(info == NULL)) {
		dev_WARN(dev, "no platform data\n");
		return -ENODATA;
	}

	thi_np = of_parse_phandle(dev->of_node, "nvidia,vi-falcon-device", 0);
	if (thi_np == NULL) {
		dev_WARN(dev, "missing %s handle\n", "nvidia,vi-falcon-device");
		return -ENODEV;
	}

	thi = of_find_device_by_node(thi_np);
	of_node_put(thi_np);

	if (thi == NULL)
		return -ENODEV;

	if (thi->dev.driver == NULL) {
		err = -EPROBE_DEFER;
		goto put_vi;
	}

	err = vi_channel_drv_fops_register(&vi5_channel_drv_ops);
	if (err) {
		dev_warn(&pdev->dev, "syncpt fops register failed, defer probe\n");
		goto put_vi;
	}

	vi5 = (struct host_vi5*) devm_kzalloc(dev, sizeof(*vi5), GFP_KERNEL);
	if (!vi5) {
		err = -ENOMEM;
		goto put_vi;
	}

	vi5->skip_v4l2_init = of_property_read_bool(dev->of_node,
					"nvidia,skip-v4l2-init");
	vi5->vi_thi = thi;
	vi5->pdev = pdev;
	info->pdev = pdev;
	mutex_init(&info->lock);
	platform_set_drvdata(pdev, info);
	info->private_data = vi5;

	(void) dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(39));

	return 0;

put_vi:
	platform_device_put(thi);
	if (err != -EPROBE_DEFER)
		dev_err(&pdev->dev, "probe failed: %d\n", err);

	info->private_data = NULL;

	return err;
}

int vi5_priv_late_probe(struct platform_device *pdev)
{
	struct tegra_camera_dev_info vi_info;
	struct nvhost_device_data *info = platform_get_drvdata(pdev);
	struct host_vi5 *vi5 = info->private_data;
	int err;

	memset(&vi_info, 0, sizeof(vi_info));
	vi_info.pdev = pdev;
	vi_info.hw_type = HWTYPE_VI;
	vi_info.ppc = NUM_PPC;
	vi_info.overhead = VI_OVERHEAD;

	err = tegra_camera_device_register(&vi_info, vi5);
	if (err)
		goto device_release;

	vi5_init_debugfs(vi5);

	return 0;

device_release:
	nvhost_client_device_release(pdev);

	return err;
}

static int vi5_probe(struct platform_device *pdev)
{
	int err;
	struct nvhost_device_data *pdata;
	struct host_vi5 *vi5;

	dev_dbg(&pdev->dev, "%s: probe %s\n", __func__, pdev->name);

	err = vi5_priv_early_probe(pdev);
	if (err)
		goto error;

	pdata = platform_get_drvdata(pdev);
	vi5 = pdata->private_data;

	err = nvhost_client_device_get_resources(pdev);
	if (err)
		goto put_vi;

	err = nvhost_module_init(pdev);
	if (err)
		goto put_vi;

	err = nvhost_client_device_init(pdev);
	if (err)
		goto deinit;

	err = vi5_priv_late_probe(pdev);
	if (err)
		goto deinit;

	return 0;

deinit:
	nvhost_module_deinit(pdev);
put_vi:
	platform_device_put(vi5->vi_thi);
	if (err != -EPROBE_DEFER)
		dev_err(&pdev->dev, "probe failed: %d\n", err);
error:
	return err;
}

struct t194_vi5_file_private {
	struct platform_device *pdev;
	struct tegra_mc_vi mc_vi;
	unsigned int vi_bypass_bw;
};

static int vi5_remove(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct host_vi5 *vi5 = pdata->private_data;

	tegra_camera_device_unregister(vi5);
	vi_channel_drv_unregister(&pdev->dev);
	tegra_vi_media_controller_cleanup(&vi5->vi_common.mc_vi);

	vi5_remove_debugfs(vi5);
	platform_device_put(vi5->vi_thi);

	return 0;
}

static const struct of_device_id tegra_vi5_of_match[] = {
	{
		.name = "vi",
		.compatible = "nvidia,tegra194-vi",
		.data = &t19_vi5_info,
	},
#if IS_ENABLED(CONFIG_TEGRA_T23X_GRHOST)
#include "vi/vi5-t23x.h"
#endif
	{ },
};

static struct platform_driver vi5_driver = {
	.probe = vi5_probe,
	.remove = vi5_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "tegra194-vi5",
#ifdef CONFIG_OF
		.of_match_table = tegra_vi5_of_match,
#endif
#ifdef CONFIG_PM
		.pm = &nvhost_module_pm_ops,
#endif
	},
};

module_platform_driver(vi5_driver);

/* === Debugfs ========================================================== */

static int vi5_init_debugfs(struct host_vi5 *vi5)
{
	static const struct debugfs_reg32 vi5_ch_regs[] = {
		{ .name = "protocol_version", 0x00 },
		{ .name = "perforce_changelist", 0x4 },
		{ .name = "build_timestamp", 0x8 },
		{ .name = "channel_count", 0x80 },
	};
	struct nvhost_device_data *pdata = platform_get_drvdata(vi5->pdev);
	struct dentry *dir = pdata->debugfs;
	struct vi5_debug *debug = &vi5->debug;

	debug->ch0.base = pdata->aperture[0];
	debug->ch0.regs = vi5_ch_regs;
	debug->ch0.nregs = ARRAY_SIZE(vi5_ch_regs);
	debugfs_create_regset32("ch0", S_IRUGO, dir, &debug->ch0);

	return 0;
}

static void vi5_remove_debugfs(struct host_vi5 *vi5)
{
}
