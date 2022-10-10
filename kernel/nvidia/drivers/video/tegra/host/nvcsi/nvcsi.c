/*
 * NVCSI driver
 *
 * Copyright (c) 2014-2022, NVIDIA Corporation.  All rights reserved.
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
#include <linux/export.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/fs.h>
#include <asm/ioctls.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <uapi/linux/nvhost_nvcsi_ioctl.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include <media/mc_common.h>
#include <media/csi.h>
#include <media/tegra_camera_platform.h>

#include "dev.h"
#include "bus_client.h"
#include "nvhost_acm.h"
#include "t194/t194.h"
#include "nvcsi.h"
#include "camera/csi/csi4_fops.h"

#include "deskew.h"

#define PG_CLK_RATE	102000000
/* width of interface between VI and CSI */
#define CSI_BUS_WIDTH	64
/* number of lanes per brick */
#define NUM_LANES	4

#define CSIA			(1 << 20)
#define CSIF			(1 << 25)

struct nvcsi {
	struct platform_device *pdev;
	struct regulator *regulator;
	struct tegra_csi_device csi;
	struct dentry *dir;
};

static struct tegra_csi_device *mc_csi;

struct nvcsi_private {
	struct platform_device *pdev;
	struct nvcsi_deskew_context deskew_ctx;
};

int nvcsi_cil_sw_reset(int lanes, int enable)
{
	unsigned int phy_num = 0U;
	unsigned int val = enable ? (SW_RESET1_EN | SW_RESET0_EN) : 0U;
	unsigned int addr, i;

	for (i = CSIA; i < CSIF; i = i << 2U) {
		if (lanes & i) {
			addr = CSI4_BASE_ADDRESS + NVCSI_CIL_A_SW_RESET +
						(CSI4_PHY_OFFSET * phy_num);
			host1x_writel(mc_csi->pdev, addr, val);
		}
		if (lanes & (i << 1U)) {
			addr = CSI4_BASE_ADDRESS + NVCSI_CIL_B_SW_RESET +
						(CSI4_PHY_OFFSET * phy_num);
			host1x_writel(mc_csi->pdev, addr, val);
		}
		phy_num++;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(nvcsi_cil_sw_reset);

static long nvcsi_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	struct nvcsi_private *priv = file->private_data;
	int ret;

	switch (cmd) {
	// sensor must be turned on before calling this ioctl, and streaming
	// should be started shortly after.
	case NVHOST_NVCSI_IOCTL_DESKEW_SETUP: {
		unsigned int active_lanes;

		dev_dbg(mc_csi->dev, "ioctl: deskew_setup\n");
		priv->deskew_ctx.deskew_lanes = get_user(active_lanes,
				(long __user *)arg);
		ret = nvcsi_deskew_setup(&priv->deskew_ctx);
		return ret;
		}
	case NVHOST_NVCSI_IOCTL_DESKEW_APPLY: {
		dev_dbg(mc_csi->dev, "ioctl: deskew_apply\n");
		ret = nvcsi_deskew_apply_check(&priv->deskew_ctx);
		return ret;
		}
	}
	return -ENOIOCTLCMD;
}

static int nvcsi_open(struct inode *inode, struct file *file)
{
	struct nvhost_device_data *pdata = container_of(inode->i_cdev,
					struct nvhost_device_data, ctrl_cdev);
	struct platform_device *pdev = pdata->pdev;
	struct nvcsi_private *priv;

	priv = kmalloc(sizeof(*priv), GFP_KERNEL);
	if (unlikely(priv == NULL))
		return -ENOMEM;

	priv->pdev = pdev;

	file->private_data = priv;
	return nonseekable_open(inode, file);
}

static int nvcsi_release(struct inode *inode, struct file *file)
{
	struct nvcsi_private *priv = file->private_data;

	kfree(priv);
	return 0;
}

const struct file_operations tegra_nvcsi_ctrl_ops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.unlocked_ioctl = nvcsi_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = nvcsi_ioctl,
#endif
	.open = nvcsi_open,
	.release = nvcsi_release,
};
