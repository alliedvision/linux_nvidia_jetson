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

#ifndef TEGRA_AON_H
#define TEGRA_AON_H

#include <linux/cdev.h>
#include <linux/types.h>

#include <aon-regs.h>

struct tegra_aon;

#define NV(p) "nvidia," p

enum smbox_msgs {
	SMBOX_IVC_READY_MSG = 0xAAAA5555,
	SMBOX_IVC_DBG_ENABLE = 0xAAAA6666,
	SMBOX_IVC_NOTIFY = 0x0000AABB,
};

/**
 * struct aon_platform_data - Data Structure to hold platform aoncific AON
 *				cluster data.
 */
struct aon_platform_data {
	/**
	 * @d : Pointer to OS agnostic aon struct. Stores all runitme info
	 * for aon cluster elements.
	 */
	struct tegra_aon *d;
	/**
	 * @fw_aon_addr : Stores the firmware address that AON sees before being
	 * converted by AST.
	 */
	u32 fw_carveout_va;
	/**
	 * fw_carveout_size : Stores the max size of AON fw carveout.
	 */
	u32 fw_carveout_size;
	/**
	 * @no_of_asts : Stores max no. of ASTs in the AON Cluster
	 */
	u8 no_of_asts;
	/**
	 * phys_stream_id : Physical stream ID to be programmed for debug
	 * purpose only.
	 */
	u32 phys_stream_id;
	/**
	 * aon_stream_id : AON stream ID to program the ASTs in debug mode
	 * only.
	 */
	u32 aon_stream_id;
	/**
	 * fw_vmindex : VMIndex to program the AST region to read FW in debug
	 * mode only.
	 */
	u8 fw_vmindex;
	/**
	 * fw_carveout_id : Carveout ID to program the AST region to read FW in
	 * debug mode only.
	 */
	u8 fw_carveout_id;
	/**
	 * @fw_name : Stores aon fw name
	 */
	const char *fw_name;
	/**
	 * @use_physical_id : Use physical streamid
	 */
	bool use_physical_id;
	/**
	 * @fw_info_valid : Tells if the above address and size info are valid.
	 * CPU driver will use this info just for debug purpose.
	 */
	bool fw_info_valid;
};

/**
 * struct aon_firmware - Contains aon firmware info
 *
 * @data : u8 pointer to hold the fw data
 * @size : size of the fw
 * @dma_handle : stores the dma_handle for firmware
 */
struct aon_firmware {
	u8 *data;
	u64 dma_handle;
};

/**
 * Declaration for struct aon_hsp that allows other structs to have a pointer
 * to it without having to define it
 */
struct aon_hsp;

/**
 * struct tegra_aon - Primary OS independent tegra aon structure to hold aon
 * cluster's and it's element's runtime info.
 * Also encapsulates linux device aoncific info.
 */
struct tegra_aon {
	/**
	 * @dev : Pointer to AON Cluster's Linux device struct.
	 */
	struct device *dev;
	/**
	 * @hsp : Pointer to HSP instance used for communication with AON FW.
	 */
	struct aon_hsp *hsp;
	/**
	 * @regs : Stores the cpu-mapped base address of AON Cluster. Will be
	 * used for MMIO transactions to AON elements.
	 */
	void __iomem *regs;
	/**
	 * @ipcbuf : Pointer to the ipc buffer.
	 */
	void *ipcbuf;
	/**
	 * @ipcbuf_size : Stores the ipc buffer size.
	 */
	size_t ipcbuf_size;
	/**
	 * @boot_status - u32 variable to store aon's boot status.
	 */
	u32 boot_status;
	/**
	 * @ivc_carveout_base_ss : Stores the shared semaphore index that holds
	 * the ipc carveout base address that AON uses to configure the AST.
	 */
	u32 ivc_carveout_base_ss;
	/**
	 * @ivc_carveout_base_ss : Stores the shared semaphore index that holds
	 * the ipc carveout size that AON uses to configure the AST.
	 */
	u32 ivc_carveout_size_ss;
	u32 ivc_tx_ss;
	u32 ivc_rx_ss;
	/**
	 * @ipcbuf_dma : DMA handle of the ipc buffer.
	 */
	dma_addr_t ipcbuf_dma;
	/**
	 * @ast_config_complete - Boolean variable to store aon's ast
	 * configuration status.
	 */
	bool ast_config_complete;
	/**
	 * @reset_complete - Boolean variable to store aon's reset status.
	 */
	bool reset_complete;
	/**
	 * @load_complete - Boolean variable to store aon's fw load status.
	 */
	bool load_complete;
	/**
	 * @log_level - Stores the log level for aon cpu prints.
	 */
	u32 log_level;
	/**
	 * @fw_data - Stores info regardign firmware to be used runtime.
	 */
	struct aon_firmware *fw;
};

/**
 * pdata_from_aon - inline function to get aon platform data from
 *				the os agnostic struct tegra_dc.
 *
 * @aon : Pointer to the os agnostic tegra_aon data structure.
 *
 * Return : pointer to struct device
 */
static inline
struct aon_platform_data *pdata_from_aon(const struct tegra_aon *aon)
{
	return dev_get_drvdata(aon->dev);
}

/**
 * aon_set_ast_config_status - updates the current status of ast configuration.
 *
 * @aon : Pointer to tegra_aon struct.
 * @val : true or false.
 *
 * Return : void
 */
static inline void aon_set_ast_config_status(struct tegra_aon *aon, bool val)
{
	aon->ast_config_complete = val;
}

/**
 * aon_set_aon_reset_status - updates the current status of aon reset.
 *
 * @aon : Pointer to tegra_aon struct.
 * @val : true or false.
 *
 * Return : void
 */
static inline void aon_set_aon_reset_status(struct tegra_aon *aon, bool val)
{
	aon->reset_complete = val;
}

/**
 * aon_set_load_fw_status - updates the current status of fw loading.
 *
 * @aon : Pointer to tegra_aon struct.
 * @val : true or false stating fw load is complete or incomplete reaonctiveely.
 *
 * Return : void
 */
static inline void aon_set_load_fw_status(struct tegra_aon *aon, bool val)
{
	aon->load_complete = val;
}

static inline void __iomem *aon_reg(const struct tegra_aon *aon, u32 reg)
{
	if (unlikely(aon->regs == NULL)) {
		dev_err(aon->dev, "AON register space not IOMapped");
		return NULL;
	}

	return (aon->regs + reg);
}

static inline u32 tegra_aon_get_fw_addr(const struct tegra_aon *aon)
{
	return pdata_from_aon(aon)->fw_carveout_va;
}

#if defined(CONFIG_DEBUG_FS)
int tegra_aon_debugfs_create(struct tegra_aon *aon);
void tegra_aon_debugfs_remove(struct tegra_aon *aon);
#else
static inline int tegra_aon_debugfs_create(struct tegra_aon *aon) { return 0; }
static inline void tegra_aon_debugfs_remove(struct tegra_aon *aon) { return; }
#endif

int tegra_aon_reset(struct tegra_aon *aon);
int tegra_aon_mail_init(struct tegra_aon *aon);
int tegra_aon_ipc_init(struct tegra_aon *aon);
void tegra_aon_mail_deinit(struct tegra_aon *aon);
int tegra_aon_ast_config(struct tegra_aon *aon);

u32 tegra_aon_hsp_ss_status(const struct tegra_aon *aon, u32 ss);
void tegra_aon_hsp_ss_set(const struct tegra_aon *aon, u32 ss, u32 bits);
void tegra_aon_hsp_ss_clr(const struct tegra_aon *aon, u32 ss, u32 bits);
void tegra_aon_hsp_sm_write(const struct tegra_aon *aon, u32 sm, u32 value);

#endif
