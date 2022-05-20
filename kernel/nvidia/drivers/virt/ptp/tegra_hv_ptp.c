/*
 * drivers/virt/ptp/tegra_hv_ptp.c
 *
 * Copyright (c) 2019-2020, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/tegra-ivc.h>
#include <linux/version.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <uapi/linux/tegra_hv_ptp_ioctl.h>

/* Values taken from ptp_ivc.h */
#define NV_VIRT_PTP_MODE_DISABLED 0
#define NV_VIRT_PTP_MODE_CLIENT 1
#define NV_VIRT_PTP_MODE_SERVER 2

struct tegra_hv_ptp {
	struct platform_device *pdev;
	struct cdev cdev;
	struct device *dev;
	struct tegra_hv_ivc_cookie *ivc;
	struct task_struct *thread;
	wait_queue_head_t notify;
	unsigned int id;
	struct mutex lock;
	bool stop;
	struct tegra_hv_ptp_payload saved_time;
};

/* A pointer to the 'current' hv instance. */
/* It is expected that there will only be one instance, */
static struct tegra_hv_ptp *s_hv;
struct mutex s_hv_lock;
static struct class *s_hv_ptp_class;
static dev_t s_hv_ptp_devt;
static DEFINE_MUTEX(s_hv_ptp_lock);
static DEFINE_IDR(s_hv_ptp_idr);

static int tegra_hv_ptp_can_read(struct tegra_hv_ptp *hv)
{
	int ret;

	if (hv->stop == true)
		ret = 1;
	else
		ret = tegra_hv_ivc_can_read(hv->ivc);

	return ret;
}

static int tegra_hv_ptp_notified(struct tegra_hv_ptp *hv)
{
	int ret;

	if (hv->stop == true)
		ret = 0;
	else
		ret = tegra_hv_ivc_channel_notified(hv->ivc);

	return ret;
}

static int tegra_hv_ptp_loop(void *arg)
{
	struct tegra_hv_ptp *hv = (struct tegra_hv_ptp *)arg;
	u64 buf[8];
	struct tegra_hv_ptp_payload *payload;
	int ret;

	mutex_lock(&hv->lock);
	/* Reset the IVC channel, then wait for an interrupt */
	tegra_hv_ivc_channel_reset(hv->ivc);
	mutex_unlock(&hv->lock);

	wait_event_interruptible(hv->notify, tegra_hv_ptp_notified(hv) == 0);

	while (!kthread_should_stop()) {
		/* Try to read from the IVC channel */
		ret = tegra_hv_ivc_read(hv->ivc, &buf,
					sizeof(buf));
		if (ret < 1) {
			/* Nothing to read, wait for an interrupt */
			wait_event_interruptible(hv->notify,
						tegra_hv_ptp_can_read(hv) != 0);
		} else {
			mutex_lock(&hv->lock);

			/* We have a message - store the PHC and GT values */
			payload = (struct tegra_hv_ptp_payload *) &buf;
			hv->saved_time = *payload;

			mutex_unlock(&hv->lock);
		}
	}

	return 0;
}

static irqreturn_t tegra_hv_ptp_interrupt(int irq, void *data)
{
	struct tegra_hv_ptp *hv = (struct tegra_hv_ptp *)data;

	wake_up(&hv->notify);

	return IRQ_HANDLED;
}

static int tegra_hv_ptp_parse(struct platform_device *pdev,
	struct device_node **qn, u32 *id, u32 *mode)
{
	struct device_node *dn;

	dn = pdev->dev.of_node;
	if (dn == NULL) {
		dev_err(&pdev->dev, "failed to find device node\n");
		return -EINVAL;
	}

	if (of_property_read_u32_index(dn, "mode", 0, mode) != 0) {
		dev_err(&pdev->dev, "failed to find mode property\n");
		return -EINVAL;
	}

	if (of_property_read_u32_index(dn, "ivc", 1, id) != 0) {
		dev_err(&pdev->dev, "failed to find ivc property\n");
		return -EINVAL;
	}

	*qn = of_parse_phandle(dn, "ivc", 0);
	if (*qn == NULL) {
		dev_err(&pdev->dev, "failed to find queue node\n");
		return -EINVAL;
	}

	return 0;
}

static int tegra_hv_ptp_open(struct inode *inode, struct file *file)
{
	struct tegra_hv_ptp *hv = container_of(inode->i_cdev,
						struct tegra_hv_ptp, cdev);
	file->private_data = hv;
	kobject_get(&hv->dev->kobj);

	return 0;
}

static int tegra_hv_ptp_close(struct inode *inode, struct file *file)
{
	struct tegra_hv_ptp *hv = container_of(inode->i_cdev,
						struct tegra_hv_ptp, cdev);
	kobject_put(&hv->dev->kobj);

	return 0;
}

static long tegra_hv_ptp_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct tegra_hv_ptp_payload local;
	struct tegra_hv_ptp *hv = (struct tegra_hv_ptp *) file->private_data;
	void __user *uarg = (void __user *)arg;
	int err;

	/* Return back the PHC GT saved values */
	/* The usermode app will perform the adjustment for current time */
	switch (cmd) {
		case TEGRA_HV_PTP_GETTIME: {
			mutex_lock(&hv->lock);
			local = hv->saved_time;
			mutex_unlock(&hv->lock);

			err = copy_to_user(uarg, &local,
			     sizeof(struct tegra_hv_ptp_payload));
			if (err)
				return -EFAULT;
			break;
		}
		default: {
			return -ENOTTY;
		}
	}
	return 0;
}

static const struct file_operations fops = {
	.owner		= THIS_MODULE,
	.open           = tegra_hv_ptp_open,
	.release        = tegra_hv_ptp_close,
	.unlocked_ioctl	= tegra_hv_ptp_ioctl,
};

static int tegra_hv_ptp_setup_no_cleanup(struct tegra_hv_ptp *hv,
	struct platform_device *pdev, struct device_node *qn, unsigned int id)
{
	struct tegra_hv_ptp_payload blank_nvptp = {0};
	int errcode;

	init_waitqueue_head(&hv->notify);
	mutex_init(&hv->lock);

	hv->pdev = pdev;
	hv->saved_time = blank_nvptp;

	hv->ivc = tegra_hv_ivc_reserve(qn, id, NULL);
	if (IS_ERR_OR_NULL(hv->ivc)) {
		dev_err(&pdev->dev, "failed to reserve ivc %u\n", id);
		return -EINVAL;
	}

	errcode = devm_request_irq(&pdev->dev, hv->ivc->irq,
		tegra_hv_ptp_interrupt, 0, dev_name(&pdev->dev), (void *)hv);
	if (errcode < 0) {
		dev_err(&pdev->dev, "failed to get irq %d\n", hv->ivc->irq);
		tegra_hv_ivc_unreserve(hv->ivc);
		return -EINVAL;
	}

	hv->thread = kthread_create(tegra_hv_ptp_loop, (void *)hv,
		"tegra-hv-ptp");
	if (IS_ERR_OR_NULL(hv->thread)) {
		dev_err(&pdev->dev, "failed to create kthread\n");
		tegra_hv_ivc_unreserve(hv->ivc);
		return -EINVAL;
	}

	return 0;
}

static void tegra_hv_ptp_cleanup(struct tegra_hv_ptp *hv)
{
	int errcode;

	hv->stop = true;

	if (!IS_ERR_OR_NULL(hv->thread)) {
		errcode = kthread_stop(hv->thread);
		if ((errcode != 0) && (errcode != -EINTR))
			dev_err(&hv->pdev->dev, "failed to stop thread\n");
	}

	if (!IS_ERR_OR_NULL(hv->ivc)) {
		if (tegra_hv_ivc_unreserve(hv->ivc) != 0)
			dev_err(&hv->pdev->dev, "failed to unreserve ivc\n");
	}
}

static void tegra_hv_ptp_release(struct device *dev)
{
	struct tegra_hv_ptp *hv = dev_get_drvdata(dev);

	cdev_del(&hv->cdev);

	mutex_lock(&s_hv_ptp_lock);
	idr_remove(&s_hv_ptp_idr, hv->id);
	mutex_unlock(&s_hv_ptp_lock);
}

static int tegra_hv_ptp_probe(struct platform_device *pdev)
{
	struct tegra_hv_ptp *hv;
	struct device_node *qn;
	int errcode;
	dev_t devt;
	u32 id;
	u32 mode;

	mutex_init(&s_hv_lock);

	if (!is_tegra_hypervisor_mode()) {
		dev_info(&pdev->dev, "hypervisor is not present\n");
		return -ENODEV;
	}

	errcode = tegra_hv_ptp_parse(pdev, &qn, &id, &mode);
	if (errcode < 0) {
		dev_err(&pdev->dev, "failed to parse device tree\n");
		return -ENODEV;
	}

	if (mode != NV_VIRT_PTP_MODE_CLIENT) {
		of_node_put(qn);
		dev_info(&pdev->dev,
		"only client mode is supported, mode read = %u\n", mode);
		return -ENODEV;
	}

	hv = devm_kzalloc(&pdev->dev, sizeof(*hv), GFP_KERNEL);
	if (!hv) {
		of_node_put(qn);
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	/*
	 * Note that if the setup function fails, there may be
	 * dangling resources. So, we must clean up after failure.
	 */

	errcode = tegra_hv_ptp_setup_no_cleanup(hv, pdev, qn, id);
	of_node_put(qn);

	if (errcode < 0) {
		dev_err(&pdev->dev, "failed to setup device\n");
		goto fail_hv_cleanup;
	}

	platform_set_drvdata(pdev, hv);

	s_hv_ptp_class = class_create(THIS_MODULE, "hv_ptp");
	if (IS_ERR(s_hv_ptp_class)) {
		dev_err(&pdev->dev, "failed to allocate class\n");
		errcode = PTR_ERR(s_hv_ptp_class);
		goto fail_hv_cleanup;
	}

	errcode = alloc_chrdev_region(&s_hv_ptp_devt, 0, 1, "hv_ptp");
	if (errcode < 0) {
		dev_err(&pdev->dev, "failed to allocate char device region\n");
		goto fail_class;
	}

	/* get an idr for the device */
	mutex_lock(&s_hv_ptp_lock);
	errcode = idr_alloc(&s_hv_ptp_idr, hv, 0, 1, GFP_KERNEL);
	if (errcode < 0) {
		if (errcode == -ENOSPC) {
			dev_err(&pdev->dev, "hv_ptp: out of idr\n");
			errcode = -EBUSY;
		}
		mutex_unlock(&s_hv_ptp_lock);
		goto fail_chrdev;
	}
	hv->id = errcode;
	mutex_unlock(&s_hv_ptp_lock);

	/* associate the cdev with the file operations */
	cdev_init(&hv->cdev, &fops);

	/* build up the device number */
	devt = MKDEV(MAJOR(s_hv_ptp_devt), hv->id);
	hv->cdev.owner = THIS_MODULE;

	/* create the device node */
	hv->dev = device_create(s_hv_ptp_class, NULL, devt, hv, "hv_ptp%d",
				hv->id);

	if (IS_ERR(hv->dev)) {
		cdev_del(&hv->cdev);

		errcode = PTR_ERR(hv->dev);
		goto fail_idr;

	}

	hv->dev->release = tegra_hv_ptp_release;

	errcode = cdev_add(&hv->cdev, devt, 1);
	if (errcode) {
		dev_err(&pdev->dev, "hv_ptp: failed to add char device %d:%d\n",
			MAJOR(s_hv_ptp_devt), hv->id);

		errcode = PTR_ERR(hv->dev);
		goto fail_device;
	}

	dev_info(&pdev->dev, "hv_ptp cdev(%d:%d)\n",
		 MAJOR(s_hv_ptp_devt), hv->id);
	errcode = wake_up_process(hv->thread);
	if (errcode != 1)
		dev_err(&pdev->dev, "failed to wake up thread\n");

	dev_info(&pdev->dev, "id=%u irq=%d peer=%d num=%d size=%d\n",
		id, hv->ivc->irq, hv->ivc->peer_vmid, hv->ivc->nframes,
		hv->ivc->frame_size);

	mutex_lock(&s_hv_lock);

	if (s_hv == NULL)
		s_hv = hv;

	mutex_unlock(&s_hv_lock);
	return 0;

fail_device:
	device_destroy(s_hv_ptp_class, hv->dev->devt);

fail_idr:
	mutex_lock(&s_hv_ptp_lock);
	idr_remove(&s_hv_ptp_idr, hv->id);
	mutex_unlock(&s_hv_ptp_lock);

fail_chrdev:
	unregister_chrdev_region(s_hv_ptp_devt, 1);
fail_class:
	class_destroy(s_hv_ptp_class);
fail_hv_cleanup:
	tegra_hv_ptp_cleanup(hv);

	return errcode;
}

static int tegra_hv_ptp_remove(struct platform_device *pdev)
{
	struct tegra_hv_ptp *hv = platform_get_drvdata(pdev);

	/*
	 * The below cleanup function will block waiting for the
	 * refresh kthread to exit (if it has already started running.)
	 */

	tegra_hv_ptp_cleanup(hv);
	device_destroy(s_hv_ptp_class, hv->dev->devt);
	platform_set_drvdata(pdev, NULL);

	class_unregister(s_hv_ptp_class);
	class_destroy(s_hv_ptp_class);
	unregister_chrdev_region(s_hv_ptp_devt, 1);

	mutex_lock(&s_hv_lock);
	if (s_hv == hv)
		s_hv = NULL;
	mutex_unlock(&s_hv_lock);

	return 0;
}

static inline uint64_t ClockCycles(void)
{
	uint64_t result;

	asm volatile ("mrs %0, cntvct_el0" : "=r"(result));
	return result;
}

int get_ptp_virt_time(u64 *ns)
{
	int ret = 0;
	struct tegra_hv_ptp_payload local;

	/* As this is called from a driver, there is no hv pointer passed in */
	/* Use the s_hv static version, so lock it to prevent unloading */
	mutex_lock(&s_hv_lock);

	if (s_hv) {
		/* Take a copy of the saved time, process using that */
		mutex_lock(&s_hv->lock);
		local = s_hv->saved_time;
		mutex_unlock(&s_hv->lock);

		/* If the GT value is 0, then PTP wasn't running */
		if (local.gt == 0) {
			*ns = 0;
			ret = -EINVAL;
		} else {
			/* Have a valid pair of PHC and GT */
			/* GT ticks at 32x PHC, so adjust for current time */
			*ns = local.phc_ns + (1000000000UL * local.phc_sec);
			*ns += (ClockCycles() - local.gt)*(32UL);
		}

	} else {
		*ns = 0;
		ret = -EINVAL;
	}

	mutex_unlock(&s_hv_lock);
	return ret;
}
EXPORT_SYMBOL(get_ptp_virt_time);

static const struct of_device_id tegra_hv_ptp_match[] = {
	{ .compatible = "nvidia,tegra-hv-ptp", },
	{}
};

static struct platform_driver tegra_hv_ptp_driver = {
	.probe		= tegra_hv_ptp_probe,
	.remove		= tegra_hv_ptp_remove,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "tegra_hv_ptp",
		.of_match_table = of_match_ptr(tegra_hv_ptp_match),
	},
};

module_platform_driver(tegra_hv_ptp_driver);

MODULE_AUTHOR("NVIDIA Corporation");
MODULE_DESCRIPTION("Tegra Hypervisor PTP Driver");
MODULE_LICENSE("GPL");
