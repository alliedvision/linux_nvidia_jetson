/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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
#include <dce.h>
#include <dce-log.h>
#include <dce-util-common.h>

#define MAX_NO_ASTS 2
#define MAX_AST_REGIONS 1
#define MAX_AST_STRMCTLS 2

#define AST_MASTER_ADDR_HI_BITS_SHIFT 32

/**
 * dce_config_ast0_control - programs the global
 *				ast control register for AST0
 *
 * @d : Pointer to tegra_dce struct
 *
 * Return : Void
 */
static void dce_config_ast0_control(struct tegra_dce *d)
{
	u32 val;
	u32 def_physical;
	u32 phy_stream_id = dce_get_phys_stream_id(d) <<
				ast_ast0_control_physstreamid_shift_v();

	if (dce_is_physical_id_valid(d))
		def_physical = 1 <<
			ast_ast0_control_carveoutlock_defphysical_shift_v();
	else
		def_physical = 0 <<
			ast_ast0_control_carveoutlock_defphysical_shift_v();

	val = phy_stream_id | ast_ast0_control_carveoutlock_false_f()
		| def_physical | ast_ast0_control_matcherrctl_decerr_f()
		| ast_ast0_control_lock_false_f();

	dce_writel(d, ast_ast0_control_r(), val);
}

/**
 * dce_config_ast1_control - programs the global
 *				ast control register for AST1
 *
 * @d : Pointer to tegra_dce struct
 *
 * Return : Void
 */
static void dce_config_ast1_control(struct tegra_dce *d)
{
	u32 val;
	u32 def_physical;
	u32 phy_stream_id = dce_get_phys_stream_id(d) <<
				ast_ast1_control_physstreamid_shift_v();

	if (dce_is_physical_id_valid(d))
		def_physical = 1 <<
			ast_ast1_control_carveoutlock_defphysical_shift_v();
	else
		def_physical = 0 <<
			ast_ast1_control_carveoutlock_defphysical_shift_v();

	val = phy_stream_id | ast_ast1_control_carveoutlock_false_f()
		| def_physical | ast_ast1_control_matcherrctl_decerr_f()
		| ast_ast1_control_lock_false_f();

	dce_writel(d, ast_ast1_control_r(), val);
}

/**
 * ast_ctl_fn is an array of read-only pointers
 * to a function returning void.
 *
 * Contains all the functions to program the global
 * ast control registers defined above.
 */
static void (*const ast_ctl_fn[MAX_NO_ASTS])(struct tegra_dce *d) = {

	dce_config_ast0_control,
	dce_config_ast1_control,
};

/**
 * dce_cfg_ast0_streamid_ctl_0 - programs the ast streamid
 *				control register for AST0 and Control0
 *
 * @d : Pointer to tegra_dce struct
 *
 * Return : Void
 */
static void dce_cfg_ast0_streamid_ctl_0(struct tegra_dce *d)
{
	u32 stream_id_en;
	u32 dce_stream_id = dce_get_dce_stream_id(d);

	if (dce_is_physical_id_valid(d))
		stream_id_en = ast_ast0_streamid_ctl_0_enable_disable_f();
	else
		stream_id_en = ast_ast0_streamid_ctl_0_enable_enable_f();

	dce_writel(d, ast_ast0_streamid_ctl_0_r(),
		(dce_stream_id << ast_ast0_streamid_ctl_0_streamid_shift_v()) |
		stream_id_en);
}

/**
 * dce_cfg_ast0_streamid_ctl_1 - programs the ast streamid
 *				control register for AST0 and Control1
 *
 * @d : Pointer to tegra_dce struct
 *
 * Return : Void
 */
static void dce_cfg_ast0_streamid_ctl_1(struct tegra_dce *d)
{
	u32 stream_id_en;
	u32 dce_stream_id = dce_get_dce_stream_id(d);

	if (dce_is_physical_id_valid(d))
		stream_id_en = ast_ast0_streamid_ctl_1_enable_disable_f();
	else
		stream_id_en = ast_ast0_streamid_ctl_1_enable_enable_f();

	dce_writel(d, ast_ast0_streamid_ctl_1_r(), (dce_stream_id <<
			ast_ast0_streamid_ctl_1_streamid_shift_v()) |
			stream_id_en);
}

/**
 * dce_cfg_ast1_streamid_ctl_0 - programs the ast streamid
 *				control register for AST1 and Control0
 *
 * @d : Pointer to tegra_dce struct
 *
 * Return : Void
 */
static void dce_cfg_ast1_streamid_ctl_0(struct tegra_dce *d)
{
	u32 stream_id_en;
	u32 dce_stream_id = dce_get_dce_stream_id(d);

	if (dce_is_physical_id_valid(d))
		stream_id_en = ast_ast1_streamid_ctl_0_enable_disable_f();
	else
		stream_id_en = ast_ast1_streamid_ctl_0_enable_enable_f();

	dce_writel(d, ast_ast1_streamid_ctl_0_r(),
		(dce_stream_id << ast_ast1_streamid_ctl_0_streamid_shift_v()) |
		stream_id_en);
}

/**
 * dce_cfg_ast1_streamid_ctl_1 - programs the ast streamid
 *				control register for AST1 and Control1
 *
 * @d : Pointer to tegra_dce struct
 *
 * Return : Void
 */
static void dce_cfg_ast1_streamid_ctl_1(struct tegra_dce *d)
{
	u32 stream_id_en;
	u32 dce_stream_id = dce_get_dce_stream_id(d);

	if (dce_is_physical_id_valid(d))
		stream_id_en = ast_ast1_streamid_ctl_1_enable_disable_f();
	else
		stream_id_en = ast_ast1_streamid_ctl_1_enable_enable_f();

	dce_writel(d, ast_ast1_streamid_ctl_1_r(),
		(dce_stream_id << ast_ast1_streamid_ctl_1_streamid_shift_v()) |
		stream_id_en);
}

/**
 * ast_strmidctl_fn is a 2D array of read-only pointers
 * to a function returning void.
 *
 * Contains all the functions to program the streamId
 * controls registers defined above for a given AST and Control ID
 */
static void (*const ast_strmidctl_fn[MAX_NO_ASTS][MAX_AST_STRMCTLS])
						(struct tegra_dce *d) = {

	{
		dce_cfg_ast0_streamid_ctl_0,
		dce_cfg_ast0_streamid_ctl_1,
	},
	{
		dce_cfg_ast1_streamid_ctl_0,
		dce_cfg_ast1_streamid_ctl_1,
	},
};


/**
 * dce_set_ast0_slave_addr_32_reg0 - programs the ast slave address
 *						for AST0 and Region0
 *
 * @d : Pointer to tegra_dce sturct.
 * @addr : Address to be programmed.
 *
 * Return : Void
 */
static void dce_set_ast0_slave_addr_32_reg0(struct tegra_dce *d, u32 addr)
{
	dce_writel(d, ast_ast0_region_0_slave_base_lo_r(),
		   (addr | ast_ast0_region_0_slave_base_lo_enable_true_f()) &
		   ast_ast1_region_0_slave_base_lo_write_mask_v());
}

/**
 * dce_set_ast1_slave_addr_32_reg0- programs the ast slave address
 *						for AST1 and Region0
 *
 * @d : Pointer to tegra_dce sturct.
 * @addr : Address to be programmed.
 *
 * Return : Void
 */
static void dce_set_ast1_slave_addr_32_reg0(struct tegra_dce *d, u32 addr)
{
	dce_writel(d, ast_ast1_region_0_slave_base_lo_r(),
		   (addr | ast_ast1_region_0_slave_base_lo_enable_true_f()) &
		   ast_ast1_region_0_slave_base_lo_write_mask_v());
}

/**
 * ast_slave_addr_fn is a 2D array of read-only pointers
 * to a function returning void.
 *
 * Contains all the functions to program the slave address and
 * other bits in salve address registers defined above for a
 * given AST and region.
 */
static void (*const ast_slave_addr_fn[MAX_NO_ASTS][MAX_AST_REGIONS])
	     (struct tegra_dce *d, u32 addr) = {

	{
		dce_set_ast0_slave_addr_32_reg0,
	},
	{
		dce_set_ast1_slave_addr_32_reg0,
	},
};

/**
 * dce_set_ast0_master_addr_lo_reg0 - programs the lower 32 bits of ast
 *					master address for AST0 and Region0
 *
 * @d : Pointer to tegra_dce sturct.
 * @addr : Address to be programmed.
 *
 * Return : Void
 */
static inline void
dce_set_ast0_master_addr_lo_reg0(struct tegra_dce *d, u32 addr)
{
	dce_writel(d, ast_ast0_region_0_master_base_lo_r(), addr);
}

/**
 * dce_set_ast1_master_addr_lo_reg0 - programs the lower 32 bits of ast
 *					master address for AST1 and Region0
 *
 * @d : Pointer to tegra_dce sturct.
 * @addr : Address to be programmed.
 *
 * Return : Void
 */
static inline void
dce_set_ast1_master_addr_lo_reg0(struct tegra_dce *d, u32 addr)
{
	dce_writel(d, ast_ast1_region_0_master_base_lo_r(), addr);
}

/**
 * dce_set_ast1_master_addr_hi_reg0 - programs the high 32 bits of ast
 *					master address for AST1 and Region0
 *
 * @d : Pointer to tegra_dce sturct.
 * @addr : Address to be programmed.
 *
 * Return : Void
 */
static inline void
dce_set_ast1_master_addr_hi_reg0(struct tegra_dce *d, u32 addr)
{
	dce_writel(d, ast_ast1_region_0_master_base_hi_r(), addr);
}

/**
 * dce_set_ast0_master_addr_hi_reg0 - programs the high 32 bits of ast
 *					master address for AST0 and Region0
 *
 * @d : Pointer to tegra_dce sturct.
 * @addr : Address to be programmed.
 *
 * Return : Void
 */
static inline void
dce_set_ast0_master_addr_hi_reg0(struct tegra_dce *d, u32 addr)
{
	dce_writel(d, ast_ast0_region_0_master_base_hi_r(), addr);
}

/**
 * dce_set_ast0_master_addr_reg0 - programs the ast master address
 *						for AST0 and Region0
 *
 * @d : Pointer to tegra_dce sturct.
 * @addr : Address to be programmed.
 *
 * Return : Void
 */
static void dce_set_ast0_master_addr_reg0(struct tegra_dce *d, u64 addr)
{
	u32 ast_master_hi;
	u32 ast_master_lo;

	ast_master_lo = addr & ast_ast0_region_0_master_base_lo_write_mask_v();
	dce_set_ast0_master_addr_lo_reg0(d, ast_master_lo);

	ast_master_hi = addr >> AST_MASTER_ADDR_HI_BITS_SHIFT;
	dce_set_ast0_master_addr_hi_reg0(d, ast_master_hi);
}

/**
 * dce_set_ast1_master_addr_reg0 - programs the ast master address
 *						for AST0 and Region0
 *
 * @d : Pointer to tegra_dce sturct.
 * @addr : Address to be programmed.
 *
 * Return : Void
 */
static void dce_set_ast1_master_addr_reg0(struct tegra_dce *d, u64 addr)
{
	u32 ast_master_hi;
	u32 ast_master_lo;

	ast_master_lo = addr & ast_ast0_region_0_master_base_lo_write_mask_v();
	dce_set_ast1_master_addr_lo_reg0(d, ast_master_lo);

	ast_master_hi = addr >> AST_MASTER_ADDR_HI_BITS_SHIFT;
	dce_set_ast1_master_addr_hi_reg0(d, ast_master_hi);
}

/**
 * ast_master_addr_fn is a 2D array of read-only pointers
 * to a function returning void.
 *
 * Contains all the functions to program the master address registers
 * defined above for a given AST and region.
 */
static void (*const ast_master_addr_fn[MAX_NO_ASTS][MAX_AST_REGIONS])
	     (struct tegra_dce *d, u64 addr) = {

	{
		dce_set_ast0_master_addr_reg0,
	},
	{
		dce_set_ast1_master_addr_reg0,
	},
};

/**
 * dce_get_fw_ast_reg_mask - Returns the size mask based on the fw size
 *				for configuring AST region
 *
 * @d : Pointer to tegra_dce struct
 *
 * If size is 64K(0x10000), mask is 0xffff. If size is 2MB(0x200000), mask is
 * 0x1FFFFF.
 *
 * Ruturns 64 bit mask.
 */
u64 dce_get_fw_ast_reg_mask(struct tegra_dce *d)
{
	struct dce_firmware *fw = d->fw_data;

	return fw->size - 1UL;
}

/**
 * dce_ast_cfg_reg_mask_ast0_reg0 - sets the region mask based on the size
 *					of the DRAM memory for AST0 and Region0.
 *
 * @d : pointer to tegra_dce struct
 *
 * Return : Void
 */
static void dce_ast_cfg_reg_mask_ast0_reg0(struct tegra_dce *d)
{
	u64 size_mask = dce_get_fw_ast_reg_mask(d);
	u32 val = size_mask & ast_ast0_region_0_mask_lo_write_mask_v();

	dce_writel(d, ast_ast0_region_0_mask_lo_r(), val);
}

/**
 * dce_ast_cfg_reg_mask_ast1_reg0 - sets the region mask based on the size
 *					of the DRAM memory for AST1 and Region0.
 *
 * @d : pointer to tegra_dce struct
 *
 * Return : Void
 */
static void dce_ast_cfg_reg_mask_ast1_reg0(struct tegra_dce *d)
{
	u64 size_mask = dce_get_fw_ast_reg_mask(d);
	u32 val = size_mask & ast_ast1_region_0_mask_lo_write_mask_v();

	dce_writel(d, ast_ast1_region_0_mask_lo_r(), val);
}

/**
 * ast_mask_fn is a 2D array of read-only pointers
 * to a function returning void.
 *
 * Contains all the functions to program the mask
 * register bits for a given AST and region.
 */
static void (*const ast_mask_fn[MAX_NO_ASTS][MAX_AST_REGIONS])
						(struct tegra_dce *d) = {

	{
		dce_ast_cfg_reg_mask_ast0_reg0,
	},
	{
		dce_ast_cfg_reg_mask_ast1_reg0,
	},
};

/**
 * dce_ast_cfg_reg_control_ast0_reg0 - Configures the ast region control
 *						register for AST0 and Region0
 *
 * @d : Pointer to tegra_dce struct
 *
 * Return : Void
 */
static void dce_ast_cfg_reg_control_ast0_reg0(struct tegra_dce *d)
{
	u32 vm_index;
	u32 carveout_id;
	u32 use_physical_id;

	if (dce_is_physical_id_valid(d)) {
		use_physical_id = 1 <<
			ast_ast0_region_0_control_physical_shift_v();
		vm_index = 0 <<
			ast_ast0_region_0_control_vmindex_shift_v();
	} else {
		use_physical_id = 0 <<
			ast_ast0_region_0_control_physical_shift_v();
		vm_index = dce_get_fw_vm_index(d) <<
			ast_ast0_region_0_control_vmindex_shift_v();
	}
	carveout_id = dce_get_fw_carveout_id(d) <<
		ast_ast0_region_0_control_carveoutid_shift_v();

	dce_writel(d, ast_ast0_region_0_control_r(),
		   use_physical_id | vm_index | carveout_id |
		   ast_ast0_region_0_control_snoop_enable_f());
}

/**
 * dce_ast_cfg_reg_control_ast1_reg0 - Configures the ast region control
 *						register for AST1 and Region0
 *
 * @d : Pointer to tegra_dce struct
 *
 * Return : Void
 */
static void dce_ast_cfg_reg_control_ast1_reg0(struct tegra_dce *d)
{
	u32 vm_index;
	u32 carveout_id;
	u32 use_physical_id;

	if (dce_is_physical_id_valid(d)) {
		use_physical_id = 1 <<
			ast_ast1_region_0_control_physical_shift_v();
		vm_index = 0 <<
			ast_ast1_region_0_control_vmindex_shift_v();
	} else {
		use_physical_id = 0 <<
			ast_ast1_region_0_control_physical_shift_v();
		vm_index = dce_get_fw_vm_index(d) <<
			ast_ast1_region_0_control_vmindex_shift_v();
	}

	carveout_id = dce_get_fw_carveout_id(d) <<
		ast_ast1_region_0_control_carveoutid_shift_v();


	dce_writel(d, ast_ast1_region_0_control_r(),
		   use_physical_id | vm_index | carveout_id |
		   ast_ast1_region_0_control_snoop_enable_f());
}

/**
 * ast_reg_control_fn is a 2D array of read-only pointers
 * to a function returning void.
 *
 * Contains all the functions to program the region control
 * register bits given AST and region.
 */
static void (*const ast_reg_control_fn[MAX_NO_ASTS][MAX_AST_REGIONS])
						(struct tegra_dce *d) = {

	{
		dce_ast_cfg_reg_control_ast0_reg0,
	},
	{
		dce_ast_cfg_reg_control_ast1_reg0,
	},
};

/**
 * dce_config_ast - Configures the a AST region for initial loading of fw
 * platform data.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : void
 */
void dce_config_ast(struct tegra_dce *d)
{
	u8 i;
	u8 j;
	u32 slave_addr;
	u64 master_addr;

	d->boot_status |= DCE_AST_CONFIG_START;
	slave_addr = dce_get_fw_dce_addr(d);

	if (!d->fw_data) {
		dce_err(d, "DCE_BOOT_FAILED: No fw_data present");
		d->boot_status |= DCE_AST_CONFIG_FAILED;
		return;
	}

	master_addr = d->fw_data->dma_handle;

	for (i = 0; i < MAX_NO_ASTS; i++) {
		ast_ctl_fn[i](d);

		for (j = 0; j < MAX_AST_STRMCTLS; j++)
			ast_strmidctl_fn[i][j](d);

		for (j = 0; j < MAX_AST_REGIONS; j++) {
			ast_mask_fn[i][j](d);
			ast_reg_control_fn[i][j](d);
			ast_master_addr_fn[i][j](d, master_addr);
			ast_slave_addr_fn[i][j](d, slave_addr);
		}
	}
	d->boot_status |= DCE_AST_CONFIG_DONE;
}
