/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

/**
 * @file drivers/media/platform/tegra/camera/fusa-capture/capture-isp-channel.c
 *
 * @brief ISP channel character device driver for the T186/T194 Camera RTCPU
 * platform.
 */

#include <asm/ioctls.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/nvhost.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/stddef.h>
#include <linux/uaccess.h>

#include "nvhost_acm.h"
#include <media/fusa-capture/capture-isp.h>

#include <media/fusa-capture/capture-isp-channel.h>

/**
 * @todo This parameter is platform-dependent and should be retrieved from the
 * Device Tree.
 */
#define MAX_ISP_CHANNELS	64

/**
 * @brief ISP channel character device driver context.
 */
struct isp_channel_drv {
	struct device *dev; /**< ISP kernel @em device */
	u8 num_channels; /**< No. of ISP channel character devices */
	struct mutex lock; /**< ISP channel driver context lock. */
	struct platform_device *ndev; /**< ISP kernel @em platform_device */
	const struct isp_channel_drv_ops *ops;
		/**< ISP fops for Host1x syncpt/gos allocations */
	struct tegra_isp_channel *channels[];
		/**< Allocated ISP channel contexts */
};

/**
 * @defgroup ISP_CHANNEL_IOCTLS
 *
 * @brief ISP channel character device IOCTL API
 *
 * Clients in the UMD may open sysfs character devices representing ISP
 * channels, and perform configuration, and enqueue buffers in capture and
 * program requests to the low-level RCE subsystem via these IOCTLs.
 *
 * @{
 */

/**
 * @brief Set up ISP channel resources and request FW channel allocation in RCE.
 *
 * Initialize the ISP channel context and synchronization primitives, pin memory
 * for the capture and program process descriptor queues, set up the buffer
 * management table, initialize the capture/capture-control IVC channels and
 * request ISP FW channel allocation in RCE.
 *
 * @param[in]	ptr	Pointer to a struct @ref isp_capture_setup
 *
 * @returns	0 (success), neg. errno (failure)
 */
#define ISP_CAPTURE_SETUP \
	_IOW('I', 1, struct isp_capture_setup)

/**
 * @brief Release the ISP FW channel allocation in RCE, and all resources and
 * contexts in the KMD.
 *
 * @param[in]	rel	uint32_t bitmask of @ref CAPTURE_CHANNEL_RESET_FLAGS
 * @returns	0 (success), neg. errno (failure)
 */
#define ISP_CAPTURE_RELEASE \
	_IOW('I', 2, __u32)

/**
 * @brief Reset the ISP channel in RCE synchronously w/ the KMD; all pending
 * capture/program descriptors in the queue are discarded and syncpoint values
 * fast-forwarded to unblock waiting clients.
 *
 * @param[in]	rst	uint32_t bitmask of @ref CAPTURE_CHANNEL_RESET_FLAGS
 *
 * @returns	0 (success), neg. errno (failure)
 */
#define ISP_CAPTURE_RESET \
	_IOW('I', 3, __u32)

/**
 * @brief Retrieve the ids and current values of the progress, stats progress
 * syncpoints, and ISP FW channel allocated by RCE.
 *
 * If successful, the queried values are written back to the input struct.
 *
 * @param[in,out]	ptr	Pointer to a struct @ref isp_capture_info
 *
 * @returns	0 (success), neg. errno (failure)
 */
#define ISP_CAPTURE_GET_INFO \
	_IOR('I', 4, struct isp_capture_info)

/**
 * @brief Enqueue a process capture request to RCE, input and prefences are
 * allocated, and the addresses to surface buffers in the descriptor (referenced
 * by the buffer_index) are pinned and patched.
 *
 * @param[in]	ptr	Pointer to a struct @ref isp_capture_req
 *
 * @returns	0 (success), neg. errno (failure)
 */
#define ISP_CAPTURE_REQUEST \
	_IOW('I', 5, struct isp_capture_req)

/**
 * @brief Wait on the next completion of an enqueued frame, signalled by RCE.
 * The status in the frame's capture descriptor is safe to read when this
 * completes w/o a -ETIMEDOUT or other error.
 *
 * @note This call completes for the frame at the head of the FIFO queue, and is
 * not necessarily for the most recently enqueued process capture request.
 *
 * @param[in]	status	uint32_t timeout [ms], 0 for indefinite
 *
 * @returns	0 (success), neg. errno (failure)
 */
#define ISP_CAPTURE_STATUS \
	_IOW('I', 6, __u32)

/**
 * @brief Enqueue a program request to RCE, the addresses to the push buffer in
 * the descriptor (referenced by the buffer_index) are pinned and patched.
 *
 * @param[in]	ptr	Pointer to a struct @ref isp_program_req
 *
 * @returns	0 (success), neg. errno (failure)
 */
#define ISP_CAPTURE_PROGRAM_REQUEST \
	_IOW('I', 7, struct isp_program_req)

/**
 * @brief Wait on the next completion of an enqueued program, signalled by RCE.
 * The program execution is finished and is safe to free when this call
 * completes.
 *
 * @note This call completes for the program at the head of the FIFO queue, and
 * is not necessarily for the most recently enqueued program request.

 * @returns	0 (success), neg. errno (failure)
 */
#define ISP_CAPTURE_PROGRAM_STATUS \
	_IOW('I', 8, __u32)

/**
 * @brief Enqueue a joint capture and program request to RCE; this is equivalent
 * to calling @ref ISP_CAPTURE_PROGRAM_REQUEST and @ref ISP_CAPTURE_REQUEST
 * sequentially, but the number of KMD-RCE IVC transmissions is reduced to one
 * in each direction for every frame.
 *
 * @param[in]	ptr	Pointer to a struct @ref isp_capture_req_ex
 *
 * @returns	0 (success), neg. errno (failure)
 */
#define ISP_CAPTURE_REQUEST_EX \
	_IOW('I', 9, struct isp_capture_req_ex)

/**
 * @brief Set up the combined capture and program process progress status
 * notifier array, which is a replacement for the blocking
 * @ref ISP_CAPTURE_STATUS and @ref ISP_CAPTURE_PROGRAM_STATUS calls; allowing
 * for out-of-order frame process completion notifications.
 *
 * The values written by the KMD are any of the
 * @ref CAPTURE_PROGRESS_NOTIFIER_STATES.
 *
 * @param[in]	ptr	Pointer to a struct @ref isp_capture_progress_status_req
 *
 * @returns	0 (success), neg. errno (failure)
 */
#define ISP_CAPTURE_SET_PROGRESS_STATUS_NOTIFIER \
	_IOW('I', 10, struct isp_capture_progress_status_req)

/**
 * @brief Perform an operation on the surface buffer by setting the bitwise
 * @a flag field with @ref CAPTURE_BUFFER_OPS flags.
 *
 * @param[in]	ptr	Pointer to a struct @ref isp_buffer_req.
 *
 * @returns	0 (success), neg. errno (failure)
 */
#define ISP_CAPTURE_BUFFER_REQUEST \
	_IOW('I', 11, struct isp_buffer_req)

/** @} */

/**
 * @brief Power on ISP via Host1x. The ISP channel is registered as an NvHost
 * ISP client and the reference count is incremented by one.
 *
 * @param[in]	chan	ISP channel context
 * @returns	0 (success), neg. errno (failure)
 */
static int isp_channel_power_on(
	struct tegra_isp_channel *chan)
{
	int ret = 0;

	dev_dbg(chan->isp_dev, "isp_channel_power_on\n");

	ret = nvhost_module_add_client(chan->ndev, chan->priv);
	if (ret < 0) {
		dev_err(chan->isp_dev,
			"%s: failed to add isp client\n", __func__);
		return ret;
	}

	ret = nvhost_module_busy(chan->ndev);
	if (ret < 0) {
		dev_err(chan->isp_dev,
			"%s: failed to power on isp\n", __func__);
		return ret;
	}

	return 0;
}

/**
 * @brief Power off ISP via Host1x. The NvHost module reference count is
 * decreased by one and the ISP channel is unregistered as a client.
 *
 * @param[in]	chan	ISP channel context
 */
static void isp_channel_power_off(
	struct tegra_isp_channel *chan)
{
	dev_dbg(chan->isp_dev, "isp_channel_power_off\n");

	nvhost_module_idle(chan->ndev);
	nvhost_module_remove_client(chan->ndev, chan->priv);
}

static struct isp_channel_drv *chdrv_;
static DEFINE_MUTEX(chdrv_lock);

/**
 * @brief Open an ISP channel character device node, power on the camera
 * subsystem and initialize the channel driver context.
 *
 * The act of opening an ISP channel character device node does not entail the
 * reservation of an ISP channel, ISP_CAPTURE_SETUP must be called afterwards
 * to request an allocation by RCE.
 *
 * This is the @a open file operation handler for an ISP channel node.
 *
 * @param[in]	inode	ISP channel character device inode struct
 * @param[in]	file	ISP channel character device file struct
 *
 * @returns	0 (success), neg. errno (failure)
 */
static int isp_channel_open(
	struct inode *inode,
	struct file *file)
{
	struct tegra_isp_channel *chan;
	unsigned int channel = iminor(inode);
	struct isp_channel_drv *chan_drv;
	int err;

	if (mutex_lock_interruptible(&chdrv_lock))
		return -ERESTARTSYS;

	chan_drv = chdrv_;

	if (chan_drv == NULL || channel >= chan_drv->num_channels) {
		mutex_unlock(&chdrv_lock);
		return -ENODEV;
	}
	mutex_unlock(&chdrv_lock);

	chan = kzalloc(sizeof(*chan), GFP_KERNEL);
	if (unlikely(chan == NULL))
		return -ENOMEM;

	chan->drv = chan_drv;
	chan->isp_dev = chan_drv->dev;
	chan->ndev = chan_drv->ndev;
	chan->ops = chan_drv->ops;
	chan->priv = file;

	err = isp_channel_power_on(chan);
	if (err < 0)
		goto error;

	err = isp_capture_init(chan);
	if (err < 0)
		goto init_err;

	mutex_lock(&chan_drv->lock);
	if (chan_drv->channels[channel] != NULL) {
		mutex_unlock(&chan_drv->lock);
		err = -EBUSY;
		goto chan_err;
	}

	chan_drv->channels[channel] = chan;
	mutex_unlock(&chan_drv->lock);

	file->private_data = chan;

	return nonseekable_open(inode, file);

chan_err:
	isp_capture_shutdown(chan);
init_err:
	isp_channel_power_off(chan);
error:
	kfree(chan);
	return err;
}

/**
 * @brief Release an ISP channel character device node, power off the camera
 * subsystem and free the ISP channel driver context.
 *
 * Under normal operation, ISP_CAPTURE_RESET followed by ISP_CAPTURE_RELEASE
 * should be called before releasing the file handle on the device node.
 *
 * This is the @a release file operation handler for an ISP channel node.
 *
 * @param[in]	inode	ISP channel character device inode struct
 * @param[in]	file	ISP channel character device file struct
 *
 * @returns	0
 */
static int isp_channel_release(
	struct inode *inode,
	struct file *file)
{
	struct tegra_isp_channel *chan = file->private_data;
	unsigned int channel = iminor(inode);
	struct isp_channel_drv *chan_drv = chan->drv;

	isp_capture_shutdown(chan);
	isp_channel_power_off(chan);

	mutex_lock(&chan_drv->lock);

	WARN_ON(chan_drv->channels[channel] != chan);
	chan_drv->channels[channel] = NULL;

	mutex_unlock(&chan_drv->lock);
	kfree(chan);

	return 0;
}

/**
 * @brief Process an IOCTL call on an ISP channel character device.
 *
 * Depending on the specific IOCTL, the argument (@a arg) may be a pointer to a
 * defined struct payload that is copied from or back to user-space. This memory
 * is allocated and mapped from user-space and must be kept available until
 * after the IOCTL call completes.
 *
 * This is the @a ioctl file operation handler for an ISP channel node.
 *
 * @param[in]		file	ISP channel character device file struct
 * @param[in]		cmd	ISP channel IOCTL command
 * @param[in,out]	arg	IOCTL argument; numerical value or pointer
 *
 * @returns		0 (success), neg. errno (failure)
 */
static long isp_channel_ioctl(
	struct file *file,
	unsigned int cmd,
	unsigned long arg)
{
	struct tegra_isp_channel *chan = file->private_data;
	void __user *ptr = (void __user *)arg;
	long err = -EFAULT;

	if (unlikely(chan == NULL)) {
		pr_err("%s: invalid channel\n", __func__);
		return -EINVAL;
	}

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(ISP_CAPTURE_SETUP): {
		struct isp_capture_setup setup;

		if (copy_from_user(&setup, ptr, sizeof(setup)))
			break;
		err = isp_capture_setup(chan, &setup);
		if (err)
			dev_err(chan->isp_dev, "isp capture setup failed\n");
		break;
	}

	case _IOC_NR(ISP_CAPTURE_RESET): {
		uint32_t rst;

		if (copy_from_user(&rst, ptr, sizeof(rst)))
			break;
		err = isp_capture_reset(chan, rst);
		if (err)
			dev_err(chan->isp_dev, "isp capture reset failed\n");
		break;
	}

	case _IOC_NR(ISP_CAPTURE_RELEASE): {
		uint32_t rel;

		if (copy_from_user(&rel, ptr, sizeof(rel)))
			break;
		err = isp_capture_release(chan, rel);
		if (err)
			dev_err(chan->isp_dev, "isp capture release failed\n");
		break;
	}

	case _IOC_NR(ISP_CAPTURE_GET_INFO): {
		struct isp_capture_info info;
		(void)memset(&info, 0, sizeof(info));

		err = isp_capture_get_info(chan, &info);
		if (err) {
			dev_err(chan->isp_dev, "isp capture get info failed\n");
			break;
		}
		if (copy_to_user(ptr, &info, sizeof(info)))
			err = -EFAULT;
		break;
	}

	case _IOC_NR(ISP_CAPTURE_REQUEST): {
		struct isp_capture_req req;

		if (copy_from_user(&req, ptr, sizeof(req)))
			break;
		err = isp_capture_request(chan, &req);
		if (err)
			dev_err(chan->isp_dev,
				"isp process capture request submit failed\n");
		break;
	}

	case _IOC_NR(ISP_CAPTURE_STATUS): {
		uint32_t status;

		if (copy_from_user(&status, ptr, sizeof(status)))
			break;
		err = isp_capture_status(chan, status);
		if (err)
			dev_err(chan->isp_dev,
				"isp process get status failed\n");
		break;
	}

	case _IOC_NR(ISP_CAPTURE_PROGRAM_REQUEST): {
		struct isp_program_req program_req;

		if (copy_from_user(&program_req, ptr, sizeof(program_req)))
			break;
		err = isp_capture_program_request(chan, &program_req);
		if (err)
			dev_err(chan->isp_dev,
				"isp process program request submit failed\n");
		break;
	}

	case _IOC_NR(ISP_CAPTURE_PROGRAM_STATUS): {
		err = isp_capture_program_status(chan);

		if (err)
			dev_err(chan->isp_dev,
				"isp process program get status failed\n");
		break;
	}

	case _IOC_NR(ISP_CAPTURE_REQUEST_EX): {
		struct isp_capture_req_ex req;

		if (copy_from_user(&req, ptr, sizeof(req)))
			break;
		err = isp_capture_request_ex(chan, &req);
		if (err)
			dev_err(chan->isp_dev,
				"isp process request extended submit failed\n");
		break;
	}
	case _IOC_NR(ISP_CAPTURE_SET_PROGRESS_STATUS_NOTIFIER): {
		struct isp_capture_progress_status_req req;

		if (copy_from_user(&req, ptr, sizeof(req)))
			break;
		err = isp_capture_set_progress_status_notifier(chan, &req);
		if (err)
			dev_err(chan->isp_dev,
				"isp capture set progress status buffers failed\n");
		break;
	}
	case _IOC_NR(ISP_CAPTURE_BUFFER_REQUEST): {
		struct isp_buffer_req req;

		if (copy_from_user(&req, ptr, sizeof(req)) != 0U)
			break;

		err = isp_capture_buffer_request(chan, &req);
		if (err < 0)
			dev_err(chan->isp_dev, "isp buffer req failed\n");
		break;
	}
	default: {
		dev_err(chan->isp_dev, "%s:Unknown ioctl\n", __func__);
		return -ENOIOCTLCMD;
	}
	}

	return err;
}

static const struct file_operations isp_channel_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.unlocked_ioctl = isp_channel_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = isp_channel_ioctl,
#endif
	.open = isp_channel_open,
	.release = isp_channel_release,
};

/* Character device */
static struct class *isp_channel_class;
static int isp_channel_major;

int isp_channel_drv_register(
	struct platform_device *ndev,
	const struct isp_channel_drv_ops *ops)
{
	struct isp_channel_drv *chan_drv;
	unsigned int i;

	chan_drv = kzalloc(offsetof(struct isp_channel_drv,
			channels[MAX_ISP_CHANNELS]), GFP_KERNEL);
	if (unlikely(chan_drv == NULL))
		return -ENOMEM;

	chan_drv->dev = &ndev->dev;
	chan_drv->ndev = ndev;
	chan_drv->ops = ops;
	chan_drv->num_channels = MAX_ISP_CHANNELS;
	mutex_init(&chan_drv->lock);

	mutex_lock(&chdrv_lock);
	if (WARN_ON(chdrv_ != NULL)) {
		mutex_unlock(&chdrv_lock);
		kfree(chan_drv);
		return -EBUSY;
	}
	chdrv_ = chan_drv;
	mutex_unlock(&chdrv_lock);

	for (i = 0; i < chan_drv->num_channels; i++) {
		dev_t devt = MKDEV(isp_channel_major, i);

		device_create(isp_channel_class, chan_drv->dev, devt, NULL,
				"capture-isp-channel%u", i);
	}

	return 0;
}
EXPORT_SYMBOL(isp_channel_drv_register);

void isp_channel_drv_unregister(
	struct device *dev)
{
	struct isp_channel_drv *chan_drv;
	unsigned int i;

	mutex_lock(&chdrv_lock);
	chan_drv = chdrv_;
	chdrv_ = NULL;
	WARN_ON(chan_drv->dev != dev);
	mutex_unlock(&chdrv_lock);

	for (i = 0; i < chan_drv->num_channels; i++) {
		dev_t devt = MKDEV(isp_channel_major, i);

		device_destroy(isp_channel_class, devt);
	}

	kfree(chan_drv);
}
EXPORT_SYMBOL(isp_channel_drv_unregister);

/**
 * @brief Initialize the ISP channel driver device (major).
 *
 * @returns	0 (success), PTR_ERR or neg. ISP channel major no. (failuure)
 */
static int __init isp_channel_drv_init(void)
{
	isp_channel_class = class_create(THIS_MODULE, "capture-isp-channel");
	if (IS_ERR(isp_channel_class))
		return PTR_ERR(isp_channel_class);

	isp_channel_major = register_chrdev(0, "capture-isp-channel",
						&isp_channel_fops);
	if (isp_channel_major < 0) {
		class_destroy(isp_channel_class);
		return isp_channel_major;
	}

	return 0;
}

/**
 * @brief De-initialize the ISP channel driver device (major).
 */
static void __exit isp_channel_drv_exit(void)
{
	unregister_chrdev(isp_channel_major, "capture-isp-channel");
	class_destroy(isp_channel_class);
}

subsys_initcall(isp_channel_drv_init);
module_exit(isp_channel_drv_exit);
