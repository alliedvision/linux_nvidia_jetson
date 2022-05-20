/*
 * NVCSI driver for T194
 *
 * Copyright (c) 2017-2021, NVIDIA Corporation.  All rights reserved.
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

#include "nvcsi-t194.h"
#include <uapi/linux/nvhost_nvcsi_ioctl.h>
#include <linux/tegra-camera-rtcpu.h>

#include <asm/ioctls.h>
#include <linux/device.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>

#include <media/mc_common.h>
#include <media/tegra_camera_platform.h>
#include "camera/nvcsi/csi5_fops.h"

#include "dev.h"
#include "bus_client.h"
#include "nvhost_acm.h"
#include "t194/t194.h"
#include "media/csi.h"

/* PG rate based on max ISP throughput */
#define PG_CLK_RATE	102000000
/* width of interface between VI and CSI */
#define CSI_BUS_WIDTH	64
/* number of lanes per brick */
#define NUM_LANES	4

#define PHY_OFFSET			0x10000U
#define CIL_A_SW_RESET			0x11024U
#define CIL_B_SW_RESET			0x110b0U
#define CSIA				(1 << 20)
#define CSIH				(1 << 27)

static struct tegra_csi_device *mc_csi;
struct t194_nvcsi {
	struct platform_device *pdev;
	struct tegra_csi_device csi;
	struct dentry *dir;
};

static const struct of_device_id tegra194_nvcsi_of_match[] = {
	{
		.compatible = "nvidia,tegra194-nvcsi",
		.data = &t19_nvcsi_info,
	},
	{ },
};

struct t194_nvcsi_file_private {
	struct platform_device *pdev;
};

static long t194_nvcsi_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	return -ENOIOCTLCMD;
}

static int t194_nvcsi_open(struct inode *inode, struct file *file)
{
	struct nvhost_device_data *pdata = container_of(inode->i_cdev,
					struct nvhost_device_data, ctrl_cdev);
	struct platform_device *pdev = pdata->pdev;
	struct t194_nvcsi_file_private *filepriv;

	filepriv = kzalloc(sizeof(*filepriv), GFP_KERNEL);
	if (unlikely(filepriv == NULL))
		return -ENOMEM;

	filepriv->pdev = pdev;

	file->private_data = filepriv;

	return nonseekable_open(inode, file);
}

static int t194_nvcsi_release(struct inode *inode, struct file *file)
{
	struct t194_nvcsi_file_private *filepriv = file->private_data;

	kfree(filepriv);

	return 0;
}

const struct file_operations tegra194_nvcsi_ctrl_ops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.unlocked_ioctl = t194_nvcsi_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = t194_nvcsi_ioctl,
#endif
	.open = t194_nvcsi_open,
	.release = t194_nvcsi_release,
};

int t194_nvcsi_early_probe(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata;
	struct t194_nvcsi *nvcsi;

	pdata = (void *)of_device_get_match_data(&pdev->dev);
	if (unlikely(pdata == NULL)) {
		dev_WARN(&pdev->dev, "no platform data\n");
		return -ENODATA;
	}

	nvcsi = devm_kzalloc(&pdev->dev, sizeof(*nvcsi), GFP_KERNEL);
	if (!nvcsi) {
		return -ENOMEM;
	}

	pdata->pdev = pdev;
	nvcsi->pdev = pdev;
	mutex_init(&pdata->lock);
	platform_set_drvdata(pdev, pdata);
	mc_csi = &nvcsi->csi;

	pdata->private_data = nvcsi;

	return 0;
}

int t194_nvcsi_late_probe(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct t194_nvcsi *nvcsi = pdata->private_data;
	struct tegra_camera_dev_info csi_info;
	int err;

	memset(&csi_info, 0, sizeof(csi_info));
	csi_info.pdev = pdev;
	csi_info.hw_type = HWTYPE_CSI;
	csi_info.use_max = true;
	csi_info.bus_width = CSI_BUS_WIDTH;
	csi_info.lane_num = NUM_LANES;
	csi_info.pg_clk_rate = PG_CLK_RATE;
	err = tegra_camera_device_register(&csi_info, nvcsi);
	if (err)
		return err;

	nvcsi->pdev = pdev;
	nvcsi->csi.fops = &csi5_fops;
	err = tegra_csi_media_controller_init(&nvcsi->csi, pdev);

	return 0;
}

static int t194_nvcsi_probe(struct platform_device *pdev)
{
	int err;
	struct nvhost_device_data *pdata;
	struct t194_nvcsi *nvcsi;

	err = t194_nvcsi_early_probe(pdev);
	if (err)
		return err;

	pdata = platform_get_drvdata(pdev);

	nvcsi = pdata->private_data;

	err = nvhost_client_device_get_resources(pdev);
	if (err)
		return err;

	err = nvhost_module_init(pdev);
	if (err)
		return err;

	err = nvhost_client_device_init(pdev);
	if (err) {
		nvhost_module_deinit(pdev);
		goto err_client_device_init;
	}

	err = t194_nvcsi_late_probe(pdev);
	if (err)
		goto err_mediacontroller_init;

	return 0;

err_mediacontroller_init:
	nvhost_client_device_release(pdev);
err_client_device_init:
	pdata->private_data = NULL;
	return err;
}

static int __exit t194_nvcsi_remove(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	struct t194_nvcsi *nvcsi = pdata->private_data;

	tegra_camera_device_unregister(nvcsi);
	mc_csi = NULL;
	tegra_csi_media_controller_remove(&nvcsi->csi);

	return 0;
}

static struct platform_driver t194_nvcsi_driver = {
	.probe = t194_nvcsi_probe,
	.remove = __exit_p(t194_nvcsi_remove),
	.driver = {
		.owner = THIS_MODULE,
		.name = "t194-nvcsi",
#ifdef CONFIG_OF
		.of_match_table = tegra194_nvcsi_of_match,
#endif
#ifdef CONFIG_PM
		.pm = &nvhost_module_pm_ops,
#endif
	},
};

module_platform_driver(t194_nvcsi_driver);
