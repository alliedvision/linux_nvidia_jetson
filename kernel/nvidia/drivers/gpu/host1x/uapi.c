// SPDX-License-Identifier: GPL-2.0-only
/*
 * /dev/host1x syncpoint interface
 *
 * Copyright (c) 2020, NVIDIA Corporation.
 */

#include <linux/anon_inodes.h>
#include <linux/cdev.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/host1x-next.h>
#include <linux/nospec.h>
#include <linux/sync_file.h>

#include "dev.h"
#include "fence.h"
#include "syncpt.h"
#include "uapi.h"

#include <uapi/linux/host1x-next.h>

static int syncpt_file_release(struct inode *inode, struct file *file)
{
	struct host1x_syncpt *sp = file->private_data;

	host1x_syncpt_put(sp);

	return 0;
}

static int syncpt_file_ioctl_info(struct host1x_syncpt *sp, void __user *data)
{
	struct host1x_syncpoint_info args;
	unsigned long copy_err;

	copy_err = copy_from_user(&args, data, sizeof(args));
	if (copy_err)
		return -EFAULT;

	if (args.reserved[0] || args.reserved[1] || args.reserved[2])
		return -EINVAL;

	args.id = sp->id;

	copy_err = copy_to_user(data, &args, sizeof(args));
	if (copy_err)
		return -EFAULT;

	return 0;
}

static int syncpt_file_ioctl_incr(struct host1x_syncpt *sp, void __user *data)
{
	struct host1x_syncpoint_increment args;
	unsigned long copy_err;
	u32 i;

	copy_err = copy_from_user(&args, data, sizeof(args));
	if (copy_err)
		return -EFAULT;

	for (i = 0; i < args.count; i++) {
		host1x_syncpt_incr(sp);
		if (signal_pending(current))
			return -EINTR;
	}

	return 0;
}

static long syncpt_file_ioctl(struct file *file, unsigned int cmd,
			      unsigned long arg)
{
	void __user *data = (void __user *)arg;
	long err;

	switch (cmd) {
	case HOST1X_IOCTL_SYNCPOINT_INFO:
		err = syncpt_file_ioctl_info(file->private_data, data);
		break;

	case HOST1X_IOCTL_SYNCPOINT_INCREMENT:
		err = syncpt_file_ioctl_incr(file->private_data, data);
		break;

	default:
		err = -ENOTTY;
	}

	return err;
}

static const struct file_operations syncpt_file_fops = {
	.owner = THIS_MODULE,
	.release = syncpt_file_release,
	.unlocked_ioctl = syncpt_file_ioctl,
	.compat_ioctl = syncpt_file_ioctl,
};

struct host1x_syncpt *host1x_syncpt_fd_get(int fd)
{
	struct host1x_syncpt *sp;
	struct file *file = fget(fd);

	if (!file)
		return ERR_PTR(-EINVAL);

	if (file->f_op != &syncpt_file_fops) {
		fput(file);
		return ERR_PTR(-EINVAL);
	}

	sp = file->private_data;

	host1x_syncpt_get(sp);

	fput(file);

	return sp;
}
EXPORT_SYMBOL(host1x_syncpt_fd_get);

static int dev_file_open(struct inode *inode, struct file *file)
{
	struct host1x_uapi *uapi =
		container_of(inode->i_cdev, struct host1x_uapi, cdev);

	file->private_data = container_of(uapi, struct host1x, uapi);

	return 0;
}

static int dev_file_ioctl_read_syncpoint(struct host1x *host1x,
					 void __user *data)
{
	struct host1x_read_syncpoint args;
	unsigned long copy_err;

	copy_err = copy_from_user(&args, data, sizeof(args));
	if (copy_err)
		return -EFAULT;

	if (args.id >= host1x_syncpt_nb_pts(host1x))
		return -EINVAL;

	args.id = array_index_nospec(args.id, host1x_syncpt_nb_pts(host1x));
	args.value = host1x_syncpt_read(&host1x->syncpt[args.id]);

	copy_err = copy_to_user(data, &args, sizeof(args));
	if (copy_err)
		return -EFAULT;

	return 0;
}

static int dev_file_ioctl_alloc_syncpoint(struct host1x *host1x,
					  void __user *data)
{
	struct host1x_allocate_syncpoint args;
	struct host1x_syncpt *sp;
	unsigned long copy_err;
	int err;

	copy_err = copy_from_user(&args, data, sizeof(args));
	if (copy_err)
		return -EFAULT;

	if (args.reserved[0] || args.reserved[1] || args.reserved[2])
		return -EINVAL;

	sp = host1x_syncpt_alloc(host1x, HOST1X_SYNCPT_CLIENT_MANAGED,
				 current->comm);
	if (!sp)
		return -EBUSY;

	err = anon_inode_getfd("host1x_syncpt", &syncpt_file_fops, sp,
			       O_CLOEXEC);
	if (err < 0)
		goto free_syncpt;

	args.fd = err;

	copy_err = copy_to_user(data, &args, sizeof(args));
	if (copy_err) {
		err = -EFAULT;
		goto put_fd;
	}

	return 0;

put_fd:
	put_unused_fd(args.fd);
free_syncpt:
	host1x_syncpt_put(sp);

	return err;
}

static int dev_file_ioctl_create_fence(struct host1x *host1x, void __user *data)
{
	struct host1x_create_fence args;
	unsigned long copy_err;
	int fd;

	copy_err = copy_from_user(&args, data, sizeof(args));
	if (copy_err)
		return -EFAULT;

	if (args.reserved[0])
		return -EINVAL;

	if (args.id >= host1x_syncpt_nb_pts(host1x))
		return -EINVAL;

	args.id = array_index_nospec(args.id, host1x_syncpt_nb_pts(host1x));

	fd = host1x_fence_create_fd(&host1x->syncpt[args.id], args.threshold);
	if (fd < 0)
		return fd;

	args.fence_fd = fd;

	copy_err = copy_to_user(data, &args, sizeof(args));
	if (copy_err)
		return -EFAULT;

	return 0;
}

static int dev_file_ioctl_fence_extract(struct host1x *host1x, void __user *data)
{
	struct host1x_fence_extract_fence __user *fences_user_ptr;
	struct dma_fence *fence, **fences;
	struct host1x_fence_extract args;
	struct dma_fence_array *array;
	unsigned int num_fences, i;
	unsigned long copy_err;
	int err;

	copy_err = copy_from_user(&args, data, sizeof(args));
	if (copy_err)
		return -EFAULT;

	fences_user_ptr = u64_to_user_ptr(args.fences_ptr);

	if (args.reserved[0] || args.reserved[1])
		return -EINVAL;

	fence = sync_file_get_fence(args.fence_fd);
	if (!fence)
		return -EINVAL;

	array = to_dma_fence_array(fence);
	if (array) {
		fences = array->fences;
		num_fences = array->num_fences;
	} else {
		fences = &fence;
		num_fences = 1;
	}

	for (i = 0; i < min(num_fences, args.num_fences); i++) {
		struct host1x_fence_extract_fence f;

		err = host1x_fence_extract(fences[i], &f.id, &f.threshold);
		if (err)
			goto put_fence;

		copy_err = copy_to_user(fences_user_ptr + i, &f, sizeof(f));
		if (copy_err) {
			err = -EFAULT;
			goto put_fence;
		}
	}

	args.num_fences = i+1;

	copy_err = copy_to_user(data, &args, sizeof(args));
	if (copy_err) {
		err = -EFAULT;
		goto put_fence;
	}

	return 0;

put_fence:
	dma_fence_put(fence);

	return err;
}

static long dev_file_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	void __user *data = (void __user *)arg;
	long err;

	switch (cmd) {
	case HOST1X_IOCTL_READ_SYNCPOINT:
		err = dev_file_ioctl_read_syncpoint(file->private_data, data);
		break;

	case HOST1X_IOCTL_ALLOCATE_SYNCPOINT:
		err = dev_file_ioctl_alloc_syncpoint(file->private_data, data);
		break;

	case HOST1X_IOCTL_CREATE_FENCE:
		err = dev_file_ioctl_create_fence(file->private_data, data);
		break;

	case HOST1X_IOCTL_FENCE_EXTRACT:
		err = dev_file_ioctl_fence_extract(file->private_data, data);
		break;

	default:
		err = -ENOTTY;
	}

	return err;
}

static const struct file_operations dev_file_fops = {
	.owner = THIS_MODULE,
	.open = dev_file_open,
	.unlocked_ioctl = dev_file_ioctl,
	.compat_ioctl = dev_file_ioctl,
};

int host1x_uapi_init(struct host1x_uapi *uapi, struct host1x *host1x)
{
	int err;
	dev_t dev_num;

	err = alloc_chrdev_region(&dev_num, 0, 1, "host1x");
	if (err)
		return err;

	uapi->class = class_create(THIS_MODULE, "host1x");
	if (IS_ERR(uapi->class)) {
		err = PTR_ERR(uapi->class);
		goto unregister_chrdev_region;
	}

	cdev_init(&uapi->cdev, &dev_file_fops);
	err = cdev_add(&uapi->cdev, dev_num, 1);
	if (err)
		goto destroy_class;

	uapi->dev = device_create(uapi->class, host1x->dev,
				  dev_num, NULL, "host1x");
	if (IS_ERR(uapi->dev)) {
		err = PTR_ERR(uapi->dev);
		goto del_cdev;
	}

	cdev_add(&uapi->cdev, dev_num, 1);

	uapi->dev_num = dev_num;

	return 0;

del_cdev:
	cdev_del(&uapi->cdev);
destroy_class:
	class_destroy(uapi->class);
unregister_chrdev_region:
	unregister_chrdev_region(dev_num, 1);

	return err;
}

void host1x_uapi_deinit(struct host1x_uapi *uapi)
{
	device_destroy(uapi->class, uapi->dev_num);
	cdev_del(&uapi->cdev);
	class_destroy(uapi->class);
	unregister_chrdev_region(uapi->dev_num, 1);
}
