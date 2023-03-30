/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/of_address.h>
#include <linux/dma-buf.h>
#include <soc/tegra/fuse.h>
#include <uapi/linux/tegra-soc-hwpm-uapi.h>

#include <tegra_hwpm_log.h>
#include <tegra_hwpm_io.h>
#include <tegra_hwpm.h>
#include <tegra_hwpm_common.h>

#define LA_CLK_RATE 625000000UL

struct tegra_soc_hwpm_ioctl {
	const char *const name;
	const size_t struct_size;
	int (*handler)(struct tegra_soc_hwpm *, void *);
};

static int device_info_ioctl(struct tegra_soc_hwpm *hwpm,
			     void *ioctl_struct);
static int floorsweep_info_ioctl(struct tegra_soc_hwpm *hwpm,
			     void *ioctl_struct);
static int resource_info_ioctl(struct tegra_soc_hwpm *hwpm,
				void *ioctl_struct);
static int reserve_resource_ioctl(struct tegra_soc_hwpm *hwpm,
				  void *ioctl_struct);
static int alloc_pma_stream_ioctl(struct tegra_soc_hwpm *hwpm,
				  void *ioctl_struct);
static int bind_ioctl(struct tegra_soc_hwpm *hwpm,
		      void *ioctl_struct);
static int query_allowlist_ioctl(struct tegra_soc_hwpm *hwpm,
				 void *ioctl_struct);
static int exec_reg_ops_ioctl(struct tegra_soc_hwpm *hwpm,
			      void *ioctl_struct);
static int update_get_put_ioctl(struct tegra_soc_hwpm *hwpm,
				void *ioctl_struct);

static const struct tegra_soc_hwpm_ioctl ioctls[] = {
	[TEGRA_SOC_HWPM_IOCTL_DEVICE_INFO] = {
		.name			= "device_info",
		.struct_size		= sizeof(struct tegra_soc_hwpm_device_info),
		.handler		= device_info_ioctl,
	},
	[TEGRA_SOC_HWPM_IOCTL_FLOORSWEEP_INFO] = {
		.name			= "floorsweep_info",
		.struct_size		= sizeof(struct tegra_soc_hwpm_ip_floorsweep_info),
		.handler		= floorsweep_info_ioctl,
	},
	[TEGRA_SOC_HWPM_IOCTL_RESOURCE_INFO] = {
		.name			= "resource_info",
		.struct_size		= sizeof(struct tegra_soc_hwpm_resource_info),
		.handler		= resource_info_ioctl,
	},
	[TEGRA_SOC_HWPM_IOCTL_RESERVE_RESOURCE] = {
		.name			= "reserve_resource",
		.struct_size		= sizeof(struct tegra_soc_hwpm_reserve_resource),
		.handler		= reserve_resource_ioctl,
	},
	[TEGRA_SOC_HWPM_IOCTL_ALLOC_PMA_STREAM] = {
		.name			= "alloc_pma_stream",
		.struct_size		= sizeof(struct tegra_soc_hwpm_alloc_pma_stream),
		.handler		= alloc_pma_stream_ioctl,
	},
	[TEGRA_SOC_HWPM_IOCTL_BIND] = {
		.name			= "bind",
		.struct_size		= 0,
		.handler		= bind_ioctl,
	},
	[TEGRA_SOC_HWPM_IOCTL_QUERY_ALLOWLIST] = {
		.name			= "query_allowlist",
		.struct_size		= sizeof(struct tegra_soc_hwpm_query_allowlist),
		.handler		= query_allowlist_ioctl,
	},
	[TEGRA_SOC_HWPM_IOCTL_EXEC_REG_OPS] = {
		.name			= "exec_reg_ops",
		.struct_size		= sizeof(struct tegra_soc_hwpm_exec_reg_ops),
		.handler		= exec_reg_ops_ioctl,
	},
	[TEGRA_SOC_HWPM_IOCTL_UPDATE_GET_PUT] = {
		.name			= "update_get_put",
		.struct_size		= sizeof(struct tegra_soc_hwpm_update_get_put),
		.handler		= update_get_put_ioctl,
	},
};

static int device_info_ioctl(struct tegra_soc_hwpm *hwpm,
			     void *ioctl_struct)
{
	struct tegra_soc_hwpm_device_info *device_info =
		(struct tegra_soc_hwpm_device_info *)ioctl_struct;

	tegra_hwpm_fn(hwpm, " ");

	device_info->chip = hwpm->device_info.chip;
	device_info->chip_revision = hwpm->device_info.chip_revision;
	device_info->revision = hwpm->device_info.revision;
	device_info->platform = hwpm->device_info.platform;

	tegra_hwpm_dbg(hwpm, hwpm_info | hwpm_dbg_device_info,
		"chip id 0x%x", device_info->chip);
	tegra_hwpm_dbg(hwpm, hwpm_info | hwpm_dbg_device_info,
		"chip_revision 0x%x", device_info->chip_revision);
	tegra_hwpm_dbg(hwpm, hwpm_info | hwpm_dbg_device_info,
		"revision 0x%x", device_info->revision);
	tegra_hwpm_dbg(hwpm, hwpm_info | hwpm_dbg_device_info,
		"platform 0x%x", device_info->platform);

	return 0;
}

static int floorsweep_info_ioctl(struct tegra_soc_hwpm *hwpm,
			     void *ioctl_struct)
{
	struct tegra_soc_hwpm_ip_floorsweep_info *fs_info =
		(struct tegra_soc_hwpm_ip_floorsweep_info *)ioctl_struct;

	tegra_hwpm_fn(hwpm, " ");

	if (fs_info->num_queries > TEGRA_SOC_HWPM_IP_QUERIES_MAX) {
		tegra_hwpm_err(hwpm, "Number of queries exceed max limit of %u",
			TEGRA_SOC_HWPM_IP_QUERIES_MAX);
		return -EINVAL;
	}

	return tegra_hwpm_get_floorsweep_info(hwpm, fs_info);
}

static int resource_info_ioctl(struct tegra_soc_hwpm *hwpm,
				void *ioctl_struct)
{
	struct tegra_soc_hwpm_resource_info *rsrc_info =
		(struct tegra_soc_hwpm_resource_info *)ioctl_struct;

	tegra_hwpm_fn(hwpm, " ");

	if (rsrc_info->num_queries > TEGRA_SOC_HWPM_RESOURCE_QUERIES_MAX) {
		tegra_hwpm_err(hwpm, "Number of queries exceed max limit of %u",
			TEGRA_SOC_HWPM_RESOURCE_QUERIES_MAX);
		return -EINVAL;
	}

	return tegra_hwpm_get_resource_info(hwpm, rsrc_info);

}

static int reserve_resource_ioctl(struct tegra_soc_hwpm *hwpm,
				  void *ioctl_struct)
{
	struct tegra_soc_hwpm_reserve_resource *reserve_resource =
			(struct tegra_soc_hwpm_reserve_resource *)ioctl_struct;
	u32 resource = reserve_resource->resource;
	int ret = 0;

	tegra_hwpm_fn(hwpm, " ");

	if (hwpm->bind_completed) {
		tegra_hwpm_err(hwpm, "The RESERVE_RESOURCE IOCTL can only be"
				" called before the BIND IOCTL.");
		return -EPERM;
	}

	if (resource >= TERGA_SOC_HWPM_NUM_RESOURCES) {
		tegra_hwpm_err(hwpm, "Requested resource %d is out of bounds.",
			resource);
		return -EINVAL;
	}

	ret = tegra_hwpm_reserve_resource(hwpm, resource);
	if (ret < 0) {
		tegra_hwpm_err(hwpm, "Failed to reserve resource %d", resource);
	}

	return ret;
}

static int alloc_pma_stream_ioctl(struct tegra_soc_hwpm *hwpm,
				  void *ioctl_struct)
{
	struct tegra_soc_hwpm_alloc_pma_stream *alloc_pma_stream =
			(struct tegra_soc_hwpm_alloc_pma_stream *)ioctl_struct;
	int ret = 0;

	tegra_hwpm_fn(hwpm, " ");

	if (hwpm->bind_completed) {
		tegra_hwpm_err(hwpm, "The ALLOC_PMA_STREAM IOCTL can only be"
				" called before the BIND IOCTL.");
		return -EPERM;
	}

	if (alloc_pma_stream->stream_buf_size == 0) {
		tegra_hwpm_err(hwpm, "stream_buf_size is 0");
		return -EINVAL;
	}
	if (alloc_pma_stream->stream_buf_fd == 0) {
		tegra_hwpm_err(hwpm, "Invalid stream_buf_fd");
		return -EINVAL;
	}
	if (alloc_pma_stream->mem_bytes_buf_fd == 0) {
		tegra_hwpm_err(hwpm, "Invalid mem_bytes_buf_fd");
		return -EINVAL;
	}

	ret = tegra_hwpm_map_stream_buffer(hwpm, alloc_pma_stream);
	if (ret < 0) {
		tegra_hwpm_err(hwpm, "Failed to map stream buffer");
	}

	return ret;
}

static int bind_ioctl(struct tegra_soc_hwpm *hwpm,
		      void *ioctl_struct)
{
	int ret = 0;

	tegra_hwpm_fn(hwpm, " ");

	ret = tegra_hwpm_bind_resources(hwpm);
	if (ret != 0) {
		tegra_hwpm_err(hwpm, "Failed to bind resources");
	} else {
		hwpm->bind_completed = true;
	}

	return ret;
}

static int query_allowlist_ioctl(struct tegra_soc_hwpm *hwpm,
				 void *ioctl_struct)
{
	int ret = 0;
	struct tegra_soc_hwpm_query_allowlist *query_allowlist =
		(struct tegra_soc_hwpm_query_allowlist *)ioctl_struct;

	tegra_hwpm_fn(hwpm, " ");

	if (!hwpm->bind_completed) {
		tegra_hwpm_err(hwpm,
			"The QUERY_ALLOWLIST IOCTL can only be called"
			" after the BIND IOCTL.");
		return -EPERM;
	}

	if (query_allowlist->allowlist == NULL) {
		/* Userspace is querying allowlist size only */
		if (hwpm->full_alist_size == 0) {
			/*Full alist size is not computed yet */
			ret = tegra_hwpm_get_allowlist_size(hwpm);
			if (ret != 0) {
				tegra_hwpm_err(hwpm,
					"failed to get alist_size");
				return ret;
			}
		}
		query_allowlist->allowlist_size = hwpm->full_alist_size;
	} else {
		/* Concatenate allowlists and return */
		ret = tegra_hwpm_update_allowlist(hwpm, query_allowlist);
		if (ret != 0) {
			tegra_hwpm_err(hwpm, "Failed to update full alist");
			return ret;
		}
	}
	return 0;
}

static int exec_reg_ops_ioctl(struct tegra_soc_hwpm *hwpm,
			      void *ioctl_struct)
{
	tegra_hwpm_fn(hwpm, " ");

	if (!hwpm->bind_completed) {
		tegra_hwpm_err(hwpm, "The EXEC_REG_OPS IOCTL can only be called"
				   " after the BIND IOCTL.");
		return -EPERM;
	}

	return tegra_hwpm_exec_regops(hwpm,
		(struct tegra_soc_hwpm_exec_reg_ops *)ioctl_struct);
}

static int update_get_put_ioctl(struct tegra_soc_hwpm *hwpm,
				void *ioctl_struct)
{
	struct tegra_soc_hwpm_update_get_put *update_get_put =
		(struct tegra_soc_hwpm_update_get_put *)ioctl_struct;

	tegra_hwpm_fn(hwpm, " ");

	if (!hwpm->bind_completed) {
		tegra_hwpm_err(hwpm,
			"The UPDATE_GET_PUT IOCTL can only be called"
			" after the BIND IOCTL.");
		return -EPERM;
	}
	if (!hwpm->mem_bytes_kernel) {
		tegra_hwpm_err(hwpm,
			"mem_bytes buffer is not mapped in the driver");
		return -ENXIO;
	}

	return tegra_hwpm_update_mem_bytes(hwpm, update_get_put);
}

static long tegra_hwpm_ioctl(struct file *file,
				 unsigned int cmd,
				 unsigned long arg)
{
	int ret = 0;
	enum tegra_soc_hwpm_ioctl_num ioctl_num = _IOC_NR(cmd);
	u32 ioc_dir = _IOC_DIR(cmd);
	u32 arg_size = _IOC_SIZE(cmd);
	struct tegra_soc_hwpm *hwpm = NULL;
	void *arg_copy = NULL;

	if ((_IOC_TYPE(cmd) != TEGRA_SOC_HWPM_IOC_MAGIC) ||
	    (ioctl_num < 0) ||
	    (ioctl_num >= TERGA_SOC_HWPM_NUM_IOCTLS)) {
		tegra_hwpm_err(hwpm, "Unsupported IOCTL call");
		ret = -EINVAL;
		goto end;
	}

	if (!file) {
		tegra_hwpm_err(hwpm, "Invalid file");
		ret = -ENODEV;
		goto fail;
	}

	if (arg_size != ioctls[ioctl_num].struct_size) {
		tegra_hwpm_err(hwpm, "Invalid userspace struct");
		ret = -EINVAL;
		goto fail;
	}

	hwpm = file->private_data;
	if (!hwpm) {
		tegra_hwpm_err(hwpm, "Invalid hwpm struct");
		ret = -ENODEV;
		goto fail;
	}

	tegra_hwpm_fn(hwpm, " ");

	if (!hwpm->device_opened) {
		tegra_hwpm_err(hwpm, "Device open failed, can't process IOCTL");
		ret = -ENODEV;
		goto fail;
	}

	/* Only allocate a buffer if the IOCTL needs a buffer */
	if (!(ioc_dir & _IOC_NONE)) {
		arg_copy = kzalloc(arg_size, GFP_KERNEL);
		if (!arg_copy) {
			tegra_hwpm_err(hwpm,
				"Can't allocate memory for kernel struct");
			ret = -ENOMEM;
			goto fail;
		}
	}

	if (ioc_dir & _IOC_WRITE) {
		if (copy_from_user(arg_copy, (void __user *)arg, arg_size)) {
			tegra_hwpm_err(hwpm,
				"Failed to copy data from userspace"
				" struct into kernel struct");
			ret = -EFAULT;
			goto fail;
		}
	}

	/*
	 * We don't goto fail here because even if the IOCTL fails, we have to
	 * call copy_to_user() to pass back any valid output params to
	 * userspace.
	 */
	ret = ioctls[ioctl_num].handler(hwpm, arg_copy);

	if (ioc_dir & _IOC_READ) {
		if (copy_to_user((void __user *)arg, arg_copy, arg_size)) {
			tegra_hwpm_err(hwpm, "Failed to copy data from kernel"
				" struct into userspace struct");
			ret = -EFAULT;
			goto fail;
		}
	}

	if (ret < 0)
		goto fail;

	tegra_hwpm_dbg(hwpm, hwpm_info, "The %s IOCTL completed successfully!",
			   ioctls[ioctl_num].name);
	goto cleanup;

fail:
	tegra_hwpm_err(hwpm, "The %s IOCTL failed(%d)!",
			   ioctls[ioctl_num].name, ret);
cleanup:
	if (arg_copy)
		kfree(arg_copy);
end:
	return ret;
}

static int tegra_hwpm_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	unsigned int minor;
	struct tegra_soc_hwpm *hwpm = NULL;

	if (!inode) {
		tegra_hwpm_err(hwpm, "Invalid inode");
		return -EINVAL;
	}

	if (!filp) {
		tegra_hwpm_err(hwpm, "Invalid file");
		return -EINVAL;
	}

	minor = iminor(inode);
	if (minor > 0) {
		tegra_hwpm_err(hwpm, "Incorrect minor number");
		return -EBADFD;
	}

	hwpm = container_of(inode->i_cdev, struct tegra_soc_hwpm, cdev);
	if (!hwpm) {
		tegra_hwpm_err(hwpm, "Invalid hwpm struct");
		return -EINVAL;
	}
	filp->private_data = hwpm;

	tegra_hwpm_fn(hwpm, " ");

	/* Initialize driver on first open call only */
	if (!atomic_add_unless(&hwpm->hwpm_in_use, 1U, 1U)) {
		return -EAGAIN;
	}

	if (tegra_platform_is_silicon()) {
		ret = reset_control_assert(hwpm->hwpm_rst);
		if (ret < 0) {
			tegra_hwpm_err(hwpm, "hwpm reset assert failed");
			goto fail;
		}
		ret = reset_control_assert(hwpm->la_rst);
		if (ret < 0) {
			tegra_hwpm_err(hwpm, "la reset assert failed");
			goto fail;
		}
		/* Set required parent for la_clk */
		if (hwpm->la_clk && hwpm->la_parent_clk) {
			ret = clk_set_parent(hwpm->la_clk, hwpm->la_parent_clk);
			if (ret < 0) {
				tegra_hwpm_err(hwpm,
					"la clk set parent failed");
				goto fail;
			}
		}
		/* set la_clk rate to 625 MHZ */
		ret = clk_set_rate(hwpm->la_clk, LA_CLK_RATE);
		if (ret < 0) {
			tegra_hwpm_err(hwpm, "la clock set rate failed");
			goto fail;
		}
		ret = clk_prepare_enable(hwpm->la_clk);
		if (ret < 0) {
			tegra_hwpm_err(hwpm, "la clock enable failed");
			goto fail;
		}
		ret = reset_control_deassert(hwpm->la_rst);
		if (ret < 0) {
			tegra_hwpm_err(hwpm, "la reset deassert failed");
			goto fail;
		}
		ret = reset_control_deassert(hwpm->hwpm_rst);
		if (ret < 0) {
			tegra_hwpm_err(hwpm, "hwpm reset deassert failed");
			goto fail;
		}
	}

	ret = tegra_hwpm_setup_hw(hwpm);
	if (ret < 0) {
		tegra_hwpm_err(hwpm, "Failed to setup hw");
		goto fail;
	}

	ret = tegra_hwpm_setup_sw(hwpm);
	if (ret < 0) {
		tegra_hwpm_err(hwpm, "Failed to setup sw");
		goto fail;
	}

	hwpm->device_opened = true;

	return 0;
fail:
	ret = tegra_hwpm_release_hw(hwpm);
	if (ret < 0) {
		tegra_hwpm_err(hwpm, "Failed to release hw");
	}

	tegra_hwpm_err(hwpm, "%s failed", __func__);
	return ret;
}

static ssize_t tegra_hwpm_read(struct file *file,
				   char __user *ubuf,
				   size_t count,
				   loff_t *offp)
{
	return 0;
}

/* FIXME: Fix double release bug */
static int tegra_hwpm_release(struct inode *inode, struct file *filp)
{
	int ret = 0, err = 0;
	struct tegra_soc_hwpm *hwpm = NULL;

	if (!inode) {
		tegra_hwpm_err(hwpm, "Invalid inode");
		return -EINVAL;
	}
	if (!filp) {
		tegra_hwpm_err(hwpm, "Invalid file");
		return -EINVAL;
	}

	hwpm = container_of(inode->i_cdev, struct tegra_soc_hwpm, cdev);
	if (!hwpm) {
		tegra_hwpm_err(hwpm, "Invalid hwpm struct");
		return -EINVAL;
	}

	tegra_hwpm_fn(hwpm, " ");

	if (hwpm->device_opened == false) {
		/* Device was not opened, do nothing */
		return 0;
	}

	ret = tegra_hwpm_disable_triggers(hwpm);
	if (ret < 0) {
		tegra_hwpm_err(hwpm, "Failed to disable PMA triggers");
		err = ret;
	}

	/* Disable and release reserved IPs */
	ret = tegra_hwpm_release_resources(hwpm);
	if (ret < 0) {
		tegra_hwpm_err(hwpm, "Failed to release IP apertures");
		err = ret;
		goto fail;
	}

	/* Clear MEM_BYTES pipeline */
	ret = tegra_hwpm_clear_mem_pipeline(hwpm);
	if (ret < 0) {
		tegra_hwpm_err(hwpm, "Failed to clear MEM_BYTES pipeline");
		err = ret;
		goto fail;
	}

	ret = tegra_hwpm_release_hw(hwpm);
	if (ret < 0) {
		tegra_hwpm_err(hwpm, "Failed to release hw");
		err = ret;
		goto fail;
	}

	if (tegra_platform_is_silicon()) {
		ret = reset_control_assert(hwpm->hwpm_rst);
		if (ret < 0) {
			tegra_hwpm_err(hwpm, "hwpm reset assert failed");
			err = ret;
			goto fail;
		}
		ret = reset_control_assert(hwpm->la_rst);
		if (ret < 0) {
			tegra_hwpm_err(hwpm, "la reset assert failed");
			err = ret;
			goto fail;
		}
		clk_disable_unprepare(hwpm->la_clk);
	}

	/* De-init driver on last close call only */
	if (!atomic_dec_and_test(&hwpm->hwpm_in_use)) {
		return 0;
	}

	hwpm->device_opened = false;
fail:
	return err;
}

/* File ops for device node */
const struct file_operations tegra_soc_hwpm_ops = {
	.owner = THIS_MODULE,
	.open = tegra_hwpm_open,
	.read = tegra_hwpm_read,
	.release = tegra_hwpm_release,
	.unlocked_ioctl = tegra_hwpm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tegra_hwpm_ioctl,
#endif
};
