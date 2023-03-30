/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>

#include <uapi/linux/nvgpu-nvs.h>

#include <nvgpu/nvs.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_init.h>
#include "os_linux.h"

#include <nvs/sched.h>
#include <nvs/domain.h>

#include "ioctl.h"

/*
 * OS-specific layer to hold device node mapping for a domain.
 */
struct nvgpu_nvs_domain_ioctl {
	struct gk20a *g;
	struct nvgpu_nvs_domain *domain;
	struct cdev *cdev;
	struct device *dev;
	struct nvgpu_class *class;
	struct list_head list; /* entry in cdev_lookup_list */
};

/*
 * This lock serializes domain removal and opening of domain device nodes.
 */
static DEFINE_MUTEX(cdev_lookup_mutex);
/*
 * A list of struct nvgpu_nvs_domain_ioctl objects.
 */
static LIST_HEAD(cdev_lookup_list);

/*
 * Priv data for an open domain device file.
 *
 * While a domain device is open, it holds a ref to the domain.
 */
struct nvgpu_nvs_domain_file_private {
	struct gk20a *g;
	struct nvgpu_nvs_domain *domain;
};

static struct nvgpu_nvs_domain_ioctl *nvgpu_nvs_lookup_cdev(dev_t dev)
{
	struct nvgpu_nvs_domain_ioctl *ioctl, *ret = NULL;

	mutex_lock(&cdev_lookup_mutex);

	list_for_each_entry(ioctl, &cdev_lookup_list, list) {
		if (ioctl->cdev->dev == dev) {
			/* put back in nvgpu_nvs_domain_dev_release */
			nvgpu_nvs_domain_get(ioctl->g, ioctl->domain);
			ret = ioctl;
			goto out;
		}
	}

out:
	mutex_unlock(&cdev_lookup_mutex);
	return ret;
}

int nvgpu_nvs_dev_open(struct inode *inode, struct file *filp)
{
	struct nvgpu_cdev *cdev;
	struct gk20a *g;

	cdev = container_of(inode->i_cdev, struct nvgpu_cdev, cdev);
	g = nvgpu_get_gk20a_from_cdev(cdev);

	filp->private_data = g;

	return 0;
}

int nvgpu_nvs_dev_release(struct inode *inode, struct file *filp)
{
	/*
	 * Since the scheduler persists through a close() call, there's nothing
	 * to do on device close (for now).
	 */
	return 0;
}

static int nvgpu_nvs_domain_dev_do_open(struct gk20a *g,
		struct nvgpu_nvs_domain *domain,
		struct file *filp)
{
	struct nvgpu_nvs_domain_file_private *priv;
	int err;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_nvs, "opening domain %s",
			domain->parent->name);

	g = nvgpu_get(g);
	if (!g)
		return -ENODEV;

	priv = nvgpu_kzalloc(g, sizeof(*priv));
	if (!priv) {
		err = -ENOMEM;
		goto put_ref;
	}

	priv->g = g;
	priv->domain = domain;
	filp->private_data = priv;

	return 0;

put_ref:
	nvgpu_put(g);
	return err;
}

static int nvgpu_nvs_domain_dev_open(struct inode *inode, struct file *filp)
{
	struct nvgpu_nvs_domain_ioctl *ioctl;
	struct cdev *cdev = inode->i_cdev;
	struct nvgpu_nvs_domain *domain;
	struct gk20a *g;
	int err;

	ioctl = nvgpu_nvs_lookup_cdev(cdev->dev);
	if (ioctl == NULL) {
		return -ENXIO;
	}

	g = ioctl->g;
	domain = ioctl->domain;

	err = nvgpu_nvs_domain_dev_do_open(g, domain, filp);
	if (err) {
		nvgpu_nvs_domain_put(g, domain);
	}

	return err;

}

static int nvgpu_nvs_domain_dev_release(struct inode *inode, struct file *filp)
{
	struct nvgpu_nvs_domain_file_private *priv = filp->private_data;
	struct nvgpu_nvs_domain *domain;
	struct gk20a *g;

	if (!priv)
		return 0;

	g = priv->g;
	domain = priv->domain;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_nvs, "releasing domain %s",
			domain->parent->name);

	/* this was taken when the file was opened */
	nvgpu_nvs_domain_put(g, domain);

	nvgpu_kfree(g, priv);
	nvgpu_put(g);
	filp->private_data = NULL;

	return 0;
}

static const struct file_operations nvgpu_nvs_domain_ops = {
	.owner   = THIS_MODULE,
	.open    = nvgpu_nvs_domain_dev_open,
	.release = nvgpu_nvs_domain_dev_release,
};

struct nvgpu_nvs_domain *nvgpu_nvs_domain_get_from_file(int fd)
{
	struct nvgpu_nvs_domain_file_private *priv;
	struct nvgpu_nvs_domain *domain;
	struct file *f = fget(fd);

	if (!f)
		return NULL;

	if (f->f_op != &nvgpu_nvs_domain_ops) {
		fput(f);
		return NULL;
	}

	priv = (struct nvgpu_nvs_domain_file_private *)f->private_data;
	domain = priv->domain;

	nvgpu_log(priv->g, gpu_dbg_fn | gpu_dbg_nvs, "domain %s",
			domain->parent->name);
	nvgpu_nvs_domain_get(priv->g, domain);
	fput(f);

	return domain;
}

static int create_domain_dev(struct gk20a *g,
			     struct nvgpu_nvs_domain *domain)
{
	struct device *dev = dev_from_gk20a(g);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct nvs_domain *nvs_domain = domain->parent;
	struct nvgpu_nvs_domain_ioctl *ioctl = domain->ioctl;
	char name[sizeof("nvsched-") + ARRAY_SIZE(nvs_domain->name)];
	struct nvgpu_class *class;
	dev_t devno;
	unsigned int minor;
	int err;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_nvs, " ");

	class = nvgpu_get_v2_user_class(g);
	if (class == NULL) {
		/* MIG? */
		dev_err(dev, "unsupported GPU for scheduling");
		return -ENOSYS;
	}

	minor = nvgpu_allocate_cdev_minor(g);
	devno = MKDEV(MAJOR(l->cdev_region), minor);
	err = register_chrdev_region(devno, 1, dev_name(dev));
	if (err) {
		dev_err(dev, "failed to allocate devno");
		return err;
	}

	(void) snprintf(name, sizeof(name), "nvsched-%s", nvs_domain->name);

	ioctl->g = g;
	ioctl->domain = domain;
	INIT_LIST_HEAD(&ioctl->list);
	ioctl->cdev = cdev_alloc();
	ioctl->cdev->ops = &nvgpu_nvs_domain_ops;
	ioctl->class = class;
	err = nvgpu_create_device(dev, devno, name,
			ioctl->cdev, &ioctl->dev, class);
	if (err) {
		unregister_chrdev_region(devno, 1);
		return err;
	}

	list_add_tail(&ioctl->list, &cdev_lookup_list);

	return 0;
}

static void delete_domain_dev(struct gk20a *g,
			     struct nvgpu_nvs_domain *domain)
{
	struct nvgpu_nvs_domain_ioctl *ioctl = domain->ioctl;
	dev_t dev = ioctl->cdev->dev;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_nvs, " ");
	/*
	 * note: we're under the lookup lock, so no new open would succeed after this.
	 *
	 * nvgpu_nvs_domain_dev_open() might be waiting for the lock now. Open
	 * cdevs remain accessible even after cdev deletion, but we won't get
	 * here until all successfully opened devices have been closed because
	 * they hold domain refs.
	 */
	list_del(&ioctl->list);

	device_destroy(nvgpu_class_get_class(ioctl->class), dev);
	cdev_del(ioctl->cdev);
	unregister_chrdev_region(dev, 1);
}

static int nvgpu_nvs_ioctl_create_domain(
	struct gk20a *g,
	struct nvgpu_nvs_ioctl_create_domain *dom_args)
{
	struct nvgpu_nvs_domain *domain = NULL;
	int err;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_nvs, " ");

	if (dom_args->reserved1 != 0) {
		return -EINVAL;
	}

	if (dom_args->domain_params.reserved1 != 0) {
		return -EINVAL;
	}

	if (dom_args->domain_params.reserved2 != 0) {
		return -EINVAL;
	}

	if (dom_args->domain_params.dom_id != 0) {
		return -EINVAL;
	}

	if (g->scheduler == NULL) {
		return -ENOSYS;
	}

	err = nvgpu_nvs_add_domain(g,
				      dom_args->domain_params.name,
				      dom_args->domain_params.timeslice_ns,
				      dom_args->domain_params.preempt_grace_ns,
				      &domain);
	if (err != 0) {
		return err;
	}

	domain->subscheduler = dom_args->domain_params.subscheduler;

	dom_args->domain_params.dom_id = domain->id;

	domain->ioctl = nvgpu_kzalloc(g, sizeof(*domain->ioctl));
	if (domain->ioctl == NULL) {
		err = -ENOMEM;
		goto del_domain;
	}

	mutex_lock(&cdev_lookup_mutex);
	err = create_domain_dev(g, domain);
	mutex_unlock(&cdev_lookup_mutex);
	if (err != 0) {
		goto free_ioctl;
	}

	return 0;
free_ioctl:
	nvgpu_kfree(g, domain->ioctl);
del_domain:
	nvgpu_nvs_del_domain(g, domain->id);
	return err;
}

static int nvgpu_nvs_ioctl_remove_domain(struct gk20a *g,
	struct nvgpu_nvs_ioctl_remove_domain *args)
{
	struct nvgpu_nvs_domain_ioctl *ioctl;
	struct nvgpu_nvs_domain *domain;
	int ret;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_nvs, " ");

	if (args->reserved1 != 0) {
		return -EINVAL;
	}

	if (g->scheduler == NULL) {
		return -ENOSYS;
	}

	domain = nvgpu_nvs_domain_by_id(g, args->dom_id);
	if (domain == NULL) {
		nvgpu_err(g, "domain %llu does not exist!", args->dom_id);
		return -ENOENT;
	}

	ioctl = domain->ioctl;

	mutex_lock(&cdev_lookup_mutex);

	nvgpu_nvs_domain_put(g, domain);
	ret = nvgpu_nvs_del_domain(g, args->dom_id);

	/* note: the internal default domain lacks ->ioctl */
	if (ret == 0 && ioctl != NULL) {
		delete_domain_dev(g, domain);
		nvgpu_kfree(g, ioctl);
	}

	mutex_unlock(&cdev_lookup_mutex);

	return ret;
}

static int nvgpu_nvs_ioctl_query_domains_locked(
	struct gk20a *g,
	void __user *user_arg,
	struct nvgpu_nvs_ioctl_query_domains *args)
{
	struct nvgpu_nvs_domain *nvgpu_dom;
	struct nvs_domain *nvs_dom;
	u32 index;
	u32 user_capacity = args->nr;
	struct nvgpu_nvs_ioctl_domain *args_domains =
		(void __user *)(uintptr_t)args->domains;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_nvs, " ");

	if (args->reserved0 != 0) {
		return -EINVAL;
	}

	if (args->reserved1 != 0) {
		return -EINVAL;
	}

	if (g->scheduler == NULL) {
		return -ENOSYS;
	}

	/* First call variant: return number of domains. */
	args->nr = nvs_domain_count(g->scheduler->sched);
	if (copy_to_user(user_arg, args, sizeof(*args))) {
		return -EFAULT;
	}
	nvs_dbg(g, "Nr domains: %u", args->nr);

	if (args_domains != NULL) {
		/*
		 * Second call variant: populate the passed array with domain info.
		 */
		index = 0;
		nvs_domain_for_each(g->scheduler->sched, nvs_dom) {
			struct nvgpu_nvs_ioctl_domain dom;
			if (index == user_capacity) {
				break;
			}

			nvgpu_dom = nvs_dom->priv;

			nvs_dbg(g, "Copying dom #%u [%s] (%llu) (%u refs)",
				index, nvs_dom->name, nvgpu_dom->id, nvgpu_dom->ref);

			(void)memset(&dom, 0, sizeof(dom));

			strncpy(dom.name, nvs_dom->name, sizeof(dom.name) - 1);
			dom.timeslice_ns     = nvs_dom->timeslice_ns;
			dom.preempt_grace_ns = nvs_dom->preempt_grace_ns;
			dom.subscheduler     = nvgpu_dom->subscheduler;
			dom.dom_id           = nvgpu_dom->id;

			if (copy_to_user(&args_domains[index],
					 &dom, sizeof(dom))) {
				nvs_dbg(g, "Fault during copy of domain to userspace.");
				return -EFAULT;
			}

			index += 1;
		}
	}

	return 0;
}

static int nvgpu_nvs_ioctl_query_domains(
	struct gk20a *g,
	void __user *user_arg,
	struct nvgpu_nvs_ioctl_query_domains *args)
{
	int err;

	nvgpu_mutex_acquire(&g->sched_mutex);
	err = nvgpu_nvs_ioctl_query_domains_locked(g, user_arg, args);
	nvgpu_mutex_release(&g->sched_mutex);
	return err;
}

long nvgpu_nvs_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	u8 buf[NVGPU_NVS_IOCTL_MAX_ARG_SIZE] = { 0 };
	bool writable = filp->f_mode & FMODE_WRITE;
	struct gk20a *g = filp->private_data;
	int err = 0;

	nvs_dbg(g, "IOC_TYPE: %c", _IOC_TYPE(cmd));
	nvs_dbg(g, "IOC_NR:   %u", _IOC_NR(cmd));
	nvs_dbg(g, "IOC_SIZE: %u", _IOC_SIZE(cmd));

	if ((_IOC_TYPE(cmd) != NVGPU_NVS_IOCTL_MAGIC) ||
	    (_IOC_NR(cmd) == 0) ||
	    (_IOC_NR(cmd) > NVGPU_NVS_IOCTL_LAST) ||
	    (_IOC_SIZE(cmd) > NVGPU_NVS_IOCTL_MAX_ARG_SIZE)) {
		nvs_dbg(g, "-> BAD!!");
		return -EINVAL;
	}

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(buf, (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	err = gk20a_busy(g);
	if (err != 0) {
		return err;
	}

	switch (cmd) {
	case NVGPU_NVS_IOCTL_CREATE_DOMAIN:
	{
		struct nvgpu_nvs_ioctl_create_domain *args =
			(struct nvgpu_nvs_ioctl_create_domain *)buf;

		if (!writable) {
			err = -EPERM;
			goto done;
		}

		err = nvgpu_nvs_ioctl_create_domain(g, args);
		if (err)
			goto done;

		/*
		 * Remove the domain in case of fault when copying back to
		 * userspace to keep this ioctl atomic.
		 */
		if (copy_to_user((void __user *)arg, buf, _IOC_SIZE(cmd))) {
			nvgpu_nvs_del_domain(g, args->domain_params.dom_id);
			err = -EFAULT;
			goto done;
		}

		break;
	}
	case NVGPU_NVS_IOCTL_QUERY_DOMAINS:
	{
		struct nvgpu_nvs_ioctl_query_domains *args =
			(struct nvgpu_nvs_ioctl_query_domains *)buf;

		err = nvgpu_nvs_ioctl_query_domains(g,
						    (void __user *)arg,
						    args);
		break;
	}
	case NVGPU_NVS_IOCTL_REMOVE_DOMAIN:
	{
		struct nvgpu_nvs_ioctl_remove_domain *args =
			(struct nvgpu_nvs_ioctl_remove_domain *)buf;

		if (!writable) {
			err = -EPERM;
			goto done;
		}

		err = nvgpu_nvs_ioctl_remove_domain(g, args);
		break;
	}
	default:
		err = -ENOTTY;
		goto done;
	}

done:
	gk20a_idle(g);
	return err;
}

ssize_t nvgpu_nvs_dev_read(struct file *filp, char __user *buf,
			   size_t size, loff_t *off)
{
	struct gk20a *g = filp->private_data;
	char log_buf[NVS_LOG_BUF_SIZE];
	const char *log_msg;
	s64 timestamp;
	int bytes;

	/*
	 * We need at least NVS_LOG_BUF_SIZE to parse text into from the binary
	 * log format.
	 *
	 * TODO: If size is large enough, return multiple entries in one go.
	 */
	if (size < NVS_LOG_BUF_SIZE) {
		nvgpu_err(g, "Write buf size too small: %zu", size);
		return -EINVAL;
	}

	nvgpu_nvs_get_log(g, &timestamp, &log_msg);
	if (log_msg == NULL) {
		return 0;
	}

	bytes = snprintf(log_buf, NVS_LOG_BUF_SIZE, "[%16lld] %s\n",
			 timestamp, log_msg);

	if (copy_to_user(buf, log_buf, bytes)) {
		return -EFAULT;
	}

	return bytes;
}
