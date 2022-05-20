/*
 * Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/**
 * @file tegra-fsicom.c
 * @brief <b> fsicom driver to communicate with fsicom daemon</b>
 *
 * This file will register as client driver so that FSICOM daemon can
 * utilize the smmu mapping and HSP drivers from kernel space
 */

/* ==================[Includes]============================================= */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/dma-buf.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/mailbox_client.h>
#include <linux/sched/signal.h>
#include "linux/tegra-fsicom.h"


/*Timeout in millisec*/
#define TIMEOUT		1000

/* =================[Data types]======================================== */

/*Data type for mailbox client and channel details*/
struct fsi_hsp_sm {
	struct mbox_client client;
	struct mbox_chan *chan;
};

/*Data type for accessing TOP2 HSP */
struct fsi_hsp {
	struct fsi_hsp_sm rx;
	struct fsi_hsp_sm tx;
	struct device dev;
};

/* =================[GLOBAL variables]================================== */
static ssize_t device_file_ioctl(
		struct file *, unsigned int cmd, unsigned long arg);

static int device_file_major_number;
static const char device_name[] = "fsicom-client";

static struct platform_device *pdev_local;
static struct dma_buf_attachment *attach;
static struct sg_table *sgt;
static struct dma_buf *dmabuf;

/* Signaling to Application */
static struct task_struct *task;

static struct fsi_hsp *fsi_hsp_v;

/*File operations*/
const static struct file_operations fsicom_driver_fops = {
	.owner   = THIS_MODULE,
	.unlocked_ioctl   = device_file_ioctl,
};

static int fsicom_register_device(void)
{
	int result = 0;
	struct class *dev_class;

	result = register_chrdev(0, device_name, &fsicom_driver_fops);
	if (result < 0) {
		pr_err("%s> register_chrdev code = %i\n", device_name, result);
		return result;
	}
	device_file_major_number = result;
	dev_class = class_create(THIS_MODULE, "fsicom_client");
	if (dev_class == NULL) {
		pr_err("%s> Could not create class for device\n", device_name);
		goto class_fail;
	}

	if ((device_create(dev_class, NULL,
		MKDEV(device_file_major_number, 0),
			 NULL, "fsicom_client")) == NULL) {
		pr_err("%s> Could not create device node\n", device_name);
		goto device_failure;
	}
	return 0;

device_failure:
	class_destroy(dev_class);
class_fail:
	unregister_chrdev(device_file_major_number, device_name);
	return -1;
}

static void fsicom_send_sgnal(int32_t data)
{

	struct siginfo info;

	/*Sending signal to app */
	memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = SIG_FSI_DAEMON;
	info.si_code = SI_QUEUE;
	info.si_int  = (u32) (unsigned long) data;
	if (task != NULL) {
		if (send_sig_info(SIG_FSI_DAEMON,
				(struct kernel_siginfo *)&info, task) < 0)
			pr_err("Unable to send signal\n");
	}
}

static void tegra_hsp_rx_notify(struct mbox_client *cl, void *msg)
{
	fsicom_send_sgnal((u32) (unsigned long)msg);
}

static void tegra_hsp_tx_empty_notify(struct mbox_client *cl,
					 void *data, int empty_value)
{
	pr_err("TX empty callback came\n");
}
static int tegra_hsp_mb_init(struct device *dev)
{
	int err;

	fsi_hsp_v = devm_kzalloc(dev, sizeof(*fsi_hsp_v), GFP_KERNEL);
	if (!fsi_hsp_v)
		return -ENOMEM;

	fsi_hsp_v->tx.client.dev = dev;
	fsi_hsp_v->rx.client.dev = dev;
	fsi_hsp_v->tx.client.tx_block = true;
	fsi_hsp_v->tx.client.tx_tout = TIMEOUT;
	fsi_hsp_v->rx.client.rx_callback = tegra_hsp_rx_notify;
	fsi_hsp_v->tx.client.tx_done = tegra_hsp_tx_empty_notify;

	fsi_hsp_v->tx.chan = mbox_request_channel_byname(&fsi_hsp_v->tx.client,
							"fsi-tx");
	if (IS_ERR(fsi_hsp_v->tx.chan)) {
		err = PTR_ERR(fsi_hsp_v->tx.chan);
		dev_err(dev, "failed to get tx mailbox: %d\n", err);
		devm_kfree(dev, fsi_hsp_v);
		return err;
	}

	fsi_hsp_v->rx.chan = mbox_request_channel_byname(&fsi_hsp_v->rx.client,
							"fsi-rx");
	if (IS_ERR(fsi_hsp_v->rx.chan)) {
		err = PTR_ERR(fsi_hsp_v->rx.chan);
		dev_err(dev, "failed to get rx mailbox: %d\n", err);
		devm_kfree(dev, fsi_hsp_v);
		return err;
	}

	return 0;
}


static void fsicom_unregister_device(void)
{
	if (device_file_major_number != 0)
		unregister_chrdev(device_file_major_number, device_name);
}

static ssize_t device_file_ioctl(
			struct file *fp, unsigned int cmd, unsigned long arg)
{
	struct rw_data input;
	dma_addr_t dma_addr;
	dma_addr_t phys_addr;
	struct rw_data *user_input;
	int ret;

	user_input = (struct rw_data *)arg;
	if (copy_from_user(&input, (struct rw_data *)arg,
				 sizeof(struct rw_data)))
		return -EACCES;

	switch (cmd) {

	case NVMAP_SMMU_MAP:
		dmabuf = dma_buf_get(input.handle);

		attach = dma_buf_attach(dmabuf, &pdev_local->dev);

		if (IS_ERR_OR_NULL(attach)) {
			pr_err("error : %lld\n", (signed long long)attach);
			return -1;
		}
		sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
		if (IS_ERR_OR_NULL(sgt)) {
			pr_err("error: %lld\n", (signed long long)sgt);
			return -1;
		}
		phys_addr = sg_phys(sgt->sgl);
		dma_addr = sg_dma_address(sgt->sgl);
		if (copy_to_user((void __user *)&user_input->pa,
				(void *)&phys_addr, sizeof(uint64_t)))
			return -EACCES;
		if (copy_to_user((void __user *)&user_input->iova,
				(void *)&dma_addr, sizeof(uint64_t)))
			return -EACCES;
		ret = 0;
	break;

	case NVMAP_SMMU_UNMAP:
		dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
		dma_buf_detach(dmabuf, attach);
		dma_buf_put(dmabuf);
	break;

	case TEGRA_HSP_WRITE:
		ret = mbox_send_message(fsi_hsp_v->tx.chan,
			(void *) (unsigned long) input.handle);
	break;

	case TEGRA_SIGNAL_REG:
		task = get_current();
		ret = 0;
	break;

	default:
		return -EINVAL;
	}

	return ret;
}

static const struct of_device_id fsicom_client_dt_match[] = {
	{ .compatible = "nvidia,tegra234-fsicom-client"},
	{}
};

MODULE_DEVICE_TABLE(of, fsicom_client_dt_match);

static int fsicom_client_probe(struct platform_device *pdev)
{
	int ret = 0;

	fsicom_register_device();
	ret = tegra_hsp_mb_init(&pdev->dev);
	pdev_local = pdev;

	return ret;
}

static int fsicom_client_remove(struct platform_device *pdev)
{
	fsicom_unregister_device();
	devm_kfree(&pdev->dev, fsi_hsp_v);
	return 0;
}

static struct platform_driver fsicom_client = {
	.driver         = {
	.name   = "fsicom_client",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = of_match_ptr(fsicom_client_dt_match),
	},
	.probe          = fsicom_client_probe,
	.remove         = fsicom_client_remove,
};

module_platform_driver(fsicom_client);

MODULE_DESCRIPTION("FSI-CCPLEX-COM driver");
MODULE_AUTHOR("Prashant Shaw <pshaw@nvidia.com>");
MODULE_LICENSE("GPL v2");
