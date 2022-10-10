/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, NVIDIA Corporation.  All rights reserved.
 */

#ifndef __NVHOST_NVDLA_T194_H__
#define __NVHOST_NVDLA_T194_H__

#include <linux/nvhost.h>
#include <linux/platform/tegra/emc_bwmgr.h>

#include "nvdla.h"
#include "nvdla_cg_regs.h"
#include "dla_t19x_fw_version.h"

struct nvhost_device_data t19_nvdla0_info = {
	.devfs_name_family	= "nvdla",
	.class			= NV_DLA0_CLASS_ID,
	.clocks			= {
		{"nafll_dla", UINT_MAX},
		{"nafll_dla_falcon", UINT_MAX},
		{"nvdla0", UINT_MAX},
		{"nvdla0_flcn", UINT_MAX},
		{"emc", 0,
		 NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		 TEGRA_SET_EMC_FLOOR}
	},
	.resource_policy	= RESOURCE_PER_CHANNEL_INSTANCE,
	.finalize_poweron	= nvhost_nvdla_finalize_poweron,
	.prepare_poweroff	= nvhost_nvdla_prepare_poweroff,
	.flcn_isr               = nvhost_nvdla_flcn_isr,
	.self_config_flcn_isr	= true,
	.vm_regs		= {{0x30, true}, {0x34, false} },
	.firmware_name		= NV_DLA_TEGRA194_FW,
	.version		= FIRMWARE_ENCODE_VERSION(T19X),
	.autosuspend_delay      = 500,
	.keepalive		= true,
	.poweron_reset		= true,
	.serialize		= true,
	.ctrl_ops		= &tegra_nvdla_ctrl_ops,
	.get_reloc_phys_addr	= nvhost_t194_get_reloc_phys_addr,
	.module_irq		= 1,
	.engine_cg_regs		= nvdla_gating_registers,
	.engine_can_cg		= true,
	.can_powergate		= true,
	.bwmgr_client_id	= TEGRA_BWMGR_CLIENT_DLA0,
	.transcfg_addr		= 0x0444,
	.transcfg_val		= 0x20,
	.firmware_not_in_subdir = true,
};

struct nvhost_device_data t19_nvdla1_info = {
	.devfs_name_family	= "nvdla",
	.class			= NV_DLA1_CLASS_ID,
	.clocks			= {
		{"nvdla1", UINT_MAX},
		{"nvdla1_flcn", UINT_MAX},
		{"emc", 0,
		 NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		 TEGRA_SET_EMC_FLOOR}
	},
	.resource_policy	= RESOURCE_PER_CHANNEL_INSTANCE,
	.finalize_poweron	= nvhost_nvdla_finalize_poweron,
	.prepare_poweroff	= nvhost_nvdla_prepare_poweroff,
	.flcn_isr               = nvhost_nvdla_flcn_isr,
	.self_config_flcn_isr	= true,
	.vm_regs		= {{0x30, true}, {0x34, false} },
	.firmware_name		= NV_DLA_TEGRA194_FW,
	.version		= FIRMWARE_ENCODE_VERSION(T19X),
	.autosuspend_delay      = 500,
	.keepalive		= true,
	.poweron_reset		= true,
	.serialize		= true,
	.ctrl_ops		= &tegra_nvdla_ctrl_ops,
	.get_reloc_phys_addr	= nvhost_t194_get_reloc_phys_addr,
	.module_irq		= 1,
	.engine_cg_regs		= nvdla_gating_registers,
	.engine_can_cg		= true,
	.can_powergate		= true,
	.bwmgr_client_id	= TEGRA_BWMGR_CLIENT_DLA1,
	.transcfg_addr		= 0x0444,
	.transcfg_val		= 0x20,
	.firmware_not_in_subdir = true,
};

#endif /* End of __NVHOST_NVDLA_T194_H__ */
