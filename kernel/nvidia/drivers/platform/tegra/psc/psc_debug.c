/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/of_platform.h>
#include <linux/mailbox_client.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/types.h>
#include <linux/completion.h>
#include "tegra23x_psc.h"

/* EXT_CFG register offset */
#define EXT_CFG_SIDTABLE 0x0
#define EXT_CFG_SIDCONFIG 0x4

#define MBOX_MSG_LEN 64

#define RX_READY 1
#define RX_IDLE 0

/* Max block period in ms before TX is assumed failure. */
#define DEFAULT_TX_TIMEOUT 2000

/* 256MB max size to use for dma_alloc* */
#define MAX_SHARED_MEM (256 * 1024 * 1024U)

struct xfer_info {
	__u32 opcode[2];
	void __user *tx_buf;
	void __user *rx_buf;

	__u32 tx_size;
	__u32 rx_size;

	__u8  out[MBOX_MSG_LEN];
};

union mbox_msg {
	struct {
		u32 opcode[2];
		u32 tx_size;
		u32 rx_size;
		u64 tx_iova;
		u64 rx_iova;
	};
	u32 data[16];
};

#define PSCDBG_IOCTL_BASE 'P'
#define PSCIOC_XFER_DATA _IOWR(PSCDBG_IOCTL_BASE, 0, struct xfer_info)

struct psc_debug_dev {
	struct mutex lock;
	struct platform_device *pdev;
	struct mbox_client cl;
	struct mbox_chan *chan;
	struct completion rx_complete;

	u8 rx_msg[MBOX_MSG_LEN];
};

static struct psc_debug_dev psc_debug;
static struct dentry *debugfs_root;

static int psc_debug_open(struct inode *inode, struct file *file)
{
	struct mbox_chan *chan;
	struct psc_debug_dev *dbg = inode->i_private;
	struct platform_device *pdev = dbg->pdev;
	int ret = 0;

	if (mutex_lock_interruptible(&dbg->lock))
		return -ERESTARTSYS;

	file->private_data = dbg;

	chan = mbox_request_channel(&dbg->cl, 0);
	if (IS_ERR(chan) && (PTR_ERR(chan) != -EPROBE_DEFER)) {
		dev_err(&pdev->dev, "failed to get channel, err %lx\n",
			PTR_ERR(chan));
		ret = PTR_ERR(chan);
		goto return_unlock;
	}

	dbg->chan = chan;
	init_completion(&dbg->rx_complete);
	nonseekable_open(inode, file);

return_unlock:
	mutex_unlock(&dbg->lock);
	return ret;
}

static int psc_debug_release(struct inode *inode, struct file *file)
{
	struct psc_debug_dev *dbg = file->private_data;

	mutex_lock(&dbg->lock);

	mbox_free_channel(dbg->chan);

	file->private_data = NULL;

	mutex_unlock(&dbg->lock);
	return 0;
}

static ssize_t psc_debug_read(struct file *file, char __user *buffer,
				size_t count, loff_t *ppos)
{
	struct psc_debug_dev *dbg = file->private_data;
	ssize_t ret;
	loff_t pos = 0;

	if (count > MBOX_MSG_LEN)
		return -EINTR;

	mutex_lock(&dbg->lock);

	ret = simple_read_from_buffer(buffer, count, &pos,
			dbg->rx_msg, min_t(size_t, count, MBOX_MSG_LEN));
	*ppos += pos;

	mutex_unlock(&dbg->lock);
	return ret;
}

static int send_msg_block(struct psc_debug_dev *dbg, void *tx)
{
	int ret;

	reinit_completion(&dbg->rx_complete);

	ret = mbox_send_message(dbg->chan, tx);
	if (ret < 0)
		return ret;

	mbox_client_txdone(dbg->chan, 0);
	ret = wait_for_completion_timeout(&dbg->rx_complete,
			msecs_to_jiffies(dbg->cl.tx_tout));
	return ret == 0 ? -ETIME : 0;

}

static ssize_t psc_debug_write(struct file *file, const char __user *buffer,
				size_t count, loff_t *ppos)
{
	struct psc_debug_dev *dbg = file->private_data;
	struct platform_device *pdev = dbg->pdev;
	u8 tx_buf[MBOX_MSG_LEN] = { 0 };
	ssize_t ret;

	if (count > MBOX_MSG_LEN) {
		dev_err(&pdev->dev, "write size > MBOX_MSG_LEN\n");
		return -EINVAL;
	}

	mutex_lock(&dbg->lock);
	if (copy_from_user(tx_buf, buffer, count)) {
		dev_err(&pdev->dev, "copy_from_user() error!\n");
		ret = -EFAULT;
		goto return_unlock;
	}
	ret = send_msg_block(dbg, tx_buf);

return_unlock:
	mutex_unlock(&dbg->lock);
	return ret < 0 ? ret : count;
}

static long xfer_data(struct file *file, char __user *data)
{
	struct psc_debug_dev *dbg = file->private_data;
	struct platform_device *pdev = dbg->pdev;
	struct device *dev = &pdev->dev;
	void *tx_virt = NULL;
	void *rx_virt = NULL;
	dma_addr_t tx_phys = 0;
	dma_addr_t rx_phys = 0;
	long ret = 0;
	union mbox_msg msg = {};
	struct xfer_info info;
	struct xfer_info __user *ptr_xfer = (struct xfer_info __user *)data;

	if (copy_from_user(&info, data, sizeof(struct xfer_info))) {
		dev_err(&pdev->dev, "failed to copy data.\n");
		ret = -EFAULT;
	}

	dev_dbg(dev, "opcode[%x %x]\n", info.opcode[0], info.opcode[1]);
	dev_dbg(dev, "tx[%p, size:%u], rx[%p, size:%u]\n",
		info.tx_buf, info.tx_size, info.rx_buf, info.rx_size);

	if (info.tx_size > MAX_SHARED_MEM || info.rx_size > MAX_SHARED_MEM)
		return -ENOMEM;

	if (info.tx_buf && info.tx_size > 0) {

		tx_virt = dma_alloc_coherent(dev, info.tx_size,
					&tx_phys, GFP_KERNEL);
		if (tx_virt == NULL || tx_phys == 0) {
			dev_err(dev, "dma_alloc_coherent() failed!\n");
			ret = -ENOMEM;
			goto fail;
		}

		if (copy_from_user(tx_virt, info.tx_buf, info.tx_size)) {
			dev_err(dev, "failed to copy data.\n");
			ret = -EFAULT;
			goto free_rx;
		}
		dma_sync_single_for_device(dev, tx_phys,
				info.tx_size, DMA_TO_DEVICE);
	}

	if (info.rx_buf && info.rx_size > 0) {

		rx_virt = dma_alloc_coherent(dev, info.rx_size,
				&rx_phys, GFP_KERNEL);
		if (rx_virt == NULL || rx_phys == 0) {
			dev_err(dev, "dma_alloc_coherent() failed!\n");
			ret = -ENOMEM;
			goto free_tx;
		}
	}
	dev_dbg(dev, "tx_virt:%p, tx_phys: %p\n", tx_virt, (void *)tx_phys);
	dev_dbg(dev, "rx_virt:%p, rx_phys: %p\n", rx_virt, (void *)rx_phys);

	msg.opcode[0] = info.opcode[0];
	msg.opcode[1] = info.opcode[1];
	msg.tx_iova = tx_phys;
	msg.rx_iova = rx_phys;
	msg.tx_size = info.tx_size;
	msg.rx_size = info.rx_size;

	if (send_msg_block(dbg, &msg) != 0)
		goto free_rx;

	/* copy mbox payload */
	if (copy_to_user(&ptr_xfer->out[0],
			&dbg->rx_msg[0], sizeof(dbg->rx_msg))) {
		dev_err(dev, "failed to mbox out data.\n");
		ret = -EFAULT;
		goto free_rx;
	}

	if (rx_phys && info.rx_size > 0) {
		dma_sync_single_for_cpu(dev, rx_phys,
				info.rx_size, DMA_BIDIRECTIONAL);

		if (copy_to_user(info.rx_buf, rx_virt, msg.rx_size)) {
			dev_err(dev, "failed to copy_to_user.\n");
			ret = -EFAULT;
			goto free_rx;
		}
	}

free_rx:
	if (rx_phys)
		dma_free_coherent(dev, info.rx_size, rx_virt, rx_phys);
free_tx:
	if (tx_phys)
		dma_free_coherent(dev, info.tx_size, tx_virt, tx_phys);
fail:
	return ret;
}

static long
psc_debug_ioctl(struct file *file, unsigned int cmd, unsigned long data)
{
	struct psc_debug_dev *dbg = file->private_data;
	long ret = -ENOIOCTLCMD;

	mutex_lock(&dbg->lock);

	switch (cmd) {
	case PSCIOC_XFER_DATA:
		ret = xfer_data(file, (char __user *)data);
		break;
	default:
		break;
	}

	mutex_unlock(&dbg->lock);
	return ret;
}

static const struct file_operations psc_debug_fops = {
	.open		= psc_debug_open,
	.read		= psc_debug_read,
	.write		= psc_debug_write,
	.release	= psc_debug_release,
	.unlocked_ioctl = psc_debug_ioctl,
};

static void psc_chan_rx_callback(struct mbox_client *c, void *msg)
{
	struct device *dev = c->dev;
	struct psc_debug_dev *dbg = container_of(c, struct psc_debug_dev, cl);

	dev_dbg(dev, "%s\n", __func__);

	memcpy(dbg->rx_msg, msg, MBOX_MSG_LEN);
	complete(&dbg->rx_complete);
}

#define NV(x) "nvidia," #x

static int
setup_extcfg(struct platform_device *pdev, struct psc_debug_dev *dbg,
	struct dentry *root)
{
	struct device_node *np = pdev->dev.of_node;
	struct resource *res;
	void __iomem *base;
	u32 value;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "extcfg");
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base))
		return -EINVAL;

	dev_info(&pdev->dev, "ext_cfg base:%p\n", base);

	if (!of_property_read_u8_array(np, NV(sidtable),
				(u8 *)&value, sizeof(value))) {
		dev_dbg(&pdev->dev, "sidtable:%08x\n", value);
		writel(value, base + EXT_CFG_SIDTABLE);
	}

	if (!of_property_read_u32(np, NV(sidconfig), &value)) {
		dev_dbg(&pdev->dev, "sidcfg:%08x\n", value);
		writel(value, base + EXT_CFG_SIDCONFIG);
	}

	return 0;
}

int psc_debugfs_create(struct platform_device *pdev)
{
	struct psc_debug_dev *dbg = &psc_debug;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int count;

	if (!debugfs_initialized())
		return -ENODEV;

	count = of_count_phandle_with_args(np, "mboxes", "#mbox-cells");
	if (count != 1) {
		dev_err(dev, "incorrect mboxes property in '%pOF'\n", np);
		return -EINVAL;
	}

	debugfs_root = debugfs_create_dir("psc", NULL);
	if (debugfs_root == NULL) {
		dev_err(dev, "failed to create psc debugfs\n");
		return -EINVAL;
	}

	dbg->cl.dev = dev;
	dbg->cl.rx_callback = psc_chan_rx_callback;
	dbg->cl.tx_block = false;
	dbg->cl.tx_tout = DEFAULT_TX_TIMEOUT;
	dbg->cl.knows_txdone = false;
	dbg->pdev = pdev;

	mutex_init(&dbg->lock);

	debugfs_create_x64("tx_timeout", 0644, debugfs_root,
			(u64 *)&dbg->cl.tx_tout);
	debugfs_create_file("mbox_dbg", 0600, debugfs_root,
			dbg, &psc_debug_fops);

	setup_extcfg(pdev, dbg, debugfs_root);

	dma_set_mask_and_coherent(dev, DMA_BIT_MASK(39));

	return 0;
}

void psc_debugfs_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s\n", __func__);

	mutex_destroy(&psc_debug.lock);
	debugfs_remove_recursive(debugfs_root);
}
