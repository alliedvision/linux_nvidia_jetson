// SPDX-License-Identifier: GPL-2.0

#include <linux/anon_inodes.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/nvhost.h>
#include <linux/slab.h>
#include <uapi/linux/nvhost_ioctl.h>

#include "host1x/host1x.h"

#include "dev.h"
#include "nvhost_syncpt.h"

struct nvhost_syncpt_fd_data {
	struct nvhost_master *master;
	u32 syncpt_id;
};

static int nvhost_syncpt_fd_release(struct inode *inode, struct file *filp)
{
	struct nvhost_syncpt_fd_data *data = filp->private_data;

	nvhost_syncpt_put_ref(&data->master->syncpt, data->syncpt_id);
	kfree(data);

	return 0;
}

static const struct file_operations nvhost_syncpt_fops = {
	.owner = THIS_MODULE,
	.release = nvhost_syncpt_fd_release,
};

int nvhost_syncpt_fd_alloc(
	struct nvhost_master *master,
	struct nvhost_ctrl_alloc_syncpt_args *args)
{
	struct nvhost_syncpt_fd_data *data;
	struct file *file;
	int err, fd;
	u32 syncpt;

	if (args->flags != 0)
		return -EINVAL;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	syncpt = nvhost_get_syncpt_client_managed(master->dev, current->comm);
	if (!syncpt) {
		err = -EBUSY;
		goto free_data;
	}

	data->master = master;
	data->syncpt_id = syncpt;

	err = get_unused_fd_flags(O_RDWR);
	if (err < 0) {
		nvhost_err(&master->dev->dev, "failed to get unused fd");
		goto free_syncpt;
	}
	fd = err;

	file = anon_inode_getfile("nvhost-syncpt", &nvhost_syncpt_fops,
	                          NULL, O_RDWR);
	if (IS_ERR(file)) {
		nvhost_err(&master->dev->dev, "failed to get file");
		err = PTR_ERR(file);
		goto put_fd;
	}

	file->private_data = data;

	fd_install(fd, file);

	args->fd = fd;
	args->syncpt_id = syncpt;

	return 0;

put_fd:
	put_unused_fd(fd);
free_syncpt:
	nvhost_syncpt_put_ref(&master->syncpt, syncpt);
free_data:
	kfree(data);

	return err;
}

/* Caller must put reference on syncpt afterwards */
int nvhost_syncpt_fd_get(int fd, struct nvhost_syncpt *syncpt, u32 *id)
{
	struct nvhost_syncpt_fd_data *data;
	struct file *file = fget(fd);

	if (!file)
		return -EINVAL;

	if (file->f_op != &nvhost_syncpt_fops) {
		fput(file);
		return -EINVAL;
	}

	data = file->private_data;
	nvhost_syncpt_get_ref(syncpt, data->syncpt_id);
	*id = data->syncpt_id;
	fput(file);

	return 0;
}

int nvhost_syncpt_fd_get_ext(int fd, struct platform_device *pdev, u32 *id)
{
	struct nvhost_syncpt *syncpt = &nvhost_get_host(pdev)->syncpt;

	return nvhost_syncpt_fd_get(fd, syncpt, id);
}
EXPORT_SYMBOL(nvhost_syncpt_fd_get_ext);
