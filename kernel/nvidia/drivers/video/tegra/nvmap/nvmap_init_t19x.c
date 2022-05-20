/*
 * drivers/video/tegra/nvmap/nvmap_init_t19x.c
 *
 * Copyright (c) 2016-2021, NVIDIA CORPORATION.  All rights reserved.
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

#define pr_fmt(fmt)	"nvmap: %s() " fmt, __func__

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/nvmap_t19x.h>
#include <linux/kobject.h>
#include <linux/debugfs.h>
#include <linux/sysfs.h>
#include <linux/io.h>

#include "nvmap_priv.h"

bool nvmap_version_t19x;

#define GOS_STR			"tegra_gos"
struct gos_sysfs {
	struct kobj_attribute status_attr;
	struct kobj_attribute cvdevs_attr;
};

struct gosmem_priv {
	struct kobject *kobj;
	struct gos_sysfs gsfs;
	struct device *dev;
	void *cpu_addr;
	void *memremap_addr;
	dma_addr_t dma_addr;
	u32 cvdevs;
	u8 **dev_names;
	bool status;
};
static struct gosmem_priv *gos;
static struct cv_dev_info *cvdev_info;

const struct of_device_id nvmap_of_ids[] = {
	{ .compatible = "nvidia,carveouts" },
	{ .compatible = "nvidia,carveouts-t18x" },
	{ .compatible = "nvidia,carveouts-t19x" },
	{ }
};

int nvmap_register_cvsram_carveout(struct device *dma_dev,
		phys_addr_t base, size_t size, int (*busy)(void),
		int (*idle)(void))
{
	static struct nvmap_platform_carveout cvsram = {
		.name = "cvsram",
		.usage_mask = NVMAP_HEAP_CARVEOUT_CVSRAM,
		.disable_dynamic_dma_map = true,
		.no_cpu_access = true,
	};

	cvsram.pm_ops.busy = busy;
	cvsram.pm_ops.idle = idle;

	if (!base || !size || (base != PAGE_ALIGN(base)) ||
	    (size != PAGE_ALIGN(size)))
		return -EINVAL;
	cvsram.base = base;
	cvsram.size = size;

	cvsram.dma_dev = &cvsram.dev;
	return nvmap_create_carveout(&cvsram);
}
EXPORT_SYMBOL(nvmap_register_cvsram_carveout);

static ssize_t gos_status_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	bool val = gos->status;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
		(val ? "Enabled" : "Disabled"));
}

static ssize_t gos_cvdevs_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	char *str = buf;
	u32 idx;

	for (idx = 0; idx < gos->cvdevs; idx++)
		str += sprintf(str, "%s\n", (gos->dev_names[idx]));

	return(str - buf);
}

static int gos_sysfs_create(void)
{
	struct attribute *attr1;
	struct attribute *attr2;
	int ret;

	gos->kobj = kobject_create_and_add(GOS_STR,
					kernel_kobj);
	if (!gos->kobj) {
		dev_err(gos->dev, "Couldn't create gos kobj\n");
		return -ENOMEM;
	}

	attr1 = &gos->gsfs.status_attr.attr;
	sysfs_attr_init(attr1);
	attr1->name = "status";
	attr1->mode = 0440;
	gos->gsfs.status_attr.show = gos_status_show;
	ret = sysfs_create_file(gos->kobj, attr1);
	if (ret) {
		dev_err(gos->dev, "Couldn't create status node\n");
		goto clean_gos_kobj;
	}

	attr2 = &gos->gsfs.cvdevs_attr.attr;
	sysfs_attr_init(attr2);
	attr2->name = "cvdevs";
	attr2->mode = 0440;
	gos->gsfs.cvdevs_attr.show = gos_cvdevs_show;
	ret = sysfs_create_file(gos->kobj, attr2);
	if (ret) {
		dev_err(gos->dev, "Couldn't create cvdevs node\n");
		sysfs_remove_file(gos->kobj, attr1);
		goto clean_gos_kobj;
	}

	return 0;

clean_gos_kobj:
	kobject_put(gos->kobj);
	gos->kobj = NULL;
	return ret;
}

static void gos_sysfs_remove(void)
{
	sysfs_remove_file(gos->kobj, &gos->gsfs.cvdevs_attr.attr);
	sysfs_remove_file(gos->kobj, &gos->gsfs.status_attr.attr);
	kobject_put(gos->kobj);
	gos->kobj = NULL;
}

#ifdef CONFIG_DEBUG_FS

#define RW_MODE			(0644)
#define RO_MODE			(0444)

static struct dentry *gos_root;

static int get_cpu_addr(struct seq_file *s, void *data)
{
	u64 idx;

	idx = (u64)s->private;

	if (idx > gos->cvdevs)
		return -EINVAL;

	seq_printf(s, "0x%p\n", cvdev_info[idx].cpu_addr);
	return 0;
}

static int gos_cpu_addr_open(struct inode *inode, struct file *file)
{
	return single_open(file, get_cpu_addr, inode->i_private);
}

static const struct file_operations gos_cpu_addr_ops = {
	.open = gos_cpu_addr_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int show_gos_tbl(struct seq_file *s, void *data)
{
	struct sg_table *sgt;
	dma_addr_t dma_addr;
	u64 idx;
	u32 i;

	idx = (u64)s->private;

	if (idx > gos->cvdevs)
		return -EINVAL;

	for (i = 0; i < gos->cvdevs; i++) {
		sgt = cvdev_info[idx].sgt + i;
		dma_addr = sg_dma_address(sgt->sgl);
		seq_printf(s, "gos_table_addr[%s]:0x%llx\n",
			 gos->dev_names[i], dma_addr);
	}
	return 0;
}

static int gos_tbl_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_gos_tbl, inode->i_private);
}

static const struct file_operations gos_tbl_ops = {
	.open = gos_tbl_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int show_sem_values(struct seq_file *s, void *unused)
{
	struct cv_dev_info *dev_info;
	u32 *sem;
	u64 idx;
	u32 i;

	idx = (u64)s->private;

	if (idx > gos->cvdevs)
		return -EINVAL;

	dev_info = &cvdev_info[idx];
	if (!dev_info)
		return -EINVAL;

	for (i = 0; i < NVMAP_MAX_GOS_COUNT; i++) {
		if (!(i % int_sqrt(NVMAP_MAX_GOS_COUNT)))
			seq_puts(s, "\n");
		sem = (u32 *)(dev_info->cpu_addr + i);
		seq_printf(s, "sem[%u]: %-12u", i, *sem);
	}

	return 0;
}

static int sem_val_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_sem_values, inode->i_private);
}

static const struct file_operations gos_sem_val_ops = {
	.open = sem_val_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int gos_debug_init(void)
{
	struct dentry *dir;
	u64 cvdev_idx;
	u8 buff[15];

	snprintf(buff, sizeof(buff), GOS_STR);
	gos_root = debugfs_create_dir(buff, NULL);
	dir = gos_root;
	if (!dir)
		goto err_out;

	for (cvdev_idx = 0; cvdev_idx < gos->cvdevs; cvdev_idx++) {
		dir = debugfs_create_dir(gos->dev_names[cvdev_idx], gos_root);
		if (!dir)
			goto err_out;

		if (!debugfs_create_file("cpu_addr", RO_MODE, dir,
			(void *)cvdev_idx, &gos_cpu_addr_ops))
			goto err_out;

		if (!debugfs_create_file("dma_addrs", RO_MODE, dir,
			(void *)cvdev_idx, &gos_tbl_ops))
			goto err_out;

		if (!debugfs_create_file("semaphore_values", RO_MODE, dir,
			(void *)cvdev_idx, &gos_sem_val_ops))
			goto err_out;
	}

	return 0;
err_out:
	debugfs_remove_recursive(gos_root);
	return -EINVAL;
}

static void gos_debug_exit(void)
{
	debugfs_remove_recursive(gos_root);
}
#else
static int gos_debug_init(void) {return 0; }
static void gos_debug_exit(void) {}
#endif /* End of debug fs */

static u8 *get_dev_name(const u8 *name)
{
	char *str = kstrdup(name, GFP_KERNEL);

	if (strchr(str, '/') != NULL)
		strsep(&str, "/");
	if (strchr(str, '/') != NULL)
		strsep(&str, "/");
	strreplace(str, '@', '\0');
	return str;
}

static int __init nvmap_gosmem_device_init(struct reserved_mem *rmem,
		struct device *dev)
{
	struct of_phandle_args outargs;
	int ret = 0, i, idx, bytes;
	DEFINE_DMA_ATTRS(attrs);
	struct device_node *np;
	struct sg_table *sgt;
	int cvdev_count;
	u8 *dev_name;

	if (!dev)
		return -ENODEV;

	dma_set_attr(DMA_ATTR_ALLOC_EXACT_SIZE, __DMA_ATTR(attrs));

	np = of_find_node_by_phandle(rmem->phandle);
	if (!np) {
		dev_err(dev, "Can't find the node using compatible\n");
		return -ENODEV;
	}

	if (!of_device_is_available(np)) {
		dev_err(dev, "device is disabled\n");
		return -ENODEV;
	}

	gos = kzalloc(sizeof(*gos), GFP_KERNEL);
	if (!gos)
		return -ENOMEM;

	gos->status = true;
	gos->dev = dev;

	gos->cvdevs = of_count_phandle_with_args(np, "cvdevs", NULL);
	if (!gos->cvdevs) {
		dev_err(gos->dev, "No cvdevs to use the gosmem!!\n");
		ret = -EINVAL;
		goto free_gos;
	}
	cvdev_count = gos->cvdevs;

	gos->dev_names = kcalloc(cvdev_count, sizeof(u8 *),
				 GFP_KERNEL);
	if (gos->dev_names == NULL) {
		ret = -ENOMEM;
		goto free_gos;
	}

	gos->cpu_addr = dma_alloc_coherent(dev, cvdev_count * SZ_4K,
				&gos->dma_addr, GFP_KERNEL);
	if (!gos->cpu_addr) {
		dev_err(gos->dev, "Failed to allocate from Gos mem carveout\n");
		ret = -ENOMEM;
		goto free_cvdevs_names;
	}

	gos->memremap_addr = memremap(virt_to_phys(gos->cpu_addr),
			cvdev_count * SZ_4K, MEMREMAP_WB);

	bytes = sizeof(*cvdev_info) * cvdev_count;
	bytes += sizeof(struct sg_table) * cvdev_count * cvdev_count;
	cvdev_info = kzalloc(bytes, GFP_KERNEL);
	if (!cvdev_info) {
		ret = -ENOMEM;
		goto unmap_dma;
	}

	for (idx = 0; idx < cvdev_count; idx++) {
		struct device_node *temp;

		ret = of_parse_phandle_with_args(np, "cvdevs",
			NULL, idx, &outargs);
		if (ret < 0) {
			/* skip empty (null) phandles */
			if (ret == -ENOENT)
				continue;
			else
				goto free_cvdev;
		}
		temp = outargs.np;

		spin_lock_init(&cvdev_info[idx].goslock);
		cvdev_info[idx].np = of_node_get(temp);
		if (!cvdev_info[idx].np)
			continue;

		dev_name = get_dev_name(of_node_full_name(cvdev_info[idx].np));
		gos->dev_names[idx] = dev_name;

		cvdev_info[idx].count = cvdev_count;
		cvdev_info[idx].idx = idx;
		cvdev_info[idx].sgt =
			(struct sg_table *)(cvdev_info + cvdev_count);
		cvdev_info[idx].sgt += idx * cvdev_count;
		cvdev_info[idx].cpu_addr = gos->memremap_addr + idx * SZ_4K;

		for (i = 0; i < cvdev_count; i++) {
			sgt = cvdev_info[idx].sgt + i;

			ret = sg_alloc_table(sgt, 1, GFP_KERNEL);
			if (ret) {
				dev_err(gos->dev, "sg_alloc_table failed:%d\n",
					ret);
				goto free_sgs;
			}
			sg_set_buf(sgt->sgl,
				(gos->memremap_addr + i * SZ_4K), SZ_4K);
		}
	}

	if (gos_sysfs_create()) {
		ret = -EINVAL;
		goto free_sgs;
	}

	if (gos_debug_init()) {
		ret = -EINVAL;
		goto free_sgs;
	}

	return 0;

free_sgs:
	sgt = (struct sg_table *)(cvdev_info + cvdev_count);
	for (i = 0; i < cvdev_count * cvdev_count; i++)
		sg_free_table(sgt++);
free_cvdev:
	for (i = 0; i < cvdev_count; i++)
		of_node_put(cvdev_info[i].np);
	kfree(cvdev_info);
	cvdev_info = NULL;
unmap_dma:
	memunmap(gos->memremap_addr);
	dma_free_coherent(dev, cvdev_count * SZ_4K, gos->cpu_addr,
				gos->dma_addr);
free_cvdevs_names:
	for (idx = 0; idx < cvdev_count; idx++) {
		kfree(gos->dev_names[idx]);
		gos->dev_names[idx] = NULL;
	}
	kfree(gos->dev_names);
	gos->dev_names = NULL;
free_gos:
	kfree(gos);
	gos = NULL;
	return ret;
}

static void nvmap_gosmem_device_release(struct reserved_mem *rmem,
		struct device *dev)
{
	int cvdev_count = gos->cvdevs;
	struct sg_table *sgt;
	int i;

	gos_debug_exit();

	gos_sysfs_remove();

	sgt = (struct sg_table *)(cvdev_info + cvdev_count);
	for (i = 0; i < (cvdev_count * cvdev_count); i++)
		sg_free_table(sgt++);

	for (i = 0; i < cvdev_count; i++)
		of_node_put(cvdev_info[i].np);

	kfree(cvdev_info);
	cvdev_info = NULL;

	memunmap(gos->memremap_addr);
	dma_free_coherent(gos->dev, cvdev_count * SZ_4K, gos->cpu_addr,
			gos->dma_addr);

	for (i = 0; i < cvdev_count; i++) {
		kfree(gos->dev_names[i]);
		gos->dev_names[i] = NULL;
	}
	kfree(gos->dev_names);
	gos->dev_names = NULL;

	kfree(gos);
	gos = NULL;
}

static struct reserved_mem_ops gosmem_rmem_ops = {
	.device_init = nvmap_gosmem_device_init,
	.device_release = nvmap_gosmem_device_release,
};

static int __init nvmap_gosmem_setup(struct reserved_mem *rmem)
{
	rmem->priv = NULL;
	rmem->ops = &gosmem_rmem_ops;

	return 0;
}
RESERVEDMEM_OF_DECLARE(nvmap_gosmem, "nvidia,gosmem", nvmap_gosmem_setup);

struct cv_dev_info *nvmap_fetch_cv_dev_info(struct device *dev);

static int nvmap_gosmem_notifier(struct notifier_block *nb,
		unsigned long event, void *_dev)
{
	struct device *dev = _dev;
	int ents, i;
	struct cv_dev_info *gos_owner;

	if ((event != BUS_NOTIFY_BOUND_DRIVER) &&
		(event != BUS_NOTIFY_UNBIND_DRIVER))
		return NOTIFY_DONE;

	if ((event == BUS_NOTIFY_BOUND_DRIVER) &&
		nvmap_dev && (dev == nvmap_dev->dev_user.parent)) {
		struct of_device_id nvmap_t19x_of_ids[] = {
			{.compatible = "nvidia,carveouts-t19x"},
			{ }
		};

		/*
		 * user space IOCTL and dmabuf ops happen much later in boot
		 * flow. So, setting the version here to ensure all of those
		 * callbacks can safely query the proper version of nvmap
		 */
		if (of_match_node((struct of_device_id *)&nvmap_t19x_of_ids,
				dev->of_node))
			nvmap_version_t19x = 1;
		return NOTIFY_DONE;
	}

	gos_owner = nvmap_fetch_cv_dev_info(dev);
	if (!gos_owner)
		return NOTIFY_DONE;

	for (i = 0; i < gos->cvdevs; i++) {
		DEFINE_DMA_ATTRS(attrs);
		enum dma_data_direction dir;

		dir = DMA_BIDIRECTIONAL;
		if (cvdev_info[i].np != dev->of_node) {
			dma_set_attr(DMA_ATTR_READ_ONLY, __DMA_ATTR(attrs));
			dir = DMA_TO_DEVICE;
		}

		switch (event) {
		case BUS_NOTIFY_BOUND_DRIVER:
			ents = dma_map_sg_attrs(dev, gos_owner->sgt[i].sgl,
					gos_owner->sgt[i].nents, dir, __DMA_ATTR(attrs));
			if (ents != 1) {
				dev_err(gos->dev, "mapping gosmem chunk %d for %s failed\n",
					i, dev_name(dev));
				return NOTIFY_DONE;
			}
			break;
		case BUS_NOTIFY_UNBIND_DRIVER:
			dma_unmap_sg_attrs(dev, gos_owner->sgt[i].sgl,
					gos_owner->sgt[i].nents, dir, __DMA_ATTR(attrs));
			break;
		default:
			return NOTIFY_DONE;
		};
	}
	return NOTIFY_DONE;
}

static struct notifier_block nvmap_gosmem_nb = {
	.notifier_call = nvmap_gosmem_notifier,
};

#ifdef NVMAP_LOADABLE_MODULE
int nvmap_t19x_init(void)
{
	return bus_register_notifier(&platform_bus_type,
			&nvmap_gosmem_nb);
}

void nvmap_t19x_deinit(void)
{
	bus_unregister_notifier(&platform_bus_type,
				&nvmap_gosmem_nb);
}
#else
static int nvmap_t19x_init(void)
{
	return bus_register_notifier(&platform_bus_type,
			&nvmap_gosmem_nb);
}
core_initcall(nvmap_t19x_init);
#endif

struct cv_dev_info *nvmap_fetch_cv_dev_info(struct device *dev)
{
	int i;

	if (!dev || !cvdev_info || !dev->of_node)
		return NULL;

	for (i = 0; i < gos->cvdevs; i++)
		if (cvdev_info[i].np == dev->of_node)
			return &cvdev_info[i];
	return NULL;
}

int nvmap_alloc_gos_slot(struct device *dev,
		u32 *return_index,
		u32 *return_offset,
		u32 **return_address)
{
	u32 offset;
	int i;

	for (i = 0; i < gos->cvdevs; i++) {
		if (cvdev_info[i].np != dev->of_node)
			continue;

		spin_lock(&cvdev_info[i].goslock);

		offset = find_first_zero_bit(cvdev_info[i].gosmap, NVMAP_MAX_GOS_COUNT);
		if (offset < NVMAP_MAX_GOS_COUNT)
			set_bit(offset, cvdev_info[i].gosmap);

		spin_unlock(&cvdev_info[i].goslock);

		if (offset >= NVMAP_MAX_GOS_COUNT)
			continue;

		*return_index = cvdev_info[i].idx;
		*return_offset = offset;
		*return_address = (u32 *)
			(cvdev_info[i].cpu_addr + (offset * sizeof(u32)));

		return 0;
	}

	return -EBUSY;
}

void nvmap_free_gos_slot(u32 index, u32 offset)
{
	if (WARN_ON(index >= gos->cvdevs) ||
		WARN_ON(offset >= NVMAP_MAX_GOS_COUNT))
		return;

	spin_lock(&cvdev_info[index].goslock);

	clear_bit(offset, cvdev_info[index].gosmap);

	spin_unlock(&cvdev_info[index].goslock);
}
