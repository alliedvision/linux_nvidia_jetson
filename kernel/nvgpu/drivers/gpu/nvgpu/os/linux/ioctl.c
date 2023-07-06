/*
 * NVGPU IOCTLs
 *
 * Copyright (c) 2011-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/file.h>
#include <linux/slab.h>

#include <nvgpu/nvgpu_common.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/mig.h>
#include <nvgpu/grmgr.h>
#include <nvgpu/nvgpu_init.h>

#include "ioctl_channel.h"
#include "ioctl_ctrl.h"
#include "ioctl_as.h"
#include "ioctl_tsg.h"
#include "ioctl_dbg.h"
#include "ioctl_prof.h"
#include "power_ops.h"
#include "ioctl_nvs.h"
#include "ioctl.h"
#include "module.h"
#include "os_linux.h"
#include "fecs_trace_linux.h"
#include "platform_gk20a.h"

const struct file_operations gk20a_power_node_ops = {
	.owner = THIS_MODULE,
	.release = gk20a_power_release,
	.open = gk20a_power_open,
	.read = gk20a_power_read,
	.write = gk20a_power_write,
};

const struct file_operations gk20a_channel_ops = {
	.owner = THIS_MODULE,
	.release = gk20a_channel_release,
	.open = gk20a_channel_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gk20a_channel_ioctl,
#endif
	.unlocked_ioctl = gk20a_channel_ioctl,
};

static const struct file_operations gk20a_ctrl_ops = {
	.owner = THIS_MODULE,
	.release = gk20a_ctrl_dev_release,
	.open = gk20a_ctrl_dev_open,
	.unlocked_ioctl = gk20a_ctrl_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gk20a_ctrl_dev_ioctl,
#endif
	.mmap = gk20a_ctrl_dev_mmap,
};

static const struct file_operations gk20a_dbg_ops = {
	.owner = THIS_MODULE,
	.release = gk20a_dbg_gpu_dev_release,
	.open = gk20a_dbg_gpu_dev_open,
	.unlocked_ioctl = gk20a_dbg_gpu_dev_ioctl,
	.poll = gk20a_dbg_gpu_dev_poll,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gk20a_dbg_gpu_dev_ioctl,
#endif
};

const struct file_operations gk20a_as_ops = {
	.owner = THIS_MODULE,
	.release = gk20a_as_dev_release,
	.open = gk20a_as_dev_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gk20a_as_dev_ioctl,
#endif
	.unlocked_ioctl = gk20a_as_dev_ioctl,
};

/*
 * Note: We use a different 'open' to trigger handling of the profiler session.
 * Most of the code is shared between them...  Though, at some point if the
 * code does get too tangled trying to handle each in the same path we can
 * separate them cleanly.
 */
static const struct file_operations gk20a_prof_ops = {
	.owner = THIS_MODULE,
	.release = gk20a_dbg_gpu_dev_release,
	.open = gk20a_prof_gpu_dev_open,
	.unlocked_ioctl = gk20a_dbg_gpu_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gk20a_dbg_gpu_dev_ioctl,
#endif
};

static const struct file_operations gk20a_prof_dev_ops = {
	.owner = THIS_MODULE,
	.release = nvgpu_prof_fops_release,
	.open = nvgpu_prof_dev_fops_open,
	.unlocked_ioctl = nvgpu_prof_fops_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = nvgpu_prof_fops_ioctl,
#endif
};

static const struct file_operations gk20a_prof_ctx_ops = {
	.owner = THIS_MODULE,
	.release = nvgpu_prof_fops_release,
	.open = nvgpu_prof_ctx_fops_open,
	.unlocked_ioctl = nvgpu_prof_fops_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = nvgpu_prof_fops_ioctl,
#endif
};

const struct file_operations gk20a_tsg_ops = {
	.owner = THIS_MODULE,
	.release = nvgpu_ioctl_tsg_dev_release,
	.open = nvgpu_ioctl_tsg_dev_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl = nvgpu_ioctl_tsg_dev_ioctl,
#endif
	.unlocked_ioctl = nvgpu_ioctl_tsg_dev_ioctl,
};

#ifdef CONFIG_NVGPU_FECS_TRACE
static const struct file_operations gk20a_ctxsw_ops = {
	.owner = THIS_MODULE,
	.release = gk20a_ctxsw_dev_release,
	.open = gk20a_ctxsw_dev_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gk20a_ctxsw_dev_ioctl,
#endif
	.unlocked_ioctl = gk20a_ctxsw_dev_ioctl,
	.poll = gk20a_ctxsw_dev_poll,
	.read = gk20a_ctxsw_dev_read,
	.mmap = gk20a_ctxsw_dev_mmap,
};
#endif

static const struct file_operations gk20a_sched_ops = {
	.owner = THIS_MODULE,
	.release = gk20a_sched_dev_release,
	.open = gk20a_sched_dev_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gk20a_sched_dev_ioctl,
#endif
	.unlocked_ioctl = gk20a_sched_dev_ioctl,
	.poll = gk20a_sched_dev_poll,
	.read = gk20a_sched_dev_read,
};

static const struct file_operations nvgpu_nvs_ops = {
	.owner          = THIS_MODULE,
	.release        = nvgpu_nvs_dev_release,
	.open           = nvgpu_nvs_dev_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = nvgpu_nvs_dev_ioctl,
#endif
	.unlocked_ioctl = nvgpu_nvs_dev_ioctl,
	.read           = nvgpu_nvs_dev_read,
};

struct nvgpu_dev_node {
	/* Device node name */
	char name[20];
	/* file operations for device */
	const struct file_operations *fops;
	/* If node should be created for physical instance in MIG mode */
	bool mig_physical_node;
	/* Flag to check if node is used by debugger/profiler. */
	bool tools_node;
};

static const struct nvgpu_dev_node dev_node_list[] = {
	{"power",	&gk20a_power_node_ops,	false,	false   },
	{"as",		&gk20a_as_ops,		false,	false	},
	{"channel",	&gk20a_channel_ops,	false,	false	},
	{"ctrl",	&gk20a_ctrl_ops,	true,	false	},
#if defined(CONFIG_NVGPU_FECS_TRACE)
	{"ctxsw",	&gk20a_ctxsw_ops,	false,	true	},
#endif
	{"dbg",		&gk20a_dbg_ops,		false,	true	},
	{"prof",	&gk20a_prof_ops,	false,	true	},
	{"prof-ctx",	&gk20a_prof_ctx_ops,	false,	true	},
	{"prof-dev",	&gk20a_prof_dev_ops,	false,	true	},
	{"sched",	&gk20a_sched_ops,	false,	false	},
	{"nvsched",	&nvgpu_nvs_ops,		false,	false	},
	{"tsg",		&gk20a_tsg_ops,		false,	false	},
};

static char *nvgpu_devnode(const char *cdev_name)
{
	/* Special case to maintain legacy names */
	if (strcmp(cdev_name, "channel") == 0) {
		return kasprintf(GFP_KERNEL, "nvhost-gpu");
	}

	return kasprintf(GFP_KERNEL, "nvhost-%s-gpu", cdev_name);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0))
static char *nvgpu_pci_devnode(struct device *dev, umode_t *mode)
#else
static char *nvgpu_pci_devnode(const struct device *dev, umode_t *mode)
#endif
{
	/* Special case to maintain legacy names */
	if (strcmp(dev_name(dev), "channel") == 0) {
		return kasprintf(GFP_KERNEL, "nvgpu-pci/card-%s",
			dev_name(dev->parent));
	}

	return kasprintf(GFP_KERNEL, "nvgpu-pci/card-%s-%s",
			dev_name(dev->parent), dev_name(dev));
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0))
static char *nvgpu_devnode_v2(struct device *dev, umode_t *mode)
#else
static char *nvgpu_devnode_v2(const struct device *dev, umode_t *mode)
#endif
{
	return kasprintf(GFP_KERNEL, "nvgpu/igpu0/%s", dev_name(dev));
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0))
static char *nvgpu_pci_devnode_v2(struct device *dev, umode_t *mode)
#else
static char *nvgpu_pci_devnode_v2(const struct device *dev, umode_t *mode)
#endif
{
	return kasprintf(GFP_KERNEL, "nvgpu/dgpu-%s/%s", dev_name(dev->parent),
			dev_name(dev));
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0))
static char *nvgpu_mig_fgpu_devnode(struct device *dev, umode_t *mode)
#else
static char *nvgpu_mig_fgpu_devnode(const struct device *dev, umode_t *mode)
#endif
{
	struct nvgpu_cdev_class_priv_data *priv_data;

	priv_data = dev_get_drvdata(dev);

	if (priv_data->pci) {
		return kasprintf(GFP_KERNEL, "nvgpu/dgpu-%s/fgpu-%u-%u/%s",
				dev_name(dev->parent), priv_data->major_instance_id,
				priv_data->minor_instance_id, dev_name(dev));
	}

	return kasprintf(GFP_KERNEL, "nvgpu/igpu0/fgpu-%u-%u/%s",
				priv_data->major_instance_id,
				priv_data->minor_instance_id, dev_name(dev));
}

int nvgpu_create_device(
	struct device *dev, int devno,
	const char *cdev_name,
	struct cdev *cdev, struct device **out,
	struct nvgpu_class *class)
{
	struct device *subdev;
	int err;
	struct gk20a *g = gk20a_from_dev(dev);
	const char *device_name = NULL;

	nvgpu_log_fn(g, " ");

	err = cdev_add(cdev, devno, 1);
	if (err) {
		dev_err(dev, "failed to add %s cdev\n", cdev_name);
		return err;
	}

	if (class->class->devnode == NULL) {
		device_name = nvgpu_devnode(cdev_name);
	}

	subdev = device_create(class->class, dev, devno,
			class->priv_data ? class->priv_data : NULL,
			device_name ? device_name : cdev_name);
	if (IS_ERR(subdev)) {
		err = PTR_ERR(subdev);
		cdev_del(cdev);
		dev_err(dev, "failed to create %s device for %s\n",
			cdev_name, dev_name(dev));
		return err;
	}

	if (device_name != NULL) {
		kfree(device_name);
	}

	*out = subdev;
	return 0;
}

static int nvgpu_alloc_and_create_device(
	struct device *dev, int devno,
	const char *cdev_name,
	const struct file_operations *ops,
	struct nvgpu_class *class)
{
	struct gk20a *g = gk20a_from_dev(dev);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct nvgpu_cdev *cdev;
	int err;

	cdev = nvgpu_kzalloc(g, sizeof(*cdev));
	if (cdev == NULL) {
		dev_err(dev, "failed to allocate cdev\n");
		err = -ENOMEM;
		goto out;
	}

	cdev_init(&cdev->cdev, ops);
	cdev->cdev.owner = THIS_MODULE;

	err = nvgpu_create_device(dev, devno, cdev_name,
			&cdev->cdev, &cdev->node, class);
	if (err) {
		goto free_cdev;
	}

	cdev->class = class;
	nvgpu_init_list_node(&cdev->list_entry);
	nvgpu_list_add(&cdev->list_entry, &l->cdev_list_head);

	return 0;

free_cdev:
	nvgpu_kfree(g, cdev);
out:
	return err;
}

void gk20a_remove_devices_and_classes(struct gk20a *g, bool power_node)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct nvgpu_cdev *cdev, *n;
	struct nvgpu_class *class, *p;

	nvgpu_list_for_each_entry_safe(cdev, n, &l->cdev_list_head, nvgpu_cdev, list_entry) {
		class = cdev->class;
		if (class->power_node != power_node)
			continue;

		nvgpu_list_del(&cdev->list_entry);
		device_destroy(nvgpu_class_get_class(cdev->class), cdev->cdev.dev);
		cdev_del(&cdev->cdev);

		nvgpu_kfree(g, cdev);
	}

	nvgpu_list_for_each_entry_safe(class, p, &l->class_list_head, nvgpu_class, list_entry) {
		if (class->power_node != power_node)
			continue;

		nvgpu_list_del(&class->list_entry);
		class_destroy(class->class);
		nvgpu_kfree(g, class);
	}
}

void gk20a_power_node_deinit(struct device *dev)
{
	struct gk20a *g = gk20a_from_dev(dev);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);

	gk20a_remove_devices_and_classes(g, true);

	if (l->power_cdev_region) {
		unregister_chrdev_region(l->power_cdev_region, l->power_cdevs);
	}
}

void gk20a_user_nodes_deinit(struct device *dev)
{
	struct gk20a *g = gk20a_from_dev(dev);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);

	gk20a_remove_devices_and_classes(g, false);

	if (l->cdev_region) {
		unregister_chrdev_region(l->cdev_region, l->num_cdevs);
		l->num_cdevs = 0;
	}

	l->dev_nodes_created = false;
}

static struct nvgpu_class *nvgpu_create_class(struct gk20a *g, const char *class_name)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct nvgpu_class *class;

	class = nvgpu_kzalloc(g, sizeof(*class));
	if (class == NULL) {
		return NULL;
	}

	class->class = class_create(THIS_MODULE, class_name);
	if (IS_ERR(class->class)) {
		nvgpu_err(g, "failed to create class");
		nvgpu_kfree(g, class);
		return NULL;
	}

	nvgpu_init_list_node(&class->list_entry);
	nvgpu_list_add_tail(&class->list_entry, &l->class_list_head);

	return class;
}

/*
 * GPU instance information in MIG mode should be fetched from
 * common.grmgr unit. But instance information is populated during GPU
 * poweron and device nodes are enumerated during probe.
 *
 * Handle this temporarily by adding static information of instances
 * where GPU is partitioned into two instances. In long term, this will
 * need to be handled with design changes.
 *
 * This static information should be removed once instance information
 * is fetched from common.grmgr unit.
 */
struct nvgpu_mig_static_info {
	enum nvgpu_mig_gpu_instance_type instance_type;
	u32 major_instance_id;
	u32 minor_instance_id;
};

static int nvgpu_prepare_mig_dev_node_class_list(struct gk20a *g, u32 *num_classes)
{
	u32 class_count = 0U;
	struct nvgpu_class *class;
	u32 i;
	u32 num_instances;
	struct nvgpu_cdev_class_priv_data *priv_data;

	num_instances = g->mig.num_gpu_instances;
	/*
	 * TODO: i=0 need to be added after ctrl node fixup.
	 */
	for (i = 1U; i < num_instances; i++) {
		priv_data = nvgpu_kzalloc(g, sizeof(*priv_data));
		if (priv_data == NULL) {
			return -ENOMEM;
		}

		snprintf(priv_data->class_name, sizeof(priv_data->class_name),
			"nvidia%s-gpu-fgpu%u",
			(g->pci_class != 0U) ? "-pci" : "", i);

		class = nvgpu_create_class(g, priv_data->class_name);
		if (class == NULL) {
			kfree(priv_data);
			return -ENOMEM;
		}

		class_count++;
		class->class->devnode = nvgpu_mig_fgpu_devnode;
		priv_data->major_instance_id = g->mig.gpu_instance[i].gpu_instance_id;
		priv_data->minor_instance_id = g->mig.gpu_instance[i].gr_syspipe.gr_syspipe_id;
		class->instance_type = NVGPU_MIG_TYPE_MIG;

		class->priv_data = priv_data;
		priv_data->local_instance_id = i;
		priv_data->pci = (g->pci_class != 0U);
	}

	*num_classes = class_count;
	return 0;
}

static int nvgpu_prepare_default_dev_node_class_list(struct gk20a *g,
		u32 *num_classes, bool power_node)
{
	struct nvgpu_class *class;
	u32 count = 0U;

	if (g->pci_class != 0U) {
		if (power_node) {
			class = nvgpu_create_class(g, "nvidia-pci-gpu-power");
		} else {
			class = nvgpu_create_class(g, "nvidia-pci-gpu");
		}

		if (class == NULL) {
			return -ENOMEM;
		}
		class->class->devnode = nvgpu_pci_devnode;
		count++;
	} else {
		if (power_node) {
			class = nvgpu_create_class(g, "nvidia-gpu-power");
		} else {
			class = nvgpu_create_class(g, "nvidia-gpu");
		}

		if (class == NULL) {
			return -ENOMEM;
		}
		class->class->devnode = NULL;
		count++;
	}

	if (power_node) {
		class->power_node = true;
	}

	/*
	 * V2 device node names hierarchy.
	 * This hierarchy will replace above hierarchy in second phase.
	 * Both legacy and V2 device node hierarchies will co-exist until then.
	 *
	 * Note: nvgpu_get_v2_user_class() assumes this order in the class list.
	 */
	if (g->pci_class != 0U) {
		if (power_node) {
			class = nvgpu_create_class(g, "nvidia-pci-gpu-v2-power");
		} else {
			class = nvgpu_create_class(g, "nvidia-pci-gpu-v2");
		}

		if (class == NULL) {
			return -ENOMEM;
		}
		class->class->devnode = nvgpu_pci_devnode_v2;
		count++;
	} else {
		if (power_node) {
			class = nvgpu_create_class(g, "nvidia-gpu-v2-power");
		} else {
			class = nvgpu_create_class(g, "nvidia-gpu-v2");
		}

		if (class == NULL) {
			return -ENOMEM;
		}
		class->class->devnode = nvgpu_devnode_v2;
		count++;
	}

	if (power_node) {
		class->power_node = true;
	}

	*num_classes = count;
	return 0;
}

struct nvgpu_class *nvgpu_get_v2_user_class(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct nvgpu_class *class;

	if (nvgpu_grmgr_is_multi_gr_enabled(g)) {
		/* Ambiguous with multiple fractional GPUs */
		return NULL;
	}

	nvgpu_assert(!nvgpu_list_empty(&l->class_list_head));
	/* this must match nvgpu_prepare_default_dev_node_class_list() */
	class = nvgpu_list_last_entry(&l->class_list_head, nvgpu_class, list_entry);
	nvgpu_assert(!class->power_node);
	return class;
}

static int nvgpu_prepare_dev_node_class_list(struct gk20a *g, u32 *num_classes,
		bool power_node)
{
	int err;

	if ((!power_node) && nvgpu_grmgr_is_multi_gr_enabled(g)) {
		err = nvgpu_prepare_mig_dev_node_class_list(g, num_classes);
	} else {
		err = nvgpu_prepare_default_dev_node_class_list(g, num_classes, power_node);
	}

	return err;
}

static bool check_valid_dev_node(struct gk20a *g, struct nvgpu_class *class,
		const struct nvgpu_dev_node *node)
{
	if (nvgpu_grmgr_is_multi_gr_enabled(g)) {
		if ((class->instance_type == NVGPU_MIG_TYPE_PHYSICAL) &&
		    !node->mig_physical_node) {
			return false;
		}
	}

	/* Do not create nodes used by GPU tools if support for debugger
	 * and profilers is disabled.
	 */
	if ((!g->support_gpu_tools) && (node->tools_node)) {
		return false;
	}
	return true;
}

static bool check_valid_class(struct gk20a *g, struct nvgpu_class *class)
{
	if (class->power_node) {
		return false;
	}

	if (nvgpu_grmgr_is_multi_gr_enabled(g)) {
		if ((class->instance_type == NVGPU_MIG_TYPE_PHYSICAL)) {
			return false;
		}
	}

	return true;
}

int gk20a_power_node_init(struct device *dev)
{
	int err;
	dev_t devno;
	struct gk20a *g = gk20a_from_dev(dev);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct nvgpu_class *class;
	u32 total_cdevs;
	u32 num_classes;

	if (!l->cdev_list_init_done) {
		nvgpu_init_list_node(&l->cdev_list_head);
		nvgpu_init_list_node(&l->class_list_head);
		l->cdev_list_init_done = true;
	}

	err = nvgpu_prepare_dev_node_class_list(g, &num_classes, true);
	if (err != 0) {
		return err;
	}

	total_cdevs = num_classes;
	err = alloc_chrdev_region(&devno, 0, total_cdevs, dev_name(dev));

	if (err) {
		dev_err(dev, "failed to allocate devno\n");
		goto fail;
	}

	l->power_cdev_region = devno;
	nvgpu_list_for_each_entry(class, &l->class_list_head, nvgpu_class, list_entry) {
		/*
		 * dev_node_list[0] is the power node to issue
		 * power-on to the GPU.
		 */
		err = nvgpu_alloc_and_create_device(dev, devno++,
			dev_node_list[0].name,
			dev_node_list[0].fops,
			class);
		if (err) {
			goto fail;
		}
	}

	l->power_cdevs = total_cdevs;
	return 0;
fail:
	gk20a_power_node_deinit(dev);
	return err;

}

int gk20a_user_nodes_init(struct device *dev)
{
	int err;
	dev_t devno;
	struct gk20a *g = gk20a_from_dev(dev);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct nvgpu_class *class;
	u32 num_cdevs, total_cdevs;
	u32 num_classes;
	u32 cdev_index;

	if (!l->cdev_list_init_done) {
		nvgpu_init_list_node(&l->cdev_list_head);
		nvgpu_init_list_node(&l->class_list_head);
		l->cdev_list_init_done = true;
	}

	err = nvgpu_prepare_dev_node_class_list(g, &num_classes, false);
	if (err != 0) {
		return err;
	}

	num_cdevs = sizeof(dev_node_list) / sizeof(dev_node_list[0]);
	if (nvgpu_grmgr_is_multi_gr_enabled(g)) {
		/**
		  * As mig physical node needs the ctrl node only.
		  * We need to add total_cdevs + 1 when we enable ctrl node.
		  */
		total_cdevs = (num_cdevs - 1) * (num_classes - 1);
	} else {
		/*
		 * As the power node is already created, we need to
		 * reduced devs by by one.
		 */
		total_cdevs = (num_cdevs - 1) * num_classes;
	}

	err = alloc_chrdev_region(&devno, 0, total_cdevs, dev_name(dev));
	if (err) {
		dev_err(dev, "failed to allocate devno\n");
		goto fail;
	}
	l->cdev_region = devno;
	atomic_set(&l->next_cdev_minor, MINOR(devno) + total_cdevs);

	nvgpu_list_for_each_entry(class, &l->class_list_head, nvgpu_class, list_entry) {
		if (!check_valid_class(g, class)) {
			continue;
		}

		/*
		 * As we created the power node with power class already, the
		 * index is starting from one.
		 */
		for (cdev_index = 1; cdev_index < num_cdevs; cdev_index++) {
			if (!check_valid_dev_node(g, class, &dev_node_list[cdev_index])) {
				continue;
			}

			err = nvgpu_alloc_and_create_device(dev, devno++,
					dev_node_list[cdev_index].name,
					dev_node_list[cdev_index].fops,
					class);
			if (err) {
				goto fail;
			}
		}
	}

	l->num_cdevs = total_cdevs;

	return 0;
fail:
	gk20a_user_nodes_deinit(dev);
	return err;
}

unsigned int nvgpu_allocate_cdev_minor(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	unsigned int next = (unsigned int)atomic_add_return(1, &l->next_cdev_minor);

	WARN_ON(next >= MINOR(UINT_MAX));
	return next;
}

struct gk20a *nvgpu_get_gk20a_from_cdev(struct nvgpu_cdev *cdev)
{
	return get_gk20a(cdev->node->parent);
}

u32 nvgpu_get_gpu_instance_id_from_cdev(struct gk20a *g, struct nvgpu_cdev *cdev)
{
	struct nvgpu_cdev_class_priv_data *priv_data;

	if (nvgpu_grmgr_is_multi_gr_enabled(g)) {
		priv_data = dev_get_drvdata(cdev->node);
		return priv_data->local_instance_id;
	}

	return 0;
}
