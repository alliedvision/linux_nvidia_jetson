/*
 * VI channel driver for T186/T194
 *
 * Copyright (c) 2017-2020 NVIDIA Corporation.  All rights reserved.
 *
 * Author: Sudhir Vyas <svyas@nvidia.com>
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
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/nvhost.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>

#include <media/capture.h>
#include <media/capture_vi_channel.h>

#include "nvhost_acm.h"

#define VI_CAPTURE_SETUP	_IOW('I', 1, struct vi_capture_setup)
#define VI_CAPTURE_RELEASE	_IOW('I', 2, __u32)
#define VI_CAPTURE_SET_CONFIG	_IOW('I', 3, struct vi_capture_control_msg)
#define VI_CAPTURE_RESET	_IOW('I', 4, __u32)
#define VI_CAPTURE_GET_INFO	_IOR('I', 5, struct vi_capture_info)
#define VI_CAPTURE_REQUEST	_IOW('I', 6, struct vi_capture_req)
#define VI_CAPTURE_STATUS	_IOW('I', 7, __u32)
#define VI_CAPTURE_SET_COMPAND	_IOW('I', 8, struct vi_capture_compand)
#define VI_CAPTURE_SET_PROGRESS_STATUS_NOTIFIER \
	_IOW('I', 9, struct vi_capture_progress_status_req)
#define VI_CAPTURE_BUFFER_REQUEST \
	_IOW('I', 10, struct vi_buffer_req)

struct vi_channel_drv {
	struct device *dev;
	struct platform_device *ndev;
	struct mutex lock;
	u8 num_channels;
	const struct vi_channel_drv_ops *ops;
	struct tegra_vi_channel __rcu *channels[];
};

void vi_capture_request_unpin(struct tegra_vi_channel *chan,
		uint32_t buffer_index)
{
	struct vi_capture *capture = chan->capture_data;
	struct capture_common_unpins *unpins;
	int i = 0;

	mutex_lock(&capture->unpins_list_lock);
	unpins = &capture->unpins_list[buffer_index];

	if (unpins->num_unpins != 0) {
		for (i = 0; i < unpins->num_unpins; i++)
			put_mapping(capture->buf_ctx, unpins->data[i]);
		(void)memset(unpins, 0U,sizeof(*unpins));
	}
	mutex_unlock(&capture->unpins_list_lock);
}
EXPORT_SYMBOL(vi_capture_request_unpin);

/**
 * Pin/map buffers and save iova boundaries into corresponding
 * memoryinfo struct.
 */
int pin_vi_capture_request_buffers_locked(struct tegra_vi_channel *chan,
		struct vi_capture_req *req,
		struct capture_common_unpins *request_unpins)
{
	struct vi_capture *capture = chan->capture_data;
	struct capture_descriptor* desc = (struct capture_descriptor*)
		(capture->requests.va +
				req->buffer_index * capture->request_size);

	struct capture_descriptor_memoryinfo* desc_mem =
			&capture->requests_memoryinfo[req->buffer_index];
	int i;
	int err = 0;

	/* Buffer count: ATOMP surfaces + engine_surface */
	BUG_ON(VI_NUM_ATOMP_SURFACES + 1U >= MAX_PIN_BUFFER_PER_REQUEST);

	for (i = 0; i < VI_NUM_ATOMP_SURFACES; i++) {
		err = capture_common_pin_and_get_iova(capture->buf_ctx,
			desc->ch_cfg.atomp.surface[i].offset_hi,
			desc->ch_cfg.atomp.surface[i].offset,
			&desc_mem->surface[i].base_address, &desc_mem->surface[i].size,
			request_unpins);

		if (err) {
			dev_err(chan->dev, "%s: get atomp iova failed\n", __func__);
			goto fail;
		}
	}

	err = capture_common_pin_and_get_iova(capture->buf_ctx,
		desc->engine_status.offset_hi,
		desc->engine_status.offset,
		&desc_mem->engine_status.base_address,
		&desc_mem->engine_status.size,
		request_unpins);

	if (err) {
		dev_err(chan->dev, "%s: get engine surf iova failed\n", __func__);
		goto fail;
	}

fail:
	/* Unpin cleanup is done in vi_capture_request_unpin() */
	return err;
}

static long vi_channel_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct tegra_vi_channel *chan = file->private_data;
	struct vi_capture *capture = chan->capture_data;
	void __user *ptr = (void __user *)arg;
	int err = -EFAULT;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(VI_CAPTURE_SETUP): {
		struct vi_capture_setup setup;

		if (copy_from_user(&setup, ptr, sizeof(setup)))
			break;

		if (setup.request_size < sizeof(struct capture_descriptor)) {
			dev_err(chan->dev,
				"request size is too small to fit capture descriptor\n");
			return -EINVAL;
		}

		capture->buf_ctx = create_buffer_table(chan->dev);
		if (capture->buf_ctx == NULL) {
			dev_err(chan->dev, "vi buffer setup failed");
			break;
		}

		/* pin the capture descriptor ring buffer */
		err = capture_common_pin_memory(capture->rtcpu_dev,
				setup.mem, &capture->requests);
		if (err < 0) {
			dev_err(chan->dev, "%s: memory setup failed\n", __func__);
			destroy_buffer_table(capture->buf_ctx);
			return -EFAULT;
		}

		/* Check that buffer size matches queue depth */
		if ((capture->requests.buf->size / setup.request_size) <
				setup.queue_depth) {
			dev_err(chan->dev,
				"%s: descriptor buffer is too small for given queue depth\n",
				__func__);
			capture_common_unpin_memory(&capture->requests);
			destroy_buffer_table(capture->buf_ctx);
			return -ENOMEM;
		}

		setup.iova = capture->requests.iova;
		err = vi_capture_setup(chan, &setup);
		if (err < 0) {
			dev_err(chan->dev, "vi capture setup failed\n");
			capture_common_unpin_memory(&capture->requests);
			destroy_buffer_table(capture->buf_ctx);
			return err;
		}
		break;
	}

	case _IOC_NR(VI_CAPTURE_RESET): {
		uint32_t reset_flags;
		int i;

		if (copy_from_user(&reset_flags, ptr, sizeof(reset_flags)))
			break;

		err = vi_capture_reset(chan, reset_flags);
		if (err < 0)
			dev_err(chan->dev, "vi capture reset failed\n");
		else {
			for (i = 0; i < capture->queue_depth; i++)
				vi_capture_request_unpin(chan, i);
		}
		break;
	}

	case _IOC_NR(VI_CAPTURE_RELEASE): {
		uint32_t reset_flags;
		int i;

		if (copy_from_user(&reset_flags, ptr, sizeof(reset_flags)))
			break;

		err = vi_capture_release(chan, reset_flags);
		if (err < 0)
			dev_err(chan->dev, "vi capture release failed\n");
		else {
			for (i = 0; i < capture->queue_depth; i++)
				vi_capture_request_unpin(chan, i);
			capture_common_unpin_memory(&capture->requests);
			vfree(capture->unpins_list);
			capture->unpins_list = NULL;
			destroy_buffer_table(capture->buf_ctx);
		}
		break;
	}

	case _IOC_NR(VI_CAPTURE_GET_INFO): {
		struct vi_capture_info info = { {0} };

		err = vi_capture_get_info(chan, &info);
		if (err < 0)
			dev_err(chan->dev, "vi capture get info failed\n");
		if (copy_to_user(ptr, &info, sizeof(info)))
			err = -EFAULT;
		break;
	}

	case _IOC_NR(VI_CAPTURE_SET_CONFIG): {
		struct vi_capture_control_msg msg;

		if (copy_from_user(&msg, ptr, sizeof(msg)))
			break;
		err = vi_capture_control_message(chan, &msg);
		if (err < 0)
			dev_err(chan->dev, "vi capture set config failed\n");
		break;
	}

	case _IOC_NR(VI_CAPTURE_REQUEST): {
		struct vi_capture_req req;
		struct capture_common_unpins *request_unpins;

		if (copy_from_user(&req, ptr, sizeof(req)))
			break;

		if (req.num_relocs == 0) {
			dev_err(chan->dev, "request must have non-zero relocs\n");
			return -EINVAL;
		}

		/* Don't let to speculate with invalid buffer_index value */
		speculation_barrier();

		if (capture->unpins_list == NULL) {
			dev_err(chan->dev, "Channel setup incomplete\n");
			return -EINVAL;
		}

		mutex_lock(&capture->unpins_list_lock);

		request_unpins = &capture->unpins_list[req.buffer_index];

		if (request_unpins->num_unpins != 0U) {
			dev_err(chan->dev, "Descriptor is still in use by rtcpu\n");
			mutex_unlock(&capture->unpins_list_lock);
			return -EBUSY;
		}
		err = pin_vi_capture_request_buffers_locked(chan, &req,
				request_unpins);

		mutex_unlock(&capture->unpins_list_lock);

		if (err < 0) {
			dev_err(chan->dev,
				"pin request failed\n");
			vi_capture_request_unpin(chan, req.buffer_index);
			break;
		}

		err = vi_capture_request(chan, &req);
		if (err < 0) {
			dev_err(chan->dev,
				"vi capture request submit failed\n");
			vi_capture_request_unpin(chan, req.buffer_index);
		}
		break;
	}

	case _IOC_NR(VI_CAPTURE_STATUS): {
		uint32_t timeout_ms;

		if (copy_from_user(&timeout_ms, ptr, sizeof(timeout_ms)))
			break;
		err = vi_capture_status(chan, timeout_ms);
		if (err < 0)
			dev_err(chan->dev,
				"vi capture get status failed\n");
		break;
	}

	case _IOC_NR(VI_CAPTURE_SET_COMPAND): {
		struct vi_capture_compand compand;

		if (copy_from_user(&compand, ptr, sizeof(compand)))
			break;
		err = vi_capture_set_compand(chan, &compand);
		if (err < 0)
			dev_err(chan->dev,
				"setting compand failed\n");
		break;
	}

	case _IOC_NR(VI_CAPTURE_SET_PROGRESS_STATUS_NOTIFIER): {
		struct vi_capture_progress_status_req req;

		if (copy_from_user(&req, ptr, sizeof(req)))
			break;
		err = vi_capture_set_progress_status_notifier(chan, &req);
		if (err < 0)
			dev_err(chan->dev,
					"setting progress status buffer failed\n");
		break;
	}

	case _IOC_NR(VI_CAPTURE_BUFFER_REQUEST): {
		struct vi_buffer_req req;

		if (copy_from_user(&req, ptr, sizeof(req)) != 0U)
			break;

		err = capture_buffer_request(
			capture->buf_ctx, req.mem, req.flag);
		if (err < 0)
			dev_err(chan->dev, "vi buffer request failed\n");
		break;
	}

	default: {
		dev_err(chan->dev, "%s:Unknown ioctl\n", __func__);
		return -ENOIOCTLCMD;
	}
	}

	return err;
}

static struct vi_channel_drv *chdrv_;
static DEFINE_MUTEX(chdrv_lock);

static int vi_channel_power_on_vi_device(struct tegra_vi_channel *chan)
{
	int ret = 0;

	dev_dbg(chan->dev, "vi_channel_power_on_vi_device\n");

	ret = nvhost_module_add_client(chan->ndev, chan->capture_data);
	if (ret < 0) {
		dev_err(chan->dev, "%s: failed to add vi client\n", __func__);
		return ret;
	}

	ret = nvhost_module_busy(chan->ndev);
	if (ret < 0) {
		dev_err(chan->dev, "%s: failed to power on vi\n", __func__);
		return ret;
	}

	return 0;
}

static void vi_channel_power_off_vi_device(struct tegra_vi_channel *chan)
{
	dev_dbg(chan->dev, "vi_channel_power_off_vi_device\n");

	nvhost_module_idle(chan->ndev);
	nvhost_module_remove_client(chan->ndev, chan->capture_data);
}

struct tegra_vi_channel *vi_channel_open_ex(unsigned channel, bool is_mem_pinned)
{
	struct tegra_vi_channel *chan;
	struct vi_channel_drv *chan_drv;
	int err;

	if (mutex_lock_interruptible(&chdrv_lock))
		return ERR_PTR(-ERESTARTSYS);

	chan_drv = chdrv_;

	if (chan_drv == NULL || channel >= chan_drv->num_channels) {
		mutex_unlock(&chdrv_lock);
		return ERR_PTR(-ENODEV);
	}
	mutex_unlock(&chdrv_lock);

	chan = kzalloc(sizeof(*chan), GFP_KERNEL);
	if (unlikely(chan == NULL))
		return ERR_PTR(-ENOMEM);

	chan->drv = chan_drv;
	chan->dev = chan_drv->dev;
	chan->ndev = chan_drv->ndev;
	chan->ops = chan_drv->ops;

	err = vi_capture_init(chan, is_mem_pinned);
	if (err < 0)
		goto error;

	err = vi_channel_power_on_vi_device(chan);
	if (err < 0)
		goto error;

	mutex_lock(&chan_drv->lock);
	if (rcu_access_pointer(chan_drv->channels[channel]) != NULL) {
		mutex_unlock(&chan_drv->lock);
		err = -EBUSY;
		goto rcu_err;
	}

	rcu_assign_pointer(chan_drv->channels[channel], chan);
	mutex_unlock(&chan_drv->lock);

	return chan;

rcu_err:
	vi_channel_power_off_vi_device(chan);
	vi_capture_shutdown(chan);
error:
	kfree(chan);
	return ERR_PTR(err);
}
EXPORT_SYMBOL(vi_channel_open_ex);

int vi_channel_close_ex(unsigned channel, struct tegra_vi_channel *chan)
{
	struct vi_channel_drv *chan_drv = chan->drv;

	vi_channel_power_off_vi_device(chan);
	vi_capture_shutdown(chan);

	mutex_lock(&chan_drv->lock);

	WARN_ON(rcu_access_pointer(chan_drv->channels[channel]) != chan);
	RCU_INIT_POINTER(chan_drv->channels[channel], NULL);

	mutex_unlock(&chan_drv->lock);
	kfree_rcu(chan, rcu);

	return 0;
}
EXPORT_SYMBOL(vi_channel_close_ex);

static int vi_channel_open(struct inode *inode, struct file *file)
{
	unsigned channel = iminor(inode);
	struct tegra_vi_channel *chan;

	chan = vi_channel_open_ex(channel, true);
	if (IS_ERR(chan))
		return PTR_ERR(chan);

	file->private_data = chan;

	return nonseekable_open(inode, file);
}

static int vi_channel_release(struct inode *inode, struct file *file)
{
	struct tegra_vi_channel *chan = file->private_data;
	unsigned channel = iminor(inode);

	vi_channel_close_ex(channel, chan);

	return 0;
}


static const struct file_operations vi_channel_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.unlocked_ioctl = vi_channel_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vi_channel_ioctl,
#endif
	.open = vi_channel_open,
	.release = vi_channel_release,
};

/* Character device */
static struct class *vi_channel_class;
static int vi_channel_major;

int vi_channel_drv_register(struct platform_device *ndev,
	const struct vi_channel_drv_ops *ops)
{
	struct vi_channel_drv *chan_drv;
	int err = 0;
	unsigned i;
	struct nvhost_device_data *pdata = platform_get_drvdata(ndev);

	chan_drv = devm_kzalloc(&ndev->dev, sizeof(*chan_drv) +
			(pdata->num_channels) *
			sizeof(struct tegra_vi_channel *),
			GFP_KERNEL);
	if (unlikely(chan_drv == NULL))
		return -ENOMEM;

	chan_drv->dev = &ndev->dev;
	chan_drv->ndev = ndev;
	chan_drv->ops = ops;
	chan_drv->num_channels = pdata->num_channels;
	mutex_init(&chan_drv->lock);

	mutex_lock(&chdrv_lock);
	if (chdrv_ != NULL) {
		mutex_unlock(&chdrv_lock);
		WARN_ON(1);
		err = -EBUSY;
		goto error;
	}
	chdrv_ = chan_drv;
	mutex_unlock(&chdrv_lock);

	for (i = 0; i < chan_drv->num_channels; i++) {
		dev_t devt = MKDEV(vi_channel_major, i);

		device_create(vi_channel_class, chan_drv->dev, devt, NULL,
				"capture-vi-channel%u", i);
	}

	return 0;

error:
	return err;
}
EXPORT_SYMBOL(vi_channel_drv_register);

void vi_channel_drv_unregister(struct device *dev)
{
	struct vi_channel_drv *chan_drv;
	unsigned i;

	mutex_lock(&chdrv_lock);
	chan_drv = chdrv_;
	chdrv_ = NULL;
	WARN_ON(chan_drv->dev != dev);
	mutex_unlock(&chdrv_lock);

	for (i = 0; i < chan_drv->num_channels; i++) {
		dev_t devt = MKDEV(vi_channel_major, i);

		device_destroy(vi_channel_class, devt);
	}

	devm_kfree(chan_drv->dev, chan_drv);
}
EXPORT_SYMBOL(vi_channel_drv_unregister);

static int __init vi_channel_drv_init(void)
{
	vi_channel_class = class_create(THIS_MODULE, "capture-vi-channel");
	if (IS_ERR(vi_channel_class))
		return PTR_ERR(vi_channel_class);

	vi_channel_major = register_chrdev(0, "capture-vi-channel",
						&vi_channel_fops);
	if (vi_channel_major < 0) {
		class_destroy(vi_channel_class);
		return vi_channel_major;
	}

	return 0;
}

static void __exit vi_channel_drv_exit(void)
{
	unregister_chrdev(vi_channel_major, "capture-vi-channel");
	class_destroy(vi_channel_class);
}

subsys_initcall(vi_channel_drv_init);
module_exit(vi_channel_drv_exit);
