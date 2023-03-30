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

#ifndef TEGRA_HWPM_H
#define TEGRA_HWPM_H

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <soc/tegra/fuse.h>

#include <uapi/linux/tegra-soc-hwpm-uapi.h>

#undef BIT
#define BIT(x) (0x1U << (u32)(x))

#undef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define TEGRA_SOC_HWPM_IP_INACTIVE	~(0U)

struct hwpm_ip_register_list {
	struct tegra_soc_hwpm_ip_ops ip_ops;
	struct hwpm_ip_register_list *next;
};
extern struct hwpm_ip_register_list *ip_register_list_head;

/*
 * This structure is copy of struct tegra_soc_hwpm_ip_ops uapi structure.
 * This is not a hard requirement as tegra_hwpm_validate_ip_ops conversion
 * function.
 */
struct tegra_hwpm_ip_ops {
	/*
	 * Opaque ip device handle used for callback from
	 * SOC HWPM driver to IP drivers. This handle can be used
	 * to access IP driver functionality with the callbacks.
	 */
	void *ip_dev;
	/*
	 * hwpm_ip_pm is callback function to disable/enable
	 * IP driver power management. Before SOC HWPM doing
	 * perf measuremnts, this callback is called with
	 * "disable = true ", so that IP driver will disable IP specific
	 * power management to keep IP driver responsive. Once SOC HWPM is
	 * done with perf measurement, this callaback is called
	 * with "disable = false", so that IP driver can restore back
	 * it's orignal power management.
	 */
	int (*hwpm_ip_pm)(void *dev, bool disable);
	/*
	 * hwpm_ip_reg_op is callback function to do IP
	 * register 32 bit read or write.
	 * For read:
	 *      input : dev - IP device handle
	 *      input : reg_op - TEGRA_SOC_HWPM_IP_REG_OP_READ
	 *      input : inst_element_index - element index within IP instance
	 *      input : reg_offset - register offset
	 *      output: reg_data - u32 read value
	 * For write:
	 *      input : dev - IP device handle
	 *      input : reg_op - TEGRA_SOC_HWPM_IP_REG_OP_WRITE
	 *      input : inst_element_index - element index within IP instance
	 *      input : reg_offset - register offset
	 *      output: reg_data -  u32 write value
	 * Return:
	 *      reg_op success / failure
	 */
	int (*hwpm_ip_reg_op)(void *dev,
				enum tegra_soc_hwpm_ip_reg_op reg_op,
				u32 inst_element_index, u64 reg_offset,
				u32 *reg_data);
};

/*
 * One of the HWPM components is a perfmux. Perfmux registers belong to the
 * IP domain. There are 2 ways of accessing perfmux registers
 * - option 1: implement HWPM <-> IP interface. IP drivers register with HWPM
 *             driver and share required function pointers
 * - option 2: map perfmux register address in HWPM driver
 * Option 1 is a preferred solution. However, IP drivers have yet to
 * implement the interface. Such IPs can be force enabled from HWPM driver
 * perspective. However, forcing an IP will enable all instances of the IP.
 * Hence, IP force enable should only be done on full chip config.
 * Note as power management API is not available, unpowergating the IP via
 * command line is required.
 */
/*#define CONFIG_HWPM_ALLOW_FORCE_ENABLE*/

/* There are 3 types of HWPM components/apertures */
#define TEGRA_HWPM_APERTURE_TYPE_PERFMUX	0U
#define TEGRA_HWPM_APERTURE_TYPE_BROADCAST	1U
#define TEGRA_HWPM_APERTURE_TYPE_PERFMON	2U
#define TEGRA_HWPM_APERTURE_TYPE_MAX		3U

#define TEGRA_HWPM_RESOURCE_STATUS_INVALID	\
		TEGRA_SOC_HWPM_RESOURCE_STATUS_INVALID
#define TEGRA_HWPM_RESOURCE_STATUS_VALID	\
		TEGRA_SOC_HWPM_RESOURCE_STATUS_VALID

#define TEGRA_HWPM_FUSE_PRODUCTION_MODE_MASK		BIT(0)
#define TEGRA_HWPM_FUSE_SECURITY_MODE_MASK		BIT(1)
#define TEGRA_HWPM_FUSE_HWPM_GLOBAL_DISABLE_MASK	BIT(2)

/*
 * Devices handled by HWPM driver can be divided into 2 categories
 * - HWPM : Components in HWPM device address space
 *          All perfmons, PMA and RTR perfmuxes
 * - IP : Components in IP address space
 *        IP perfmuxes
 *
 * This enum defines MACROS to specify an element to be HWPM or IP
 * and the specific aperture.
 */
enum tegra_hwpm_element_type {
	HWPM_ELEMENT_INVALID,
	HWPM_ELEMENT_PERFMON,
	HWPM_ELEMENT_PERFMUX,
	IP_ELEMENT_PERFMUX,
	IP_ELEMENT_BROADCAST,
};

enum tegra_hwpm_funcs {
	TEGRA_HWPM_INIT_IP_STRUCTURES,
	TEGRA_HWPM_MATCH_BASE_ADDRESS,
	TEGRA_HWPM_UPDATE_IP_INST_MASK,
	TEGRA_HWPM_GET_ALIST_SIZE,
	TEGRA_HWPM_COMBINE_ALIST,
	TEGRA_HWPM_RESERVE_GIVEN_RESOURCE,
	TEGRA_HWPM_BIND_RESOURCES,
	TEGRA_HWPM_FIND_GIVEN_ADDRESS,
	TEGRA_HWPM_RELEASE_RESOURCES,
	TEGRA_HWPM_RELEASE_ROUTER,
	TEGRA_HWPM_RELEASE_IP_STRUCTURES
};

struct tegra_hwpm_func_args {
	u64 *alist;
	u64 full_alist_idx;
};

struct allowlist {
	u64 reg_offset;
	bool zero_at_init;
};

struct hwpm_ip_aperture {
	/*
	 * Indicates which domain (HWPM or IP) aperture belongs to,
	 * used for reverse mapping
	 */
	enum tegra_hwpm_element_type element_type;

	/*
	 * Element index : Index of this aperture within the instance
	 * This will be used to update element_fs_mask to indicate availability.
	 */
	u32 element_index_mask;

	/*
	 * Element index in device tree entry
	 * For perfmux entries, this index is passed to hwpm_ip_reg_op()
	 */
	u32 dt_index;

	/* MMIO device tree aperture - only populated for perfmon */
	void __iomem *dt_mmio;

	/* DT tree name */
	char name[64];

	/* Allowlist */
	struct allowlist *alist;
	u64 alist_size;

	/* Physical aperture */
	u64 start_abs_pa;
	u64 end_abs_pa;

	/* MMIO aperture */
	u64 start_pa;
	u64 end_pa;

	/* Base address: used to calculate register offset */
	u64 base_pa;

	/* Fake registers for VDK which doesn't have a SOC HWPM fmodel */
	u32 *fake_registers;
};

struct hwpm_ip_element_info {
	/* Number of elements per instance */
	u32 num_element_per_inst;

	/*
	 * Static elements in this instance corresponding to aperture
	 * Array size: num_element_per_inst
	 */
	struct hwpm_ip_aperture *element_static_array;

	/*
	 * Ascending instance address range corresponding to elements
	 * NOTE: It is possible that address range of elements in the instance
	 * are not in same sequential order as their indexes.
	 * For example, 0th element of an instance can have start address higher
	 * than element 1. In this case, range_start and range_end should be
	 * initialized in increasing order.
	 */
	u64 range_start;
	u64 range_end;

	/* Element physical address stride for each element of IP instance */
	u64 element_stride;

	/*
	 * Elements that can fit into instance address range.
	 * This gives number of indices in element_arr
	 */
	u32 element_slots;

	/*
	 * Dynamic elements array corresponding to this element
	 * Array size: element_slots pointers
	 */
	struct hwpm_ip_aperture **element_arr;
};

struct hwpm_ip_inst {
	/*
	 * HW inst index : HW instance index of this instance
	 * This mask builds hwpm_ip.inst_fs_mask indicating availability.
	 */
	u32 hw_inst_mask;

	/*
	 * An IP instance is a group of core elements of the IP
	 * Eg., channels in MSS, controllers in PCIe
	 * Performance tracking is counted by 0, 1 or more HWPM components
	 * (perfmux/perfmon) connected to each IP core element.
	 *
	 */
	u32 num_core_elements_per_inst;

	/* Element details specific to this instance */
	struct hwpm_ip_element_info element_info[TEGRA_HWPM_APERTURE_TYPE_MAX];

	/*
	 * IP ops are specific for an instance, used for perfmux and broadcast
	 * register accesses.
	 */
	struct tegra_hwpm_ip_ops ip_ops;

	/*
	 * An IP contains perfmux-perfmon groups that correspond to each other.
	 * If a perfmux is present, it indicates that the corresponding
	 * perfmon is present.
	 * This mask is usually updated based on available perfmuxes.
	 * (except for SCF).
	 */
	u32 element_fs_mask;
};

struct hwpm_ip_inst_per_aperture_info {
	/*
	 * Ascending IP address range corresponding to instances
	 * NOTE: It is possible that address range of IP instances
	 * are not in same sequential order as their indexes.
	 * For example, 0th instance of an IP can have start address higher
	 * than instance 1. In this case, range_start and range_end should be
	 * initialized in increasing order.
	 */
	u64 range_start;
	u64 range_end;

	/* Aperture address range for each IP instance */
	u64 inst_stride;

	/*
	 * Aperture instances that can fit into IP aperture address range.
	 * This gives number of entries in inst_arr
	 */
	u32 inst_slots;

	/* IP inst aperture array */
	struct hwpm_ip_inst **inst_arr;
};

struct hwpm_ip {
	/* Number of instances */
	u32 num_instances;

	/* Static array of IP instances */
	struct hwpm_ip_inst *ip_inst_static_array;

	/* Instance info corresponding to apertures in this IP */
	struct hwpm_ip_inst_per_aperture_info inst_aperture_info[
		TEGRA_HWPM_APERTURE_TYPE_MAX];

	/*
	 * Indicates fuses this IP depends on
	 * If fuse corresponding to the mask is blown,
	 * set override_enable = true
	 */
	u32 dependent_fuse_mask;

	/* Override IP config based on fuse value */
	bool override_enable;

	/*
	 * IP floorsweep info based on hw index of aperture
	 * NOTE: This mask needs to based on hw instance index because
	 * hwpm driver clients use hw instance index to find aperture
	 * info (start/end address) from hw manual.
	 */
	u32 inst_fs_mask;

	/*
	 * Resource status can be: TEGRA_HWPM_RESOURCE_STATUS_*
	 * - invalid:  resource is not available to be reserved
	 * - valid:    resource exists on the chip
	 * - reserved: resource is reserved
	 * - fault:    resource faulted during reservation
	 */
	u32 resource_status;
	bool reserved;
};

struct tegra_soc_hwpm;

struct tegra_soc_hwpm_chip {
	/* Array of pointers to active IP structures */
	struct hwpm_ip **chip_ips;

	/* Chip HALs */
	bool (*is_ip_active)(struct tegra_soc_hwpm *hwpm,
	u32 ip_index, u32 *config_ip_index);
	bool (*is_resource_active)(struct tegra_soc_hwpm *hwpm,
	u32 res_index, u32 *config_ip_index);

	u32 (*get_rtr_int_idx)(struct tegra_soc_hwpm *hwpm);
	u32 (*get_ip_max_idx)(struct tegra_soc_hwpm *hwpm);

	int (*extract_ip_ops)(struct tegra_soc_hwpm *hwpm,
	struct tegra_soc_hwpm_ip_ops *hwpm_ip_ops, bool available);
	int (*force_enable_ips)(struct tegra_soc_hwpm *hwpm);
	int (*validate_current_config)(struct tegra_soc_hwpm *hwpm);
	int (*get_fs_info)(struct tegra_soc_hwpm *hwpm,
	u32 ip_enum, u64 *fs_mask, u8 *ip_status);
	int (*get_resource_info)(struct tegra_soc_hwpm *hwpm,
	u32 resource_enum, u8 *status);

	int (*init_prod_values)(struct tegra_soc_hwpm *hwpm);
	int (*disable_slcg)(struct tegra_soc_hwpm *hwpm);
	int (*enable_slcg)(struct tegra_soc_hwpm *hwpm);

	int (*reserve_rtr)(struct tegra_soc_hwpm *hwpm);
	int (*release_rtr)(struct tegra_soc_hwpm *hwpm);

	int (*disable_triggers)(struct tegra_soc_hwpm *hwpm);
	int (*perfmon_enable)(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_aperture *perfmon);
	int (*perfmon_disable)(struct tegra_soc_hwpm *hwpm,
		struct hwpm_ip_aperture *perfmon);
	int (*perfmux_disable)(struct tegra_soc_hwpm *hwpm,
		struct hwpm_ip_aperture *perfmux);

	int (*disable_mem_mgmt)(struct tegra_soc_hwpm *hwpm);
	int (*enable_mem_mgmt)(struct tegra_soc_hwpm *hwpm,
		struct tegra_soc_hwpm_alloc_pma_stream *alloc_pma_stream);
	int (*invalidate_mem_config)(struct tegra_soc_hwpm *hwpm);
	int (*stream_mem_bytes)(struct tegra_soc_hwpm *hwpm);
	int (*disable_pma_streaming)(struct tegra_soc_hwpm *hwpm);
	int (*update_mem_bytes_get_ptr)(struct tegra_soc_hwpm *hwpm,
		u64 mem_bump);
	u64 (*get_mem_bytes_put_ptr)(struct tegra_soc_hwpm *hwpm);
	bool (*membuf_overflow_status)(struct tegra_soc_hwpm *hwpm);

	size_t (*get_alist_buf_size)(struct tegra_soc_hwpm *hwpm);
	int (*zero_alist_regs)(struct tegra_soc_hwpm *hwpm,
		struct hwpm_ip_inst *ip_inst,
		struct hwpm_ip_aperture *aperture);
	int (*copy_alist)(struct tegra_soc_hwpm *hwpm,
		struct hwpm_ip_aperture *aperture,
		u64 *full_alist,
		u64 *full_alist_idx);
	bool (*check_alist)(struct tegra_soc_hwpm *hwpm,
		struct hwpm_ip_aperture *aperture, u64 phys_addr);

	void (*release_sw_setup)(struct tegra_soc_hwpm *hwpm);
};

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

	/* Device info */
	struct tegra_soc_hwpm_device_info device_info;

	/* Active chip info */
	struct tegra_soc_hwpm_chip *active_chip;

	/* Clocks and resets */
	struct clk *la_clk;
	struct clk *la_parent_clk;
	struct reset_control *la_rst;
	struct reset_control *hwpm_rst;

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
	bool device_opened;
	u64 full_alist_size;

	atomic_t hwpm_in_use;

	u32 dbg_mask;

	/* Debugging */
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_root;
#endif
	bool fake_registers_enabled;
};

#endif /* TEGRA_HWPM_H */
