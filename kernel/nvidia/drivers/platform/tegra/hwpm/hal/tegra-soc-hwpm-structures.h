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
 */

#ifndef TEGRA_SOC_HWPM_STRUCTURES_H
#define TEGRA_SOC_HWPM_STRUCTURES_H

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/delay.h>

#include <uapi/linux/tegra-soc-hwpm-uapi.h>

#define TEGRA_SOC_HWPM_DT_APERTURE_INVALID 100U

#define RELEASE_FAIL(msg, ...)					\
	do {							\
		if (err < 0) {					\
			tegra_soc_hwpm_err(msg, ##__VA_ARGS__);	\
			if (ret == 0)				\
				ret = err;			\
		}						\
	} while (0)

/* FIXME: Default timeout is 1 sec. Is this sufficient for pre-si? */
#define HWPM_TIMEOUT(timeout_check, expiry_msg) ({			\
	bool timeout_expired = false;					\
	s32 timeout_msecs = 1000;					\
	u32 sleep_msecs = 100;						\
	while (!(timeout_check)) {					\
		msleep(sleep_msecs);					\
		timeout_msecs -= sleep_msecs;				\
		if (timeout_msecs <= 0) {				\
			tegra_soc_hwpm_err("Timeout expired for %s!",	\
					expiry_msg);			\
			timeout_expired = true;				\
			break;						\
		}							\
	}								\
	timeout_expired;						\
})

struct allowlist;
extern struct platform_device *tegra_soc_hwpm_pdev;
extern const struct file_operations tegra_soc_hwpm_ops;

/* Driver struct */
struct tegra_soc_hwpm {
	/* Device */
	struct platform_device *pdev;
	struct device *dev;
	struct device_node *np;
	struct class class;
	dev_t dev_t;
	struct cdev cdev;

	struct hwpm_resource *hwpm_resources;

	/* IP floorsweep info */
	u64 ip_fs_info[TERGA_SOC_HWPM_NUM_IPS];

	/* MMIO apertures in device tree */
	void __iomem **dt_apertures;

	/* Clocks and resets */
	struct clk *la_clk;
	struct clk *la_parent_clk;
	struct reset_control *la_rst;
	struct reset_control *hwpm_rst;

	struct tegra_soc_hwpm_ip_ops *ip_info;

	/* Memory Management */
	struct dma_buf *stream_dma_buf;
	struct dma_buf_attachment *stream_attach;
	struct sg_table *stream_sgt;
	struct dma_buf *mem_bytes_dma_buf;
	struct dma_buf_attachment *mem_bytes_attach;
	struct sg_table *mem_bytes_sgt;
	void *mem_bytes_kernel;

	/* SW State */
	bool bind_completed;
	s32 full_alist_size;

	/* Debugging */
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_root;
#endif
	bool fake_registers_enabled;
};

struct hwpm_resource_aperture {
	/*
	 * If false, this is a HWPM aperture (PERFRMON, PMA or RTR). Else this
	 * is a non-HWPM aperture (ex: VIC).
	 */
	bool is_ip;

	/*
	 * If is_ip == false, specify dt_aperture for readl/writel operations.
	 * If is_ip == true, dt_aperture == TEGRA_SOC_HWPM_INVALID_DT.
	 */
	u32 dt_aperture;

	/* Physical aperture */
	u64 start_abs_pa;
	u64 end_abs_pa;
	u64 start_pa;
	u64 end_pa;

	/* Allowlist */
	struct allowlist *alist;
	u64 alist_size;

	/*
	 * Currently, perfmons and perfmuxes for all instances of an IP
	 * are listed in a single aperture mask. It is possible that
	 * some instances are disable. In this case, accessing corresponding
	 * registers will result in kernel panic.
	 * Bit set in the index_mask value will indicate the instance index
	 * within that IP (or resource).
	 */
	u32 index_mask;

	/* Fake registers for VDK which doesn't have a SOC HWPM fmodel */
	u32 *fake_registers;
};

struct hwpm_resource {
	bool reserved;
	u32 map_size;
	struct hwpm_resource_aperture *map;
};

#endif /* TEGRA_SOC_HWPM_STRUCTURES_H */
