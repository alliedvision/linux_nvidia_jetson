/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/uaccess.h>

#include <uapi/linux/nvgpu-nvs.h>

#include <nvgpu/nvs.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_init.h>

#include <nvs/sched.h>
#include <nvs/domain.h>

#include "ioctl.h"

int nvgpu_nvs_dev_open(struct inode *inode, struct file *filp)
{
	struct nvgpu_cdev *cdev;
	struct gk20a *g;
	int err;

	cdev = container_of(inode->i_cdev, struct nvgpu_cdev, cdev);
	g = nvgpu_get_gk20a_from_cdev(cdev);

	err = nvgpu_nvs_open(g);
	if (err != 0) {
		return err;
	}

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

static int nvgpu_nvs_ioctl_create_domain(
	struct gk20a *g,
	struct nvgpu_nvs_ioctl_create_domain *dom_args)
{
	struct nvgpu_nvs_domain *domain = NULL;
	int err;

	err = nvgpu_nvs_add_domain(g,
				      dom_args->domain_params.name,
				      dom_args->domain_params.timeslice_us,
				      dom_args->domain_params.preempt_grace_us,
				      &domain);
	if (err != 0) {
		return err;
	}

	domain->subscheduler = dom_args->domain_params.subscheduler;

	dom_args->domain_params.dom_id = domain->id;

	return 0;
}

static int nvgpu_nvs_ioctl_remove_domain(struct gk20a *g, u32 dom_id)
{
	return nvgpu_nvs_del_domain(g, dom_id);
}

static int nvgpu_nvs_ioctl_query_domains(
	struct gk20a *g,
	void __user *user_arg,
	struct nvgpu_nvs_ioctl_query_domains *args)
{
	struct nvgpu_nvs_domain *nvgpu_dom;
	struct nvs_domain *nvs_dom;
	u32 index;
	struct nvgpu_nvs_ioctl_domain *args_domains = (void __user *)(uintptr_t)args->domains;

	/* First call variant: return number of domains. */
	if (args_domains == NULL) {
		args->nr = nvgpu_nvs_domain_count(g);
		if (copy_to_user(user_arg, args, sizeof(*args))) {
			return -EFAULT;
		}
		nvs_dbg(g, "Nr domains: %u", args->nr);
		return 0;
	}

	/*
	 * Second call variant: populate the passed array with domain info.
	 */
	index = 0;
	nvs_domain_for_each(g->scheduler->sched, nvs_dom) {
		struct nvgpu_nvs_ioctl_domain dom;

		nvgpu_dom = nvs_dom->priv;

		nvs_dbg(g, "Copying dom #%u [%s] (%llu)",
			index, nvs_dom->name, nvgpu_dom->id);

		(void)memset(&dom, 0, sizeof(dom));

		strncpy(dom.name, nvs_dom->name, sizeof(dom.name) - 1);
		dom.timeslice_us     = nvs_dom->timeslice_us;
		dom.preempt_grace_us = nvs_dom->preempt_grace_us;
		dom.subscheduler     = nvgpu_dom->subscheduler;
		dom.dom_id           = nvgpu_dom->id;

		if (copy_to_user(&args_domains[index],
				 &dom, sizeof(dom))) {
			nvs_dbg(g, "Fault during copy of domain to userspace.");
			return -EFAULT;
		}

		index += 1;
	}

	return 0;
}

long nvgpu_nvs_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct gk20a *g = filp->private_data;
	int err = 0;
	u8 buf[NVGPU_NVS_IOCTL_MAX_ARG_SIZE] = { 0 };

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

	gk20a_busy(g);

	switch (cmd) {
	case NVGPU_NVS_IOCTL_CREATE_DOMAIN:
	{
		struct nvgpu_nvs_ioctl_create_domain *args =
			(struct nvgpu_nvs_ioctl_create_domain *)buf;

		err = nvgpu_nvs_ioctl_create_domain(g, args);
		if (err)
			goto done;

		/*
		 * Issue a remove domain IOCTL in case of fault when copying back to
		 * userspace.
		 */
		if (copy_to_user((void __user *)arg, buf, _IOC_SIZE(cmd))) {
			nvgpu_nvs_ioctl_remove_domain(g,
						args->domain_params.dom_id);
			err = -EFAULT;
			goto done;
		}

		break;
	}
	case NVGPU_NVS_IOCTL_QUERY_DOMAINS:
		err = nvgpu_nvs_ioctl_query_domains(g,
						    (void __user *)arg,
						    (void *)buf);
		if (err)
			goto done;

		break;
	case NVGPU_NVS_IOCTL_REMOVE_DOMAIN:
	{
		struct nvgpu_nvs_ioctl_remove_domain *args =
			(struct nvgpu_nvs_ioctl_remove_domain *)buf;

		err = nvgpu_nvs_ioctl_remove_domain(g, args->dom_id);
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
