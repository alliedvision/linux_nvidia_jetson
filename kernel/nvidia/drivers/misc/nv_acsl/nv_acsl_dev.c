// SPDX-License-Identifier: GPL-2.0-only
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  nv_acsl_dev.c -- User-space interface to ACSL
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>

#include "nv_acsl.h"

static void acsl_nvmap_release_entry(struct acsl_nvmap_entry *entry)
{
	struct dma_buf *dmabuf = entry->attach->dmabuf;

	WARN_ON_ONCE(entry->refcnt);
	dma_buf_unmap_attachment(entry->attach, entry->sgt, entry->dma_dir);
	dma_buf_detach(dmabuf, entry->attach);
	dma_buf_put(dmabuf);
	/* yes this is necessary(not redundant) to balance two dma_buf_get*/
	dma_buf_put(dmabuf);
	list_del(&entry->list);
	kfree(entry);
}

status_t acsl_unmap_iova_addr(struct acsl_drv *drv)
{
	struct acsl_nvmap_args_t *map_args = drv->map_args;
	struct device *dev = drv->dev;
	struct acsl_nvmap_entry *entry;
	struct dma_buf *dmabuf = NULL;
	status_t ret = 0;

	if (!map_args) {
		dev_err(dev, "iova address NULL\n");
		ret = -EACCES;
		goto buf_map_fail;
	}

	dmabuf = dma_buf_get(map_args->mem_handle);
	if (IS_ERR_OR_NULL(dmabuf)) {
		dev_err(dev, "failed to get dma buf from fd %d\n",
			map_args->mem_handle);
		ret = -ENOMEM;
		goto buf_map_fail;
	}

	mutex_lock(&drv->map_lock);

	list_for_each_entry(entry, &drv->map_list, list) {
		if (entry->attach->dmabuf != dmabuf)
			continue;

		WARN_ON_ONCE(!entry->refcnt);

		if (--entry->refcnt == 0)
			acsl_nvmap_release_entry(entry);

		break;
	}

	mutex_unlock(&drv->map_lock);
buf_map_fail:
	return ret;
}

status_t acsl_map_iova_addr(struct acsl_drv *drv)
{
	struct acsl_nvmap_args_t *map_args = drv->map_args;
	struct device *dev = drv->dev;
	enum dma_data_direction dma_dir = DMA_BIDIRECTIONAL;
	struct dma_buf *dmabuf = NULL;
	struct dma_buf_attachment *attachment;
	struct acsl_nvmap_entry *entry;
	struct sg_table *sgt;
	status_t ret = 0;

	dmabuf = dma_buf_get(map_args->mem_handle);

	mutex_lock(&drv->map_lock);

	list_for_each_entry(entry, &drv->map_list, list) {
		if (entry->attach->dmabuf != dmabuf)
			continue;
		dma_buf_put(dmabuf);
		map_args->iova_addr = sg_dma_address(entry->sgt->sgl);
		goto ref;
	}

	attachment = dma_buf_attach(dmabuf, dev);
	if (IS_ERR(attachment)) {
		dev_err(dev, "Failed to attach dmabuf\n");
		ret = PTR_ERR(attachment);
		goto err_unlock;
	}

	sgt = dma_buf_map_attachment(attachment, dma_dir);
	if (IS_ERR(sgt)) {
		dev_err(dev, "Failed to get dmabufs sg_table\n");
		ret = PTR_ERR(sgt);
		goto err_detach;
	}

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry) {
		ret = -ENOMEM;
		goto err_unmap;
	}

	map_args->iova_addr = sg_dma_address(sgt->sgl);

	list_add(&entry->list, &drv->map_list);

	entry->dma_dir = dma_dir;
	entry->dmabuf = dmabuf;
	entry->sgt = sgt;
	entry->attach = attachment;
ref:
	entry->refcnt++;

	mutex_unlock(&drv->map_lock);

	return ret;

err_unmap:
	dma_buf_unmap_attachment(attachment, sgt, dma_dir);
err_detach:
	dma_buf_detach(dmabuf, attachment);
err_unlock:
	mutex_unlock(&drv->map_lock);

	return ret;
}

static long acsl_dev_ioctl(struct file *filp,
				unsigned int cmd, unsigned long arg)
{
	struct acsl_drv *drv = filp->private_data;
	struct device *dev = drv->dev;
	void __user *uarg = (void __user *)arg;
	struct acsl_csm_args_t csm_args = {0};
	struct acsl_nvmap_args_t map_args = {0};
	struct acsl_buf_args_t buf_args = {0};
	status_t ret = 0;

	if (_IOC_TYPE(cmd) != NV_ACSL_MAGIC)
		return -EFAULT;

	switch (_IOC_NR(cmd)) {

	case _IOC_NR(ACSL_INIT_CMD):
		if (!access_ok(uarg, sizeof(csm_args)))
			return -EACCES;
		ret = copy_from_user(&csm_args, uarg,
				sizeof(csm_args));
		if (ret || (csm_args.size > MAX_PAYLOAD)) {
			ret = -EACCES;
			break;
		}

		ret = csm_app_init(drv);
		if (ret)
			dev_err(dev, "CSM init failed with ret:%d\n", ret);
		ret = acsl_open(drv, &csm_args);
		if (ret)
			dev_err(dev, "acsl open failed with ret:%d\n", ret);
		break;

	case _IOC_NR(ACSL_DEINIT_CMD):
		ret = acsl_close(drv);
		if (ret)
			dev_err(dev, "acsl close failed with ret:%d\n", ret);

		csm_app_deinit(drv);
		break;

	case _IOC_NR(ACSL_INTF_OPEN_CMD):
		if (!access_ok(uarg, sizeof(csm_args)))
			return -EACCES;
		ret = copy_from_user(&csm_args, uarg,
				sizeof(csm_args));
		if (ret || (csm_args.size > MAX_PAYLOAD)) {
			ret = -EACCES;
			break;
		}

		ret = acsl_intf_open(drv, &csm_args);
		if (ret)
			dev_err(dev, "intf open failed with ret:%d\n", ret);
		break;

	case _IOC_NR(ACSL_INTF_CLOSE_CMD):
		if (!access_ok(uarg, sizeof(csm_args)))
			return -EACCES;
		ret = copy_from_user(&csm_args, uarg,
				sizeof(csm_args));
		if (ret || (csm_args.size > MAX_PAYLOAD)) {
			ret = -EACCES;
			break;
		}

		ret = acsl_intf_close(drv, &csm_args);
		if (ret)
			dev_err(dev, "intf close failed with ret:%d\n", ret);
		break;

	case _IOC_NR(ACSL_MAP_IOVA_CMD):
		if (!access_ok(uarg, sizeof(map_args)))
			return -EACCES;
		ret = copy_from_user(&map_args, uarg,
				sizeof(map_args));
		if (ret) {
			ret = -EACCES;
			break;
		}

		drv->map_args = &map_args;
		ret = acsl_map_iova_addr(drv);
		if (ret) {
			dev_err(dev, "iova map failed with ret:%d\n", ret);
			break;
		}

		ret = copy_to_user(uarg, &map_args,
				sizeof(map_args));
		if (ret)
			ret = -EACCES;
		break;

	case _IOC_NR(ACSL_UNMAP_IOVA_CMD):
		if (!access_ok(uarg, sizeof(map_args)))
			return -EACCES;
		ret = copy_from_user(&map_args, uarg,
				sizeof(map_args));
		if (ret) {
			ret = -EACCES;
			break;
		}
		drv->map_args = &map_args;
		acsl_unmap_iova_addr(drv);
		break;

	case _IOC_NR(ACSL_COMP_OPEN_CMD):
		if (!access_ok(uarg, sizeof(csm_args)))
			return -EACCES;
		ret = copy_from_user(&csm_args, uarg,
				sizeof(csm_args));
		if (ret || (csm_args.size > MAX_PAYLOAD)) {
			ret = -EACCES;
			break;
		}
		ret = acsl_comp_open(drv, &csm_args);
		if (ret)
			dev_err(dev, "comp open failed with ret:%d\n", ret);
		break;

	case _IOC_NR(ACSL_COMP_CLOSE_CMD):
		if (!access_ok(uarg, sizeof(csm_args)))
			return -EACCES;
		ret = copy_from_user(&csm_args, uarg,
				sizeof(csm_args));
		if (ret || (csm_args.size > MAX_PAYLOAD)) {
			ret = -EACCES;
			break;
		}
		ret = acsl_comp_close(drv, &csm_args);
		if (ret)
			dev_err(dev, "comp close failed with ret:%d\n", ret);
		break;

	case _IOC_NR(ACSL_IN_ACQ_BUF_CMD):
		if (!access_ok(uarg, sizeof(buf_args)))
			return -EACCES;
		ret = copy_from_user(&buf_args, uarg,
				sizeof(buf_args));
		if (ret || (buf_args.comp_id >= MAX_COMP)) {
			ret = -EACCES;
			break;
		}

		buf_args.buf_index = acsl_acq_buf(drv, &buf_args, IN_PORT);

		ret = copy_to_user(uarg, &buf_args,
				sizeof(struct acsl_buf_args_t));
		if (ret)
			ret = -EACCES;

		break;
	case _IOC_NR(ACSL_IN_REL_BUF_CMD):
		if (!access_ok(uarg, sizeof(buf_args)))
			return -EACCES;
		ret = copy_from_user(&buf_args, uarg,
				sizeof(buf_args));
		if (ret || (buf_args.comp_id >= MAX_COMP)) {
			ret = -EACCES;
			break;
		}

		buf_args.buf_index = acsl_rel_buf(drv, &buf_args, IN_PORT);

		ret = copy_to_user(uarg, &buf_args,
				sizeof(struct acsl_buf_args_t));
		if (ret)
			ret = -EACCES;
		break;
	case _IOC_NR(ACSL_OUT_ACQ_BUF_CMD):
		if (!access_ok(uarg, sizeof(buf_args)))
			return -EACCES;
		ret = copy_from_user(&buf_args, uarg,
				sizeof(buf_args));
		if (ret || (buf_args.comp_id >= MAX_COMP)) {
			ret = -EACCES;
			break;
		}

		buf_args.buf_index = acsl_acq_buf(drv, &buf_args, OUT_PORT);

		ret = copy_to_user(uarg, &buf_args,
				sizeof(struct acsl_buf_args_t));
		if (ret)
			ret = -EACCES;

		break;

	case _IOC_NR(ACSL_OUT_REL_BUF_CMD):
		if (!access_ok(uarg, sizeof(struct acsl_buf_args_t)))
			return -EACCES;
		ret = copy_from_user(&buf_args, uarg,
				sizeof(buf_args));
		if (ret || (buf_args.comp_id >= MAX_COMP)) {
			ret = -EACCES;
			break;
		}

		buf_args.buf_index = acsl_rel_buf(drv, &buf_args, OUT_PORT);

		ret = copy_to_user(uarg, &buf_args,
				sizeof(struct acsl_buf_args_t));
		if (ret)
			ret = -EACCES;
		break;

	default:
		ret = -EINVAL;
		dev_err(dev, "invalid command\n");
		break;
	}
	return ret;
}

static status_t nv_acsl_dev_open(struct inode *inp, struct file *filep)
{
	struct acsl_drv *drv;

	if (!inp || !filep) {
		pr_err("Invalid acsl inode/file");
		return -EINVAL;
	}

	drv = container_of(inp->i_cdev, struct acsl_drv, cdev);
	if (!drv) {
		pr_err("Invalid acsl_drv struct");
		return -EINVAL;
	}
	filep->private_data = drv;
	return 0;
}

static const struct file_operations acsl_fops = {
	.owner = THIS_MODULE,
	.open = nv_acsl_dev_open,
	.unlocked_ioctl = acsl_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = acsl_dev_ioctl,
#endif
};

static void acsl_ioctl_cleanup(struct acsl_drv *drv)
{
	cdev_del(&drv->cdev);
	device_destroy(drv->class, drv->dev_t);
	mutex_destroy(&drv->map_lock);
	if (drv->class)
		class_destroy(drv->class);
	unregister_chrdev_region(drv->dev_t, 1);
}

static status_t acsl_init(const struct device *dev)
{
	struct acsl_drv *drv = dev_get_drvdata(dev);
	struct device *dev_acsl;
	status_t ret;

	ret = alloc_chrdev_region(&drv->dev_t, 0,
			1, "nv_acsl");
	if (ret < 0)
		goto alloc_chrdev_region;

	drv->major = MAJOR(drv->dev_t);
	cdev_init(&drv->cdev, &acsl_fops);
	drv->cdev.owner = THIS_MODULE;
	drv->cdev.ops = &acsl_fops;

	ret = cdev_add(&drv->cdev, drv->dev_t, 1);
	if (ret < 0)
		goto cdev_add;

	drv->class = class_create(THIS_MODULE, "nv_acsl");
	if (IS_ERR(drv->class)) {
		dev_err(dev, "device class file already in use\n");
		ret = PTR_ERR(drv->class);
		goto class_create;
	}

	dev_acsl = device_create(drv->class, NULL,
				MKDEV(drv->major, 0),
				NULL,  "%s", "nv_acsl");

	if (IS_ERR(dev_acsl)) {
		dev_err(drv->dev, "Failed to create device\n");
		ret = PTR_ERR(dev_acsl);
		goto device_create;
	}

	INIT_LIST_HEAD(&drv->map_list);
	mutex_init(&drv->map_lock);

	return 0;

device_create:
	class_destroy(drv->class);
class_create:
	cdev_del(&drv->cdev);
cdev_add:
	unregister_chrdev_region(drv->dev_t, 1);
alloc_chrdev_region:
	return ret;
}

static void acsl_exit(struct acsl_drv *drv)
{
	acsl_ioctl_cleanup(drv);
}

static status_t acsl_probe(struct platform_device *pdev)
{
	struct acsl_drv *drv;
	status_t ret;

	drv = devm_kzalloc(&pdev->dev, sizeof(struct acsl_drv),
			GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, drv);
	drv->dev = &pdev->dev;

	ret = nvadsp_os_load();
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to load OS.\n");
		goto os_fail;
	}

	if (nvadsp_os_start()) {
		dev_err(&pdev->dev, "Failed to start OS\n");
		goto os_fail;
	}

	ret = acsl_init(&pdev->dev);
	dev_info(&pdev->dev, "%s\n", __func__);

os_fail:
	return ret;
}

static status_t acsl_remove(struct platform_device *pdev)
{
	struct acsl_drv *drv = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	status_t ret;

	acsl_exit(drv);
	ret = nvadsp_os_suspend();
	if (ret < 0)
		dev_err(dev, "Failed to suspend OS.");

	return ret;
}

static const struct of_device_id acsl_of_match[] = {
	{
		.compatible = "nvidia,tegra23x-acsl-audio",
	},
	{},
};

static struct platform_driver acsl_driver = {
	.driver = {
		.name = "acsl_audio",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(acsl_of_match),
	},
	.probe = acsl_probe,
	.remove = acsl_remove,
};

static status_t __init acsl_modinit(void)
{
	return platform_driver_register(&acsl_driver);
}
module_init(acsl_modinit);

static void __exit acsl_modexit(void)
{
	platform_driver_unregister(&acsl_driver);
}
module_exit(acsl_modexit);

MODULE_AUTHOR("Dara Ramesh <dramesh@nvidia.com>");
MODULE_DESCRIPTION("ACSL Host IO control");
MODULE_LICENSE("GPL");
