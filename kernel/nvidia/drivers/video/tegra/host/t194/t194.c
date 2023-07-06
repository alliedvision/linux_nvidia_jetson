/*
 * Tegra Graphics Init for T194 Architecture Chips
 *
 * Copyright (c) 2016-2022, NVIDIA Corporation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/version.h>
#include <linux/platform/tegra/emc_bwmgr.h>

#include <soc/tegra/kfuse.h>

#include <linux/tegra_vhost.h>

#include "platform.h"
#include "dev.h"
#include "class_ids.h"
#include "class_ids_t194.h"

#include "nvhost_syncpt_unit_interface.h"
#include "t194.h"
#include "host1x/host1x.h"
#if IS_ENABLED(CONFIG_TEGRA_GRHOST_ISP)
#include "isp/isp5.h"
#endif
#if IS_ENABLED(CONFIG_TEGRA_GRHOST_TSEC)
#include "tsec/tsec.h"
#endif
#include "flcn/flcn.h"
#if IS_ENABLED(CONFIG_TEGRA_GRHOST_NVCSI)
#include "nvcsi/nvcsi-t194.h"
#endif
#if IS_ENABLED(CONFIG_TEGRA_GRHOST_NVDEC)
#include "nvdec/nvdec.h"
#endif
#include "hardware_t194.h"

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_SLVSEC)
#include "slvsec/slvsec.h"
#endif
#if IS_ENABLED(CONFIG_VIDEO_TEGRA_VI)
#include "vi/vi5.h"
#endif

#include "chip_support.h"

#include "scale_emc.h"

#include "streamid_regs.c"
#include "cg_regs.c"
#include "actmon_regs.c"

dma_addr_t nvhost_t194_get_reloc_phys_addr(dma_addr_t phys_addr, u32 reloc_type)
{
	if (reloc_type == NVHOST_RELOC_TYPE_BLOCK_LINEAR)
		phys_addr += BIT(39);

	return phys_addr;
}

static struct host1x_device_info host1x04_info = {
	.nb_channels	= T194_NVHOST_NUMCHANNELS,
	.ch_base	= 0,
	.ch_limit	= T194_NVHOST_NUMCHANNELS,
	.nb_mlocks	= NV_HOST1X_NB_MLOCKS,
	.initialize_chip_support = nvhost_init_t194_support,
	.nb_hw_pts	= NV_HOST1X_SYNCPT_NB_PTS,
	.nb_pts		= NV_HOST1X_SYNCPT_NB_PTS,
	.pts_base	= 0,
	.pts_limit	= NV_HOST1X_SYNCPT_NB_PTS,
	.nb_syncpt_irqs	= 1,
	.syncpt_policy	= SYNCPT_PER_CHANNEL_INSTANCE,
	.channel_policy	= MAP_CHANNEL_ON_SUBMIT,
	.nb_actmons	= 1,
	.use_cross_vm_interrupts = 1,
	.resources	= {
		"guest",
		"hypervisor",
		"actmon",
		"sem-syncpt-shim"
	},
	.nb_resources	= 4,
	.secure_cmdfifo = true,
	.syncpt_page_size = 0x1000,
};

struct nvhost_device_data t19_host1x_info = {
	.clocks			= {
		{"host1x", 204000000},
		{"actmon", UINT_MAX}
	},
	.autosuspend_delay      = 50,
	.private_data		= &host1x04_info,
	.finalize_poweron	= nvhost_host1x_finalize_poweron,
	.prepare_poweroff	= nvhost_host1x_prepare_poweroff,
	.engine_can_cg		= true,
};

struct nvhost_device_data t19_host1x_hv_info = {
	.clocks			= {
		{"host1x", 204000000},
		{"actmon", UINT_MAX}
	},
	.autosuspend_delay      = 2000,
	.private_data		= &host1x04_info,
	.finalize_poweron = nvhost_host1x_finalize_poweron,
	.prepare_poweroff = nvhost_host1x_prepare_poweroff,
};

static struct host1x_device_info host1xb04_info = {
	.nb_channels	= T194_NVHOST_NUMCHANNELS,
	.ch_base	= 0,
	.ch_limit	= T194_NVHOST_NUMCHANNELS,
	.nb_mlocks	= NV_HOST1X_NB_MLOCKS,
	.initialize_chip_support = nvhost_init_t194_support,
	.nb_hw_pts	= NV_HOST1X_SYNCPT_NB_PTS,
	.nb_pts		= NV_HOST1X_SYNCPT_NB_PTS,
	.pts_base	= 0,
	.pts_limit	= NV_HOST1X_SYNCPT_NB_PTS,
	.nb_syncpt_irqs	= 1,
	.syncpt_policy	= SYNCPT_PER_CHANNEL_INSTANCE,
	.channel_policy	= MAP_CHANNEL_ON_SUBMIT,
	.use_cross_vm_interrupts = 1,
};

struct nvhost_device_data t19_host1xb_info = {
	.clocks			= {
		{"host1x", UINT_MAX},
		{"actmon", UINT_MAX}
	},
	.private_data		= &host1xb04_info,
};

#if IS_ENABLED(CONFIG_VIDEO_TEGRA_VI)
struct nvhost_device_data t19_vi_thi_info = {
	.devfs_name		= "vi-thi",
	.moduleid		= NVHOST_MODULE_VI,
};

struct nvhost_device_data t19_vi5_info = {
	.devfs_name		= "vi",
	.moduleid		= NVHOST_MODULE_VI,
	.clocks = {
		{"vi", UINT_MAX},
		{"emc", 0,
		 NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		 TEGRA_SET_EMC_FLOOR, false, UINT_MAX}
	},
	.num_ppc		= 8,
	.aggregate_constraints	= nvhost_vi5_aggregate_constraints,
	.pre_virt_init		= vi5_priv_early_probe,
	.post_virt_init		= vi5_priv_late_probe,
	.bwmgr_client_id	= TEGRA_BWMGR_CLIENT_VI,
};
#endif

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_NVCSI)
struct nvhost_device_data t19_nvcsi_info = {
	.moduleid		= NVHOST_MODULE_NVCSI,
	.clocks			= {
		{"nvcsi", 400000000},
	},
	.devfs_name		= "nvcsi",
	.autosuspend_delay      = 500,
	.can_powergate = true,
};
#endif

#ifdef CONFIG_TEGRA_GRHOST_ISP
struct nvhost_device_data t19_isp_thi_info = {
	.devfs_name		= "isp-thi",
	.moduleid		= NVHOST_MODULE_ISP,
};

struct nvhost_device_data t19_isp5_info = {
	.devfs_name		= "isp",
	.moduleid		= NVHOST_MODULE_ISP,
	.clocks			= {
		{"isp", UINT_MAX},
	},
	.ctrl_ops		= &tegra194_isp5_ctrl_ops,
	.pre_virt_init		= isp5_priv_early_probe,
	.post_virt_init		= isp5_priv_late_probe,
	.autosuspend_delay      = 500,
	.can_powergate = true,
};
#endif

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_NVENC)
struct nvhost_device_data t19_msenc_info = {
	.version		= NVHOST_ENCODE_FLCN_VER(7, 0),
	.devfs_name		= "msenc",
	.class			= NV_VIDEO_ENCODE_NVENC_CLASS_ID,
	.modulemutexes		= {NV_HOST1X_MLOCK_ID_NVENC},
	.autosuspend_delay      = 500,
	.clocks			= {
		{"nvenc", UINT_MAX},
		{"emc", 0,
		 NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		 TEGRA_SET_EMC_SHARED_BW}
	},
	.poweron_reset		= true,
	.finalize_poweron	= nvhost_flcn_finalize_poweron_t194,
	.moduleid		= NVHOST_MODULE_MSENC,
	.num_channels		= 1,
	.firmware_name		= "nvhost_nvenc070.fw",
	.firmware_not_in_subdir = true,
	.serialize		= true,
	.push_work_done		= true,
	.resource_policy	= RESOURCE_PER_CHANNEL_INSTANCE,
	.vm_regs		= {{0x30, true}, {0x34, false} },
	.transcfg_addr		= 0x1844,
	.transcfg_val		= 0x20,
	.bwmgr_client_id	= TEGRA_BWMGR_CLIENT_MSENC,
	.scaling_init		= nvhost_scale_emc_init,
	.scaling_deinit		= nvhost_scale_emc_deinit,
	.scaling_post_cb	= &nvhost_scale_emc_callback,
	.actmon_regs		= HOST1X_THOST_ACTMON_NVENC,
	.actmon_enabled         = true,
	.actmon_irq		= 2,
	.actmon_weight_count	= 224,
	.actmon_setting_regs	= t19x_nvenc_actmon_registers,
	.devfreq_governor	= "userspace",
	.get_reloc_phys_addr	= nvhost_t194_get_reloc_phys_addr,
	.engine_cg_regs		= t19x_nvenc_gating_registers,
	.engine_can_cg		= true,
	.can_powergate		= true,
	.isolate_contexts	= true,
	.enable_timestamps	= flcn_enable_timestamps,
	.mlock_timeout_factor   = 4,
};

struct nvhost_device_data t19_nvenc1_info = {
	.version		= NVHOST_ENCODE_FLCN_VER(7, 0),
	.devfs_name		= "nvenc1",
	.class			= NV_VIDEO_ENCODE_NVENC1_CLASS_ID,
	.modulemutexes		= {NV_HOST1X_MLOCK_ID_NVENC1},
	.autosuspend_delay      = 500,
	.clocks			= {
		{"nvenc", UINT_MAX},
		{"emc", 0,
		 NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		 TEGRA_SET_EMC_SHARED_BW}
	},
	.poweron_reset		= true,
	.finalize_poweron	= nvhost_flcn_finalize_poweron_t194,
	.moduleid		= NVHOST_MODULE_NVENC1,
	.num_channels		= 1,
	.firmware_name		= "nvhost_nvenc070.fw",
	.firmware_not_in_subdir = true,
	.serialize		= true,
	.push_work_done		= true,
	.resource_policy	= RESOURCE_PER_CHANNEL_INSTANCE,
	.vm_regs		= {{0x30, true}, {0x34, false} },
	.transcfg_addr		= 0x1844,
	.transcfg_val		= 0x20,
	.get_reloc_phys_addr	= nvhost_t194_get_reloc_phys_addr,
	.bwmgr_client_id	= TEGRA_BWMGR_CLIENT_NVENC1,
	.scaling_init		= nvhost_scale_emc_init,
	.scaling_deinit		= nvhost_scale_emc_deinit,
	.scaling_post_cb	= &nvhost_scale_emc_callback,
	.actmon_regs		= HOST1X_THOST_ACTMON_NVENC1,
	.actmon_enabled         = true,
	.actmon_irq		= 6,
	.actmon_weight_count	= 224,
	.actmon_setting_regs	= t19x_nvenc_actmon_registers,
	.devfreq_governor	= "userspace",
	.engine_cg_regs		= t19x_nvenc_gating_registers,
	.engine_can_cg		= true,
	.can_powergate		= true,
	.isolate_contexts	= true,
	.enable_timestamps	= flcn_enable_timestamps,
	.mlock_timeout_factor   = 4,
};
#endif

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_NVDEC)
struct nvhost_device_data t19_nvdec_info = {
	.version		= NVHOST_ENCODE_NVDEC_VER(4, 0),
	.devfs_name		= "nvdec",
	.modulemutexes		= {NV_HOST1X_MLOCK_ID_NVDEC},
	.class			= NV_NVDEC_CLASS_ID,
	.autosuspend_delay      = 500,
	.clocks			= {
		{"nvdec", UINT_MAX},
		{"kfuse", 0, 0},
		{"efuse", 0, 0},
		{"emc", 0,
		 NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		 TEGRA_SET_EMC_FLOOR}
	},
	.poweron_reset		= true,
	.finalize_poweron	= nvhost_nvdec_finalize_poweron_t194,
	.prepare_poweroff	= nvhost_nvdec_prepare_poweroff_t194,
	.moduleid		= NVHOST_MODULE_NVDEC,
	.ctrl_ops		= &tegra_nvdec_ctrl_ops,
	.num_channels		= 1,
	.serialize		= true,
	.push_work_done		= true,
	.resource_policy	= RESOURCE_PER_CHANNEL_INSTANCE,
	.vm_regs		= {{0x30, true}, {0x34, false} },
	.transcfg_addr		= 0x2c44,
	.transcfg_val		= 0x20,
	.bwmgr_client_id	= TEGRA_BWMGR_CLIENT_NVDEC,
	.scaling_init		= nvhost_scale_emc_init,
	.scaling_deinit		= nvhost_scale_emc_deinit,
	.scaling_post_cb	= &nvhost_scale_emc_callback,
	.actmon_regs		= HOST1X_THOST_ACTMON_NVDEC,
	.actmon_enabled         = true,
	.actmon_irq		= 4,
	.actmon_weight_count	= 248,
	.actmon_setting_regs	= t19x_nvdec_actmon_registers,
	.devfreq_governor	= "userspace",
	.get_reloc_phys_addr	= nvhost_t194_get_reloc_phys_addr,
	.engine_cg_regs		= t19x_nvdec_gating_registers,
	.engine_can_cg		= true,
	.can_powergate		= true,
	.isolate_contexts	= true,
	.enable_timestamps	= flcn_enable_timestamps,
	.mlock_timeout_factor   = 4,
};

struct nvhost_device_data t19_nvdec1_info = {
	.version		= NVHOST_ENCODE_NVDEC_VER(4, 0),
	.devfs_name		= "nvdec1",
	.modulemutexes		= {NV_HOST1X_MLOCK_ID_NVDEC1},
	.class			= NV_NVDEC1_CLASS_ID,
	.autosuspend_delay      = 500,
	.clocks			= {
		{"nvdec", UINT_MAX},
		{"kfuse", 0, 0},
		{"efuse", 0, 0},
		{"emc", 0,
		 NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		 TEGRA_SET_EMC_FLOOR}
	},
	.poweron_reset		= true,
	.finalize_poweron	= nvhost_nvdec_finalize_poweron_t194,
	.prepare_poweroff	= nvhost_nvdec_prepare_poweroff_t194,
	.moduleid		= NVHOST_MODULE_NVDEC1,
	.ctrl_ops		= &tegra_nvdec_ctrl_ops,
	.num_channels		= 1,
	.serialize		= true,
	.push_work_done		= true,
	.resource_policy	= RESOURCE_PER_CHANNEL_INSTANCE,
	.vm_regs		= {{0x30, true}, {0x34, false} },
	.transcfg_addr		= 0x2c44,
	.transcfg_val		= 0x20,
	.get_reloc_phys_addr	= nvhost_t194_get_reloc_phys_addr,
	.bwmgr_client_id	= TEGRA_BWMGR_CLIENT_NVDEC1,
	.scaling_init		= nvhost_scale_emc_init,
	.scaling_deinit		= nvhost_scale_emc_deinit,
	.scaling_post_cb	= &nvhost_scale_emc_callback,
	.actmon_regs		= HOST1X_THOST_ACTMON_NVDEC1,
	.actmon_enabled         = true,
	.actmon_irq		= 7,
	.actmon_weight_count	= 248,
	.actmon_setting_regs	= t19x_nvdec_actmon_registers,
	.devfreq_governor	= "userspace",
	.engine_cg_regs		= t19x_nvdec_gating_registers,
	.engine_can_cg		= true,
	.can_powergate		= true,
	.isolate_contexts	= true,
	.enable_timestamps	= flcn_enable_timestamps,
	.mlock_timeout_factor   = 4,
};
#endif

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_NVJPG)
struct nvhost_device_data t19_nvjpg_info = {
	.version		= NVHOST_ENCODE_FLCN_VER(1, 2),
	.devfs_name		= "nvjpg",
	.modulemutexes		= {NV_HOST1X_MLOCK_ID_NVJPG},
	.class			= NV_NVJPG_CLASS_ID,
	.autosuspend_delay      = 500,
	.clocks			= {
		{"nvjpg", UINT_MAX},
		{"emc", 0,
		 NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		 TEGRA_SET_EMC_SHARED_BW}
	},
	.poweron_reset		= true,
	.finalize_poweron	= nvhost_flcn_finalize_poweron_t194,
	.moduleid		= NVHOST_MODULE_NVJPG,
	.num_channels		= 1,
	.firmware_name		= "nvhost_nvjpg012.fw",
	.serialize		= true,
	.push_work_done		= true,
	.resource_policy	= RESOURCE_PER_CHANNEL_INSTANCE,
	.vm_regs		= {{0x30, true}, {0x34, false} },
	.transcfg_addr		= 0x1444,
	.transcfg_val		= 0x20,
	.bwmgr_client_id	= TEGRA_BWMGR_CLIENT_NVJPG,
	.engine_cg_regs		= t19x_nvjpg_gating_registers,
	.engine_can_cg		= true,
	.can_powergate		= true,
	.isolate_contexts	= true,
};
#endif

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_TSEC)
struct nvhost_device_data t19_tsec_info = {
	.num_channels		= 1,
	.devfs_name		= "tsec",
	.version		= NVHOST_ENCODE_TSEC_VER(1, 0),
	.modulemutexes		= {NV_HOST1X_MLOCK_ID_TSEC},
	.class			= NV_TSEC_CLASS_ID,
	.clocks			= {
		{"tsec", UINT_MAX},
		{"efuse", 0, 0},
		{"emc", 0,
		 NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		 TEGRA_SET_EMC_FLOOR}
	},
	.autosuspend_delay      = 500,
	.keepalive		= true,
	.moduleid		= NVHOST_MODULE_TSEC,
	.poweron_reset		= true,
	.finalize_poweron	= nvhost_tsec_finalize_poweron_t194,
	.prepare_poweroff	= nvhost_tsec_prepare_poweroff,
	.serialize		= true,
	.push_work_done		= true,
	.resource_policy	= RESOURCE_PER_CHANNEL_INSTANCE,
	.vm_regs		= {{0x30, true}, {0x34, false} },
	.bwmgr_client_id	= TEGRA_BWMGR_CLIENT_TSEC,
	.engine_cg_regs		= t19x_tsec_gating_registers,
	.engine_can_cg		= true,
	.can_powergate		= true,
};

struct nvhost_device_data t19_tsecb_info = {
	.num_channels		= 1,
	.devfs_name		= "tsecb",
	.version		= NVHOST_ENCODE_TSEC_VER(1, 0),
	.modulemutexes		= {NV_HOST1X_MLOCK_ID_TSECB},
	.class			= NV_TSECB_CLASS_ID,
	.clocks			= {
		{"tsecb", UINT_MAX},
		{"efuse", 0, 0},
		{"emc", 0,
		 NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		 TEGRA_SET_EMC_FLOOR}
	},
	.autosuspend_delay      = 500,
	.keepalive		= true,
	.moduleid               = NVHOST_MODULE_TSECB,
	.poweron_reset		= true,
	.finalize_poweron	= nvhost_tsec_finalize_poweron_t194,
	.prepare_poweroff	= nvhost_tsec_prepare_poweroff,
	.serialize		= true,
	.push_work_done		= true,
	.resource_policy	= RESOURCE_PER_CHANNEL_INSTANCE,
	.vm_regs		= {{0x30, true}, {0x34, false} },
	.bwmgr_client_id	= TEGRA_BWMGR_CLIENT_TSECB,
	.engine_cg_regs		= t19x_tsec_gating_registers,
	.engine_can_cg		= true,
	.can_powergate		= true,
};
#endif

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_VIC)
struct nvhost_device_data t19_vic_info = {
	.num_channels		= 1,
	.devfs_name		= "vic",
	.clocks			= {
		{"vic", UINT_MAX, 0},
		{"emc", 0, NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		 TEGRA_SET_EMC_SHARED_BW},
	},
	.version		= NVHOST_ENCODE_FLCN_VER(4, 2),
	.autosuspend_delay      = 500,
	.moduleid		= NVHOST_MODULE_VIC,
	.poweron_reset		= true,
	.modulemutexes		= {NV_HOST1X_MLOCK_ID_VIC},
	.class			= NV_GRAPHICS_VIC_CLASS_ID,
	.finalize_poweron	= nvhost_flcn_finalize_poweron_t194,
	.prepare_poweroff	= nvhost_flcn_prepare_poweroff,
	.flcn_isr		= nvhost_flcn_common_isr,
	.firmware_name		= "nvhost_vic042.fw",
	.firmware_not_in_subdir = true,
	.serialize		= true,
	.push_work_done		= true,
	.resource_policy	= RESOURCE_PER_CHANNEL_INSTANCE,
	.vm_regs		= {{0x30, true}, {0x34, false} },
	.transcfg_addr		= 0x2044,
	.transcfg_val		= 0x20,
	.bwmgr_client_id	= TEGRA_BWMGR_CLIENT_VIC,
	.scaling_init		= nvhost_scale_emc_init,
	.scaling_deinit		= nvhost_scale_emc_deinit,
	.scaling_post_cb	= &nvhost_scale_emc_callback,
	.get_reloc_phys_addr	= nvhost_t194_get_reloc_phys_addr,
	.module_irq		= 1,
	.engine_cg_regs		= t19x_vic_gating_registers,
	.engine_can_cg		= true,
	.can_powergate		= true,
	.isolate_contexts	= true,
	.enable_timestamps	= flcn_enable_timestamps,
	.actmon_regs		= HOST1X_THOST_ACTMON_VIC,
	.actmon_enabled         = true,
	.actmon_irq		= 3,
	.actmon_weight_count	= 216,
	.actmon_setting_regs	= t19x_vic_actmon_registers,
	.devfreq_governor	= "userspace",
};
#endif

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_SLVSEC)
struct nvhost_device_data t19_slvsec_info = {
	.num_channels		= 1,
	.clocks			= {
		{"slvs-ec", UINT_MAX},
		{"slvs-ec-lp", UINT_MAX},
	},
	.devfs_name		= "slvs-ec",
	.class			= NV_SLVSEC_CLASS_ID,
	.autosuspend_delay      = 500,
	.finalize_poweron	= slvsec_finalize_poweron,
	.prepare_poweroff	= slvsec_prepare_poweroff,
	.poweron_reset		= true,
	.keepalive		= true,
	.serialize		= 1,
	.push_work_done		= 1,
	.can_powergate		= true,
};
#endif

#include "host1x/host1x_channel_t194.c"

static void t194_set_nvhost_chanops(struct nvhost_channel *ch)
{
	if (!ch)
		return;

	ch->ops = host1x_channel_ops;

	/* Disable gather filter in simulator */
	if (tegra_platform_is_vdk())
		ch->ops.init_gather_filter = NULL;
}

int nvhost_init_t194_channel_support(struct nvhost_master *host,
				     struct nvhost_chip_support *op)
{
	op->nvhost_dev.set_nvhost_chanops = t194_set_nvhost_chanops;

	return 0;
}

static void t194_remove_support(struct nvhost_chip_support *op)
{
	kfree(op->priv);
	op->priv = NULL;
}

#define SYNCPT_RAM_INIT_TIMEOUT_MS	1000

static void t194_init_regs(struct platform_device *pdev, bool prod)
{
	struct nvhost_gating_register *cg_regs = t19x_host1x_gating_registers;
	struct nvhost_streamid_mapping *map_regs = t19x_host1x_streamid_mapping;
	int ret = 0;
	u64 cl = 0;

	if (nvhost_dev_is_virtual(pdev) == true) {
		return;
	}

	/* Use old mapping registers on older simulator CLs */
	ret = of_property_read_u64(pdev->dev.of_node,
				   "nvidia,changelist",
				   &cl);
	if (ret == 0 && cl <= 38424879)
		map_regs = t19x_host1x_streamid_mapping_vdk_r6;

	/* Write the map registers */
	while (map_regs->host1x_offset) {
		host1x_hypervisor_writel(pdev,
					 map_regs->host1x_offset,
					 map_regs->client_offset);
		host1x_hypervisor_writel(pdev,
					 map_regs->host1x_offset + sizeof(u32),
					 map_regs->client_limit);
		map_regs++;
	}

	while (cg_regs->addr) {
		u32 val = prod ? cg_regs->prod : cg_regs->disable;

		host1x_hypervisor_writel(pdev, cg_regs->addr, val);
		cg_regs++;
	}
}

#include "host1x/host1x_cdma_t194.c"
#include "host1x/host1x_syncpt.c"
#include "host1x/host1x_syncpt_prot_t194.c"
#include "host1x/host1x_intr_t194.c"
#include "host1x/host1x_debug_t194.c"
#include "host1x/host1x_vm_t194.c"
#if IS_ENABLED(CONFIG_TEGRA_GRHOST_SCALE)
#include "host1x/host1x_actmon_t194.c"
#endif

int nvhost_init_t194_support(struct nvhost_master *host,
			     struct nvhost_chip_support *op)
{
	int err;

	op->soc_name = "tegra19x";

	/* don't worry about cleaning up on failure... "remove" does it. */
	err = nvhost_init_t194_channel_support(host, op);
	if (err)
		return err;

	op->cdma = host1x_cdma_ops;
	op->push_buffer = host1x_pushbuffer_ops;
	op->debug = host1x_debug_ops;

	host->sync_aperture = host->aperture;
	op->syncpt = host1x_syncpt_ops;
	op->intr = host1x_intr_ops;
	op->vm = host1x_vm_ops;
	op->vm.init_syncpt_interface = nvhost_syncpt_unit_interface_init;
#if IS_ENABLED(CONFIG_TEGRA_GRHOST_SCALE)
	op->actmon = host1x_actmon_ops;
#endif
	op->nvhost_dev.load_gating_regs = t194_init_regs;

	/* WAR to bugs 200094901 and 200082771: enable protection
	 * only on silicon/emulation */

	if (!tegra_platform_is_vdk()) {
		op->syncpt.reset = t194_syncpt_reset;
		op->syncpt.mark_used = t194_syncpt_mark_used;
		op->syncpt.mark_unused = t194_syncpt_mark_unused;
	}
	op->syncpt.mutex_owner = t194_syncpt_mutex_owner;

	op->remove_support = t194_remove_support;

	return 0;
}
