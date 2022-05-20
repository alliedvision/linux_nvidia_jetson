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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * tegra-soc-hwpm-ioctl.c:
 * This file adds IOCTL handlers for the Tegra SOC HWPM driver.
 */

#include <soc/tegra/fuse.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/of_address.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/dma-buf.h>
#include <linux/delay.h>
/* FIXME: Is this include needed for struct resource? */
#if 0
#include <linux/ioport.h>
#endif
#include <linux/mm.h>
#include <linux/vmalloc.h>

#include <uapi/linux/tegra-soc-hwpm-uapi.h>

#include <tegra-soc-hwpm.h>
#include <tegra-soc-hwpm-io.h>
#include "tegra-soc-hwpm-log.h"
#include <hal/tegra-soc-hwpm-structures.h>
#include <hal/tegra_soc_hwpm_init.h>

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
static int timer_relation_ioctl(struct tegra_soc_hwpm *hwpm,
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
	[TEGRA_SOC_HWPM_IOCTL_GET_GPU_CPU_TIME_CORRELATION_INFO] = {
		.name			= "timer_relation",
		.struct_size		= sizeof(struct tegra_soc_hwpm_timer_relation),
		.handler		= timer_relation_ioctl,
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

	device_info->chip = tegra_get_chip_id();
	device_info->chip_revision = tegra_get_major_rev();
	device_info->revision = tegra_chip_get_revision();
	device_info->platform = tegra_get_platform();

	tegra_soc_hwpm_dbg("chip id 0x%x", device_info->chip);
	tegra_soc_hwpm_dbg("chip_revision 0x%x", device_info->chip_revision);
	tegra_soc_hwpm_dbg("revision 0x%x", device_info->revision);
	tegra_soc_hwpm_dbg("platform 0x%x", device_info->platform);

	return 0;
}

static int floorsweep_info_ioctl(struct tegra_soc_hwpm *hwpm,
			     void *ioctl_struct)
{
	u32 i = 0U;
	struct tegra_soc_hwpm_ip_floorsweep_info *fs_info =
		(struct tegra_soc_hwpm_ip_floorsweep_info *)ioctl_struct;

	if (fs_info->num_queries > TEGRA_SOC_HWPM_IP_QUERIES_MAX) {
		tegra_soc_hwpm_err("Number of queries exceed max limit of %u",
			TEGRA_SOC_HWPM_IP_QUERIES_MAX);
		return -EINVAL;
	}

	for (i = 0U; i < fs_info->num_queries; i++) {
		if (fs_info->ip_fsinfo[i].ip_type < TERGA_SOC_HWPM_NUM_IPS) {
			fs_info->ip_fsinfo[i].status =
				TEGRA_SOC_HWPM_IP_STATUS_VALID;
			fs_info->ip_fsinfo[i].ip_inst_mask =
				hwpm->ip_fs_info[fs_info->ip_fsinfo[i].ip_type];
		} else {
			fs_info->ip_fsinfo[i].ip_inst_mask = 0ULL;
			fs_info->ip_fsinfo[i].status =
				TEGRA_SOC_HWPM_IP_STATUS_INVALID;
		}
		tegra_soc_hwpm_dbg(
			"Query %d: ip_type %d: ip_status: %d inst_mask 0x%llx",
			i, fs_info->ip_fsinfo[i].ip_type,
			fs_info->ip_fsinfo[i].status,
			fs_info->ip_fsinfo[i].ip_inst_mask);
	}

	return 0;
}

static int timer_relation_ioctl(struct tegra_soc_hwpm *hwpm,
				void *ioctl_struct)
{
/* FIXME: Implement IOCTL */
#if 0
	struct tegra_soc_hwpm_timer_relation *timer_relation =
			(struct tegra_soc_hwpm_timer_relation *)ioctl_struct;
#endif

	tegra_soc_hwpm_err("The GET_GPU_CPU_TIME_CORRELATION_INFO IOCTL is"
			   " currently not implemented");
	return -ENXIO;

}



static int reserve_resource_ioctl(struct tegra_soc_hwpm *hwpm,
				  void *ioctl_struct)
{
	struct tegra_soc_hwpm_reserve_resource *reserve_resource =
			(struct tegra_soc_hwpm_reserve_resource *)ioctl_struct;
	u32 resource = reserve_resource->resource;

	if (hwpm->bind_completed) {
		tegra_soc_hwpm_err("The RESERVE_RESOURCE IOCTL can only be"
				" called before the BIND IOCTL.");
		return -EPERM;
	}

	if (resource >= TERGA_SOC_HWPM_NUM_RESOURCES) {
		tegra_soc_hwpm_err("Requested resource %d is out of bounds.",
			resource);
		return -EINVAL;
	}

	if ((resource < TERGA_SOC_HWPM_NUM_IPS) &&
		(hwpm->ip_fs_info[resource] == 0)) {
		tegra_soc_hwpm_dbg("Requested resource %d unavailable.",
			resource);
		return 0;
	}

	/*
	 * FIXME: Tell IPs which are being profiled to power up IP and
	 * disable power management
	 */
	return tegra_soc_hwpm_reserve_given_resource(hwpm, resource);
}

static int alloc_pma_stream_ioctl(struct tegra_soc_hwpm *hwpm,
				  void *ioctl_struct)
{
	struct tegra_soc_hwpm_alloc_pma_stream *alloc_pma_stream =
			(struct tegra_soc_hwpm_alloc_pma_stream *)ioctl_struct;

	if (hwpm->bind_completed) {
		tegra_soc_hwpm_err("The ALLOC_PMA_STREAM IOCTL can only be"
				" called before the BIND IOCTL.");
		return -EPERM;
	}

	if (alloc_pma_stream->stream_buf_size == 0) {
		tegra_soc_hwpm_err("stream_buf_size is 0");
		return -EINVAL;
	}
	if (alloc_pma_stream->stream_buf_fd == 0) {
		tegra_soc_hwpm_err("Invalid stream_buf_fd");
		return -EINVAL;
	}
	if (alloc_pma_stream->mem_bytes_buf_fd == 0) {
		tegra_soc_hwpm_err("Invalid mem_bytes_buf_fd");
		return -EINVAL;
	}

	return tegra_soc_hwpm_stream_buf_map(hwpm, alloc_pma_stream);
}

static int bind_ioctl(struct tegra_soc_hwpm *hwpm,
		      void *ioctl_struct)
{
	if (tegra_soc_hwpm_bind_resources(hwpm)) {
		tegra_soc_hwpm_err("Failed to bind resources");
		return -EIO;
	}

	hwpm->bind_completed = true;
	return 0;
}

static int query_allowlist_ioctl(struct tegra_soc_hwpm *hwpm,
				 void *ioctl_struct)
{
	int ret = 0;
	struct tegra_soc_hwpm_query_allowlist *query_allowlist =
			(struct tegra_soc_hwpm_query_allowlist *)ioctl_struct;

	if (!hwpm->bind_completed) {
		tegra_soc_hwpm_err("The QUERY_ALLOWLIST IOCTL can only be called"
				   " after the BIND IOCTL.");
		return -EPERM;
	}

	if (query_allowlist->allowlist != NULL) {
		/* Concatenate allowlists and return */
		ret = tegra_soc_hwpm_update_allowlist(hwpm, ioctl_struct);
		return ret;
	}

	 /* Return allowlist_size */
	if (hwpm->full_alist_size >= 0) {
		query_allowlist->allowlist_size = hwpm->full_alist_size;
		return 0;
	}

	hwpm->full_alist_size = 0;
	tegra_soc_hwpm_get_full_allowlist(hwpm);

	query_allowlist->allowlist_size = hwpm->full_alist_size;
	return ret;
}

static int exec_reg_ops_ioctl(struct tegra_soc_hwpm *hwpm,
			      void *ioctl_struct)
{
	int ret = 0;
	struct tegra_soc_hwpm_exec_reg_ops *exec_reg_ops =
			(struct tegra_soc_hwpm_exec_reg_ops *)ioctl_struct;
	struct hwpm_resource_aperture *aperture = NULL;
	int op_idx = 0;
	struct tegra_soc_hwpm_reg_op *reg_op = NULL;
	u64 upadted_pa = 0ULL;

	if (!hwpm->bind_completed) {
		tegra_soc_hwpm_err("The EXEC_REG_OPS IOCTL can only be called"
				   " after the BIND IOCTL.");
		return -EPERM;
	}
	switch (exec_reg_ops->mode) {
	case TEGRA_SOC_HWPM_REG_OP_MODE_FAIL_ON_FIRST:
	case TEGRA_SOC_HWPM_REG_OP_MODE_CONT_ON_ERR:
		break;

	default:
		tegra_soc_hwpm_err("Invalid reg ops mode(%u)",
				   exec_reg_ops->mode);
		return -EINVAL;
	}

	for (op_idx = 0; op_idx < exec_reg_ops->op_count; op_idx++) {
#define REG_OP_FAIL(op_status, msg, ...)				\
	do {								\
		tegra_soc_hwpm_err(msg, ##__VA_ARGS__);			\
		reg_op->status =					\
			TEGRA_SOC_HWPM_REG_OP_STATUS_ ## op_status;	\
		exec_reg_ops->b_all_reg_ops_passed = false;		\
		if (exec_reg_ops->mode ==				\
		    TEGRA_SOC_HWPM_REG_OP_MODE_FAIL_ON_FIRST) {		\
			return -EINVAL;					\
		}							\
	} while (0)

		reg_op = &(exec_reg_ops->ops[op_idx]);
		tegra_soc_hwpm_dbg("reg op: idx(%d), phys(0x%llx), cmd(%u)",
				op_idx, reg_op->phys_addr, reg_op->cmd);

		/* The allowlist check is done here */
		aperture = tegra_soc_hwpm_find_aperture(hwpm, reg_op->phys_addr,
						true, true, &upadted_pa);
		if (!aperture) {
			REG_OP_FAIL(INSUFFICIENT_PERMISSIONS,
				"Invalid register address(0x%llx)",
				reg_op->phys_addr);
			continue;
		}

		switch (reg_op->cmd) {
		case TEGRA_SOC_HWPM_REG_OP_CMD_RD32:
			reg_op->reg_val_lo = ioctl_readl(hwpm,
							aperture,
							upadted_pa);
			reg_op->status = TEGRA_SOC_HWPM_REG_OP_STATUS_SUCCESS;
			break;

		case TEGRA_SOC_HWPM_REG_OP_CMD_RD64:
			reg_op->reg_val_lo = ioctl_readl(hwpm,
							 aperture,
							 upadted_pa);
			reg_op->reg_val_hi = ioctl_readl(hwpm,
							 aperture,
							 upadted_pa + 4);
			reg_op->status = TEGRA_SOC_HWPM_REG_OP_STATUS_SUCCESS;
			break;

		/* Read Modify Write operation */
		case TEGRA_SOC_HWPM_REG_OP_CMD_WR32:
			ret = reg_rmw(hwpm, aperture, aperture->dt_aperture,
				upadted_pa, reg_op->mask_lo,
				reg_op->reg_val_lo, true, aperture->is_ip);
			if (ret < 0) {
				REG_OP_FAIL(WR_FAILED,
					"WR32 REGOP failed for register(0x%llx)",
					upadted_pa);
			} else {
				reg_op->status = TEGRA_SOC_HWPM_REG_OP_STATUS_SUCCESS;
			}
			break;

		/* Read Modify Write operation */
		case TEGRA_SOC_HWPM_REG_OP_CMD_WR64:
			/* Lower 32 bits */
			ret = reg_rmw(hwpm, aperture, aperture->dt_aperture,
				upadted_pa, reg_op->mask_lo,
				reg_op->reg_val_lo, true, aperture->is_ip);
			if (ret < 0) {
				REG_OP_FAIL(WR_FAILED,
					"WR64 REGOP failed for register(0x%llx)",
					upadted_pa);
				continue;
			}

			/* Upper 32 bits */
			ret = reg_rmw(hwpm, aperture, aperture->dt_aperture,
				upadted_pa + 4, reg_op->mask_hi,
				reg_op->reg_val_hi, true, aperture->is_ip);
			if (ret < 0) {
				REG_OP_FAIL(WR_FAILED,
					"WR64 REGOP failed for register(0x%llx)",
					upadted_pa + 4);
			} else {
				reg_op->status = TEGRA_SOC_HWPM_REG_OP_STATUS_SUCCESS;
			}

			break;

		default:
			REG_OP_FAIL(INVALID_CMD,
				    "Invalid reg op command(%u)",
				    reg_op->cmd);
			break;
		}

	}

	exec_reg_ops->b_all_reg_ops_passed = true;
	return 0;
}

static int update_get_put_ioctl(struct tegra_soc_hwpm *hwpm,
				void *ioctl_struct)
{
	struct tegra_soc_hwpm_update_get_put *update_get_put =
			(struct tegra_soc_hwpm_update_get_put *)ioctl_struct;

	if (!hwpm->bind_completed) {
		tegra_soc_hwpm_err("The UPDATE_GET_PUT IOCTL can only be called"
				   " after the BIND IOCTL.");
		return -EPERM;
	}
	if (!hwpm->mem_bytes_kernel) {
		tegra_soc_hwpm_err("mem_bytes buffer is not mapped in the driver");
		return -ENXIO;
	}

	return tegra_soc_hwpm_update_mem_bytes(hwpm, update_get_put);
}

static long tegra_soc_hwpm_ioctl(struct file *file,
				 unsigned int cmd,
				 unsigned long arg)
{
	int ret = 0;
	enum tegra_soc_hwpm_ioctl_num ioctl_num = _IOC_NR(cmd);
	u32 ioc_dir = _IOC_DIR(cmd);
	u32 arg_size = _IOC_SIZE(cmd);
	struct tegra_soc_hwpm *hwpm = NULL;
	void *arg_copy = NULL;

	if (!file) {
		tegra_soc_hwpm_err("Invalid file");
		ret = -ENODEV;
		goto fail;
	}

	if ((_IOC_TYPE(cmd) != TEGRA_SOC_HWPM_IOC_MAGIC) ||
	    (ioctl_num < 0) ||
	    (ioctl_num >= TERGA_SOC_HWPM_NUM_IOCTLS)) {
		tegra_soc_hwpm_err("Unsupported IOCTL call");
		ret = -EINVAL;
		goto fail;
	}
	if (arg_size != ioctls[ioctl_num].struct_size) {
		tegra_soc_hwpm_err("Invalid userspace struct");
		ret = -EINVAL;
		goto fail;
	}

	hwpm = file->private_data;
	if (!hwpm) {
		tegra_soc_hwpm_err("Invalid hwpm struct");
		ret = -ENODEV;
		goto fail;
	}

	/* Only allocate a buffer if the IOCTL needs a buffer */
	if (!(ioc_dir & _IOC_NONE)) {
		arg_copy = kzalloc(arg_size, GFP_KERNEL);
		if (!arg_copy) {
			tegra_soc_hwpm_err("Can't allocate memory for kernel struct");
			ret = -ENOMEM;
			goto fail;
		}
	}

	if (ioc_dir & _IOC_WRITE) {
		if (copy_from_user(arg_copy, (void __user *)arg, arg_size)) {
			tegra_soc_hwpm_err("Failed to copy data from userspace"
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
			tegra_soc_hwpm_err("Failed to copy data from kernel"
					   " struct into userspace struct");
			ret = -EFAULT;
			goto fail;
		}
	}

	if (ret < 0)
		goto fail;

	tegra_soc_hwpm_dbg("The %s IOCTL completed successfully!",
			   ioctls[ioctl_num].name);
	goto cleanup;

fail:
	tegra_soc_hwpm_err("The %s IOCTL failed(%d)!",
			   ioctls[ioctl_num].name, ret);
cleanup:
	if (arg_copy)
		kfree(arg_copy);

	return ret;
}

static int tegra_soc_hwpm_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	unsigned int minor = iminor(inode);
	struct tegra_soc_hwpm *hwpm = NULL;
	u32 i;

	if (!inode) {
		tegra_soc_hwpm_err("Invalid inode");
		return -EINVAL;
	}
	if (!filp) {
		tegra_soc_hwpm_err("Invalid file");
		return -EINVAL;
	}
	if (minor > 0) {
		tegra_soc_hwpm_err("Incorrect minor number");
		return -EBADFD;
	}

	hwpm = container_of(inode->i_cdev, struct tegra_soc_hwpm, cdev);
	if (!hwpm) {
		tegra_soc_hwpm_err("Invalid hwpm struct");
		return -EINVAL;
	}
	filp->private_data = hwpm;

	if (tegra_platform_is_silicon()) {
		ret = reset_control_assert(hwpm->hwpm_rst);
		if (ret < 0) {
			tegra_soc_hwpm_err("hwpm reset assert failed");
			ret = -ENODEV;
			goto fail;
		}
		ret = reset_control_assert(hwpm->la_rst);
		if (ret < 0) {
			tegra_soc_hwpm_err("la reset assert failed");
			ret = -ENODEV;
			goto fail;
		}
		/* Set required parent for la_clk */
		if (hwpm->la_clk && hwpm->la_parent_clk) {
			ret = clk_set_parent(hwpm->la_clk, hwpm->la_parent_clk);
			if (ret < 0) {
				tegra_soc_hwpm_err("la clk set parent failed");
				ret = -ENODEV;
				goto fail;
			}
		}
		/* set la_clk rate to 625 MHZ */
		ret = clk_set_rate(hwpm->la_clk, LA_CLK_RATE);
		if (ret < 0) {
			tegra_soc_hwpm_err("la clock set rate failed");
			ret = -ENODEV;
			goto fail;
		}
		ret = clk_prepare_enable(hwpm->la_clk);
		if (ret < 0) {
			tegra_soc_hwpm_err("la clock enable failed");
			ret = -ENODEV;
			goto fail;
		}
		ret = reset_control_deassert(hwpm->la_rst);
		if (ret < 0) {
			tegra_soc_hwpm_err("la reset deassert failed");
			ret = -ENODEV;
			goto fail;
		}
		ret = reset_control_deassert(hwpm->hwpm_rst);
		if (ret < 0) {
			tegra_soc_hwpm_err("hwpm reset deassert failed");
			ret = -ENODEV;
			goto fail;
		}
	}

	/* Initialize IP floorsweep info */
	tegra_soc_hwpm_dbg("Initialize IP fs info");
	for (i = 0U; i < TERGA_SOC_HWPM_NUM_IPS; i++) {
		hwpm->ip_fs_info[i] = 0ULL;
	}

	/* Map PMA and RTR apertures */
	ret = tegra_soc_hwpm_fs_info_init(hwpm);
	if (ret < 0) {
		tegra_soc_hwpm_err("Unable to initialize fs fs_info");
		ret = -EIO;
		goto fail;
	}

	/* Map PMA and RTR apertures */
	ret = tegra_soc_hwpm_pma_rtr_map(hwpm);
	if (ret < 0) {
		tegra_soc_hwpm_err("Unable to reserve PMA RTR apertures");
		ret = -EIO;
		goto fail;
	}

	/* Disable SLCG */
	ret = tegra_soc_hwpm_disable_slcg(hwpm);
	if (ret < 0) {
		tegra_soc_hwpm_err("Unable to disable SLCG");
		goto fail;
	}

	/* Initialize SW state */
	hwpm->bind_completed = false;
	hwpm->full_alist_size = -1;

	return 0;

fail:
	tegra_soc_hwpm_pma_rtr_unmap(hwpm);
	tegra_soc_hwpm_err("%s failed", __func__);

	return ret;
}

static ssize_t tegra_soc_hwpm_read(struct file *file,
				   char __user *ubuf,
				   size_t count,
				   loff_t *offp)
{
	return 0;
}

/* FIXME: Fix double release bug */
static int tegra_soc_hwpm_release(struct inode *inode, struct file *filp)
{
	int err = 0;
	int ret = 0;
	struct tegra_soc_hwpm *hwpm = NULL;

	if (!inode) {
		tegra_soc_hwpm_err("Invalid inode");
		return -EINVAL;
	}
	if (!filp) {
		tegra_soc_hwpm_err("Invalid file");
		return -EINVAL;
	}

	hwpm = container_of(inode->i_cdev, struct tegra_soc_hwpm, cdev);
	if (!hwpm) {
		tegra_soc_hwpm_err("Invalid hwpm struct");
		return -EINVAL;
	}

	ret = tegra_soc_hwpm_disable_pma_triggers(hwpm);
	if (ret != 0) {
		return ret;
	}

	/* Disable all PERFMONs */
	tegra_soc_hwpm_dbg("Disabling PERFMONs");
	tegra_soc_hwpm_disable_perfmons(hwpm);

	/* Clear MEM_BYTES pipeline */
	err = tegra_soc_hwpm_clear_pipeline(hwpm);
	if (err < 0) {
		tegra_soc_hwpm_err("Failed to clear MEM_BYTES pipeline");
		return err;
	}

	/* Enable SLCG */
	err = tegra_soc_hwpm_enable_slcg(hwpm);
	if (err != 0) {
		tegra_soc_hwpm_err("Unable to enable SLCG");
		return err;
	}

	/* Unmap PMA and RTR apertures */
	err = tegra_soc_hwpm_pma_rtr_unmap(hwpm);
	if (err != 0) {
		tegra_soc_hwpm_err("Unable to unmap PMA and RTR");
		return err;
	}

	tegra_soc_hwpm_reset_resources(hwpm);

	if (tegra_platform_is_silicon()) {
		err = reset_control_assert(hwpm->hwpm_rst);
		RELEASE_FAIL("hwpm reset assert failed");
		err = reset_control_assert(hwpm->la_rst);
		RELEASE_FAIL("la reset assert failed");
		clk_disable_unprepare(hwpm->la_clk);
	}

	return ret;
}

/* File ops for device node */
const struct file_operations tegra_soc_hwpm_ops = {
	.owner = THIS_MODULE,
	.open = tegra_soc_hwpm_open,
	.read = tegra_soc_hwpm_read,
	.release = tegra_soc_hwpm_release,
	.unlocked_ioctl = tegra_soc_hwpm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tegra_soc_hwpm_ioctl,
#endif
};
