/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
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

/*
 * This is NvSciIpc kernel driver. At present its only use is to support
 * secure buffer sharing use case across processes.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/mod_devicetable.h>
#include <linux/mutex.h>
#include <linux/cred.h>
#include <linux/of.h>

#include <linux/version.h>
#ifdef CONFIG_TEGRA_VIRTUALIZATION
#include <soc/tegra/virt/syscalls.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif
#endif /* CONFIG_TEGRA_VIRTUALIZATION */

#include "nvsciipc.h"

#define NVSCIIPC_VUID_INDEX_SHIFT 0
#define NVSCIIPC_VUID_INDEX_MASK ((1<<16)-1)
#define NVSCIIPC_VUID_TYPE_SHIFT 16
#define NVSCIIPC_VUID_TYPE_MASK  ((1<<4)-1)
#define NVSCIIPC_VUID_VMID_SHIFT 20
#define NVSCIIPC_VUID_VMID_MASK  ((1<<8)-1)
#define NVSCIIPC_VUID_SOCID_SHIFT 28
#define NVSCIIPC_VUID_SOCID_MASK ((1<<4)-1)

/* Use temporarily until the userspace migrates to use new ioctl id */
#define NVSCIIPC_IOCTL_GET_VUID_LEGACY 0xc028c302
#define NVSCIIPC_MAX_EP_NAME_LEGACY    32
struct nvsciipc_get_vuid_legacy {
	char ep_name[NVSCIIPC_MAX_EP_NAME_LEGACY];
	uint64_t vuid;
};

DEFINE_MUTEX(nvsciipc_mutex);

static struct platform_device *nvsciipc_pdev;
static struct nvsciipc *ctx;

NvSciError NvSciIpcEndpointGetAuthToken(NvSciIpcEndpoint handle,
		NvSciIpcEndpointAuthToken *authToken)
{
	return NvSciError_NotImplemented;
}
EXPORT_SYMBOL(NvSciIpcEndpointGetAuthToken);

NvSciError NvSciIpcEndpointGetVuid(NvSciIpcEndpoint handle,
		NvSciIpcEndpointVuid *vuid)
{
	return NvSciError_NotImplemented;
}
EXPORT_SYMBOL(NvSciIpcEndpointGetVuid);

NvSciError NvSciIpcEndpointValidateAuthTokenLinuxCurrent(
		NvSciIpcEndpointAuthToken authToken,
		NvSciIpcEndpointVuid *localUserVuid)
{
	struct fd f;
	struct file *filp;
	int i, ret;
	char node[NVSCIIPC_MAX_EP_NAME+11];

	f = fdget((int)authToken);
	if (!f.file) {
		ERR("invalid auth token\n");
		return NvSciError_BadParameter;
	}
	filp = f.file;

	mutex_lock(&nvsciipc_mutex);
	if (ctx == NULL) {
		mutex_unlock(&nvsciipc_mutex);
		fdput(f);
		ERR("not initialized\n");
		return NvSciError_NotInitialized;
	}

	for (i = 0; i < ctx->num_eps; i++) {
		ret = snprintf(node, sizeof(node), "%s%d",
			ctx->db[i]->dev_name, ctx->db[i]->id);

		if ((ret < 0) || (ret >= sizeof(node)))
			continue;

		if (!strncmp(filp->f_path.dentry->d_name.name, node,
			sizeof(node))) {
			*localUserVuid = ctx->db[i]->vuid;
			break;
		}
	}

	if (i == ctx->num_eps) {
		mutex_unlock(&nvsciipc_mutex);
		fdput(f);
		ERR("wrong auth token passed\n");
		return NvSciError_BadParameter;
	}
	mutex_unlock(&nvsciipc_mutex);

	fdput(f);

	return NvSciError_Success;
}
EXPORT_SYMBOL(NvSciIpcEndpointValidateAuthTokenLinuxCurrent);

NvSciError NvSciIpcEndpointMapVuid(NvSciIpcEndpointVuid localUserVuid,
		NvSciIpcTopoId *peerTopoId, NvSciIpcEndpointVuid *peerUserVuid)
{
	int i;

	mutex_lock(&nvsciipc_mutex);
	if (ctx == NULL) {
		mutex_unlock(&nvsciipc_mutex);
		ERR("not initialized\n");
		return NvSciError_NotInitialized;
	}


	for (i = 0; i < ctx->num_eps; i++) {
		if (ctx->db[i]->vuid == localUserVuid)
			break;
	}

	if (i == ctx->num_eps) {
		mutex_unlock(&nvsciipc_mutex);
		ERR("wrong localUserVuid passed\n");
		return NvSciError_BadParameter;
	}

	mutex_unlock(&nvsciipc_mutex);

	*peerUserVuid = (localUserVuid ^ 1);
	peerTopoId->VmId = ((localUserVuid >> NVSCIIPC_VUID_VMID_SHIFT)
			   & NVSCIIPC_VUID_VMID_MASK);
	peerTopoId->SocId = ((localUserVuid >> NVSCIIPC_VUID_SOCID_SHIFT)
			    & NVSCIIPC_VUID_SOCID_MASK);

	return NvSciError_Success;
}
EXPORT_SYMBOL(NvSciIpcEndpointMapVuid);

static int nvsciipc_dev_open(struct inode *inode, struct file *filp)
{
	struct nvsciipc *ctx = container_of(inode->i_cdev,
			struct nvsciipc, cdev);

	filp->private_data = ctx;

	return 0;
}

static void nvsciipc_free_db(struct nvsciipc *ctx)
{
	int i;

	if (ctx->num_eps != 0) {
		for (i = 0; i < ctx->num_eps; i++)
			kfree(ctx->db[i]);

		kfree(ctx->db);
	}

	ctx->num_eps = 0;
}

static int nvsciipc_dev_release(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;

	return 0;
}

static int nvsciipc_ioctl_get_vuid(struct nvsciipc *ctx, unsigned int cmd,
				   unsigned long arg)
{
	struct nvsciipc_get_vuid get_vuid;
	int i;

	if (copy_from_user(&get_vuid, (void __user *)arg, _IOC_SIZE(cmd))) {
		ERR("%s : copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	if (ctx->num_eps == 0) {
		ERR("need to set endpoint database first\n");
		return -EINVAL;
	}

	for (i = 0; i < ctx->num_eps; i++) {
		if (!strncmp(get_vuid.ep_name, ctx->db[i]->ep_name,
			NVSCIIPC_MAX_EP_NAME)) {
			get_vuid.vuid = ctx->db[i]->vuid;
			break;
		}
	}

	if (i == ctx->num_eps) {
		ERR("wrong endpoint name passed\n");
		return -EINVAL;
	} else if (copy_to_user((void __user *)arg, &get_vuid,
				_IOC_SIZE(cmd))) {
		ERR("%s : copy_to_user failed\n", __func__);
		return -EFAULT;
	}

	return 0;
}

/* To support legacy - max ep name size 32 bytes.
 *  will be removed once userspace is updated to use 64 bytes
 */
static int nvsciipc_ioctl_get_vuid_legacy(struct nvsciipc *ctx, unsigned int cmd,
				   unsigned long arg)
{
	struct nvsciipc_get_vuid_legacy get_vuid;
	int i;

	if (copy_from_user(&get_vuid, (void __user *)arg, _IOC_SIZE(cmd))) {
		ERR("%s : copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	if (ctx->num_eps == 0) {
		ERR("need to set endpoint database first\n");
		return -EINVAL;
	}

	for (i = 0; i < ctx->num_eps; i++) {
		if (!strncmp(get_vuid.ep_name, ctx->db[i]->ep_name,
			NVSCIIPC_MAX_EP_NAME_LEGACY)) {
			get_vuid.vuid = ctx->db[i]->vuid;
			break;
		}
	}

	if (i == ctx->num_eps) {
		ERR("wrong endpoint name passed\n");
		return -EINVAL;
	} else if (copy_to_user((void __user *)arg, &get_vuid,
				_IOC_SIZE(cmd))) {
		ERR("%s : copy_to_user failed\n", __func__);
		return -EFAULT;
	}

	return 0;
}

static int nvsciipc_ioctl_set_db(struct nvsciipc *ctx, unsigned int cmd,
				 unsigned long arg)
{
	struct nvsciipc_db user_db;
	struct nvsciipc_config_entry **entry_ptr;
	int vmid = 0;
	int ret = 0;
	int i;

	if (current_cred()->uid.val != 0) {
		ERR("no permission to set db\n");
		return -EPERM;
	}

	if (ctx->num_eps != 0) {
		INFO("nvsciipc db is set already\n");
		return -EINVAL;
	}

	if (copy_from_user(&user_db, (void __user *)arg, _IOC_SIZE(cmd))) {
		ERR("copying user db failed\n");
		return -EFAULT;
	}

	if (user_db.num_eps <= 0) {
		INFO("invalid value passed for num_eps\n");
		return -EINVAL;
	}

	ctx->num_eps = user_db.num_eps;

	entry_ptr = (struct nvsciipc_config_entry **)
		kzalloc(ctx->num_eps * sizeof(struct nvsciipc_config_entry *),
			GFP_KERNEL);

	if (entry_ptr == NULL) {
		ERR("memory allocation for entry_ptr failed\n");
		ret = -EFAULT;
		goto ptr_error;
	}

	ret = copy_from_user(entry_ptr, (void __user *)user_db.entry,
			ctx->num_eps * sizeof(struct nvsciipc_config_entry *));
	if (ret < 0) {
		ERR("copying entry ptr failed\n");
		ret = -EFAULT;
		goto ptr_error;
	}

	ctx->db = (struct nvsciipc_config_entry **)
		kzalloc(ctx->num_eps * sizeof(struct nvsciipc_config_entry *),
			GFP_KERNEL);

	if (ctx->db == NULL) {
		ERR("memory allocation for ctx->db failed\n");
		ret = -EFAULT;
		goto ptr_error;
	}

	for (i = 0; i < ctx->num_eps; i++) {
		ctx->db[i] = (struct nvsciipc_config_entry *)
			kzalloc(sizeof(struct nvsciipc_config_entry),
				GFP_KERNEL);

		if (ctx->db[i] == NULL) {
			ERR("memory allocation for ctx->db[%d] failed\n", i);
			ret = -EFAULT;
			goto ptr_error;
		}

		ret = copy_from_user(ctx->db[i], (void __user *)entry_ptr[i],
				sizeof(struct nvsciipc_config_entry));
		if (ret < 0) {
			ERR("copying config entry failed\n");
			ret = -EFAULT;
			goto ptr_error;
		}
	}

#ifdef CONFIG_TEGRA_VIRTUALIZATION
	if (is_tegra_hypervisor_mode()) {
		ret = hyp_read_gid(&vmid);
		if (ret != 0) {
			ERR("Failed to read guest id\n");
			goto ptr_error;
		}
	}
#endif

	for (i = 0; i < ctx->num_eps; i++) {
		ctx->db[i]->vuid |= ((vmid & NVSCIIPC_VUID_VMID_MASK)
				    << NVSCIIPC_VUID_VMID_SHIFT);
	}

	kfree(entry_ptr);
	return ret;

ptr_error:
	if (ctx->db != NULL) {
		for (i = 0; i < ctx->num_eps; i++) {
			if (ctx->db[i] != NULL) {
				memset(ctx->db[i], 0, sizeof(struct nvsciipc_config_entry));
				kfree(ctx->db[i]);
			}
		}

		kfree(ctx->db);
		ctx->db = NULL;
	}

	if (entry_ptr != NULL)
		kfree(entry_ptr);

	ctx->num_eps = 0;

	return ret;
}

static long nvsciipc_dev_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	struct nvsciipc *ctx = filp->private_data;
	long ret = 0;

	if (_IOC_TYPE(cmd) != NVSCIIPC_IOCTL_MAGIC) {
		ERR("%s: not a nvsciipc ioctl\n", __func__);
		return -ENOTTY;
	}

	if (_IOC_NR(cmd) > NVSCIIPC_IOCTL_NUMBER_MAX) {
		ERR("%s: wrong nvsciipc ioctl\n", __func__);
		ret = -ENOTTY;
	}

	switch (cmd) {
	case NVSCIIPC_IOCTL_SET_DB:
		mutex_lock(&nvsciipc_mutex);
		ret = nvsciipc_ioctl_set_db(ctx, cmd, arg);
		mutex_unlock(&nvsciipc_mutex);
		break;
	case NVSCIIPC_IOCTL_GET_VUID:
		mutex_lock(&nvsciipc_mutex);
		ret = nvsciipc_ioctl_get_vuid(ctx, cmd, arg);
		mutex_unlock(&nvsciipc_mutex);
		break;
	case NVSCIIPC_IOCTL_GET_VUID_LEGACY:
		mutex_lock(&nvsciipc_mutex);
		ret = nvsciipc_ioctl_get_vuid_legacy(ctx, cmd, arg);
		mutex_unlock(&nvsciipc_mutex);
		break;
	default:
		ERR("unrecognised ioctl cmd: 0x%x\n", cmd);
		ret = -ENOTTY;
		break;
	}

	return ret;
}

static const struct file_operations nvsciipc_fops = {
	.owner          = THIS_MODULE,
	.open           = nvsciipc_dev_open,
	.release        = nvsciipc_dev_release,
	.unlocked_ioctl = nvsciipc_dev_ioctl,
	.llseek         = noop_llseek,
};

static int nvsciipc_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (pdev == NULL) {
		ERR("invalid platform device\n");
		ret = -EINVAL;
		goto error;
	}

	ctx = devm_kzalloc(&pdev->dev, sizeof(struct nvsciipc),	GFP_KERNEL);
	if (ctx == NULL) {
		ERR("devm_kzalloc failed for nvsciipc\n");
		ret = -ENOMEM;
		goto error;
	}

	ctx->dev = &(pdev->dev);
	platform_set_drvdata(pdev, ctx);

	ret = alloc_chrdev_region(&(ctx->dev_t), 0, 1, MODULE_NAME);
	if (ret != 0) {
		ERR("alloc_chrdev_region() failed\n");
		goto error;
	}

	ctx->nvsciipc_class = class_create(THIS_MODULE, MODULE_NAME);
	if (IS_ERR(ctx->nvsciipc_class)) {
		ERR("failed to create class: %ld\n",
		    PTR_ERR(ctx->nvsciipc_class));
		ret = PTR_ERR(ctx->nvsciipc_class);
		goto error;
	}

	ctx->dev_t = MKDEV(MAJOR(ctx->dev_t), 0);
	cdev_init(&ctx->cdev, &nvsciipc_fops);
	ctx->cdev.owner = THIS_MODULE;
	ret = cdev_add(&(ctx->cdev), ctx->dev_t, 1);
	if (ret != 0) {
		ERR("cdev_add() failed\n");
		goto error;
	}

	if (snprintf(ctx->device_name, (MAX_NAME_SIZE - 1), "%s", MODULE_NAME) < 0) {
		pr_err("snprintf() failed\n");
		ret = -ENOMEM;
		goto error;
	}

	ctx->device = device_create(ctx->nvsciipc_class, NULL,
			ctx->dev_t, ctx,
			ctx->device_name);
	if (IS_ERR(ctx->device)) {
		ret = PTR_ERR(ctx->device);
		ERR("device_create() failed\n");
		goto error;
	}
	dev_set_drvdata(ctx->device, ctx);

	INFO("loaded module\n");

	return ret;

error:
	nvsciipc_cleanup(ctx);

	return ret;
}

static void nvsciipc_cleanup(struct nvsciipc *ctx)
{
	if (ctx == NULL)
		return;

	mutex_lock(&nvsciipc_mutex);
	nvsciipc_free_db(ctx);
	mutex_unlock(&nvsciipc_mutex);

	if (ctx->device != NULL) {
		cdev_del(&ctx->cdev);
		device_del(ctx->device);
		ctx->device = NULL;
	}

	if (ctx->nvsciipc_class) {
		class_destroy(ctx->nvsciipc_class);
		ctx->nvsciipc_class = NULL;
	}

	if (ctx->dev_t) {
		unregister_chrdev_region(ctx->dev_t, 1);
		ctx->dev_t = 0;
	}

	ctx = NULL;
}

static int nvsciipc_remove(struct platform_device *pdev)
{
	struct nvsciipc *ctx = NULL;

	if (pdev == NULL)
		goto exit;

	ctx = (struct nvsciipc *)platform_get_drvdata(pdev);
	if (ctx == NULL)
		goto exit;

	nvsciipc_cleanup(ctx);

exit:
	INFO("Unloaded module\n");

	return 0;
}

static struct platform_driver nvsciipc_driver = {
	.probe  = nvsciipc_probe,
	.remove = nvsciipc_remove,
	.driver = {
		.name = MODULE_NAME,
	},
};

static int __init nvsciipc_module_init(void)
{
	int ret;

	if (!(of_machine_is_compatible("nvidia,tegra194") ||
	      of_machine_is_compatible("nvidia,tegra234")))
		return -ENODEV;

	ret = platform_driver_register(&nvsciipc_driver);
	if (ret)
		return ret;

	nvsciipc_pdev = platform_device_register_simple(MODULE_NAME, -1,
							NULL, 0);
	if (IS_ERR(nvsciipc_pdev)) {
		platform_driver_unregister(&nvsciipc_driver);
		return PTR_ERR(nvsciipc_pdev);
	}

	return 0;
}

static void __exit nvsciipc_module_deinit(void)
{
	platform_device_unregister(nvsciipc_pdev);
}

module_init(nvsciipc_module_init);
module_exit(nvsciipc_module_deinit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Nvidia Corporation");
