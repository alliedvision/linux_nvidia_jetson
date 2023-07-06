/*
 * Tegra Graphics Init for T23X Architecture Chips
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
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif

#include <soc/tegra/kfuse.h>

#include <linux/tegra_vhost.h>

#include <uapi/linux/nvhost_ioctl.h>

#include "dev.h"
#include "platform.h"
#include "class_ids.h"
#include "class_ids_t194.h"
#include "class_ids_t23x.h"

#include "nvhost_syncpt_unit_interface.h"
#include "t23x.h"
#include "host1x/host1x.h"
#if IS_ENABLED(CONFIG_TEGRA_GRHOST_ISP)
#include "isp/isp5.h"
#endif
#if IS_ENABLED(CONFIG_TEGRA_GRHOST_TSEC)
#include "tsec/tsec.h"
#include "tsec/tsec_t23x.h"
#endif
#include "flcn/flcn.h"
#if IS_ENABLED(CONFIG_TEGRA_GRHOST_NVCSI)
#include "nvcsi/nvcsi-t194.h"
#endif
#if IS_ENABLED(CONFIG_TEGRA_GRHOST_NVDEC)
#include "nvdec/nvdec.h"
#include "nvdec/nvdec_t23x.h"
#endif
#include "hardware_t23x.h"
#if IS_ENABLED(CONFIG_VIDEO_TEGRA_VI)
#include "vi/vi5.h"
#endif
#if IS_ENABLED(CONFIG_TEGRA_GRHOST_OFA)
#include "ofa/ofa.h"
#endif

#include "chip_support.h"

#include "scale_emc.h"

#include "streamid_regs.c"
#include "cg_regs.c"
#include "classid_vm_regs.c"
#include "mmio_vm_regs.c"
#include "actmon_regs.c"

#include <dt-bindings/interconnect/tegra_icc_id.h>

#define NVHOST_HAS_SUBMIT_HOST1XSTREAMID

dma_addr_t nvhost_t23x_get_reloc_phys_addr(dma_addr_t phys_addr, u32 reloc_type)
{
	if (reloc_type == NVHOST_RELOC_TYPE_BLOCK_LINEAR)
		phys_addr += BIT(39);

	return phys_addr;
}

static struct host1x_device_info host1x04_info = {
	.nb_channels	= T23X_NVHOST_NUMCHANNELS,
	.ch_base	= 0,
	.ch_limit	= T23X_NVHOST_NUMCHANNELS,
	.nb_mlocks	= NV_HOST1X_NB_MLOCKS,
	.initialize_chip_support = nvhost_init_t23x_support,
	.nb_hw_pts	= NV_HOST1X_SYNCPT_NB_PTS,
	.nb_pts		= NV_HOST1X_SYNCPT_NB_PTS,
	.pts_base	= 0,
	.pts_limit	= NV_HOST1X_SYNCPT_NB_PTS,
	.nb_syncpt_irqs	= 8,
	.syncpt_policy	= SYNCPT_PER_CHANNEL_INSTANCE,
	.channel_policy	= MAP_CHANNEL_ON_SUBMIT,
	.nb_actmons	= 1,
	.use_cross_vm_interrupts = 1,
	.resources	= {
		"guest",
		"hypervisor",
		"actmon",
		"sem-syncpt-shim",
		"common"
	},
	.nb_resources	= 5,
	.secure_cmdfifo = true,
	.syncpt_page_size = 0x10000,
	.rw_mlock_register = true,
};

struct nvhost_device_data t23x_host1x_info = {
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

struct nvhost_device_data t23x_host1x_hv_info = {
	.autosuspend_delay      = 2000,
	.private_data		= &host1x04_info,
	.finalize_poweron = nvhost_host1x_finalize_poweron,
	.prepare_poweroff = nvhost_host1x_prepare_poweroff,
};

static struct host1x_device_info host1xb04_info = {
	.nb_channels	= T23X_NVHOST_NUMCHANNELS,
	.ch_base	= 0,
	.ch_limit	= T23X_NVHOST_NUMCHANNELS,
	.nb_mlocks	= NV_HOST1X_NB_MLOCKS,
	.initialize_chip_support = nvhost_init_t23x_support,
	.nb_hw_pts	= NV_HOST1X_SYNCPT_NB_PTS,
	.nb_pts		= NV_HOST1X_SYNCPT_NB_PTS,
	.pts_base	= 0,
	.pts_limit	= NV_HOST1X_SYNCPT_NB_PTS,
	.nb_syncpt_irqs	= 8,
	.syncpt_policy	= SYNCPT_PER_CHANNEL_INSTANCE,
	.channel_policy	= MAP_CHANNEL_ON_SUBMIT,
	.use_cross_vm_interrupts = 1,
};

struct nvhost_device_data t23x_host1xb_info = {
	.clocks			= {
		{"host1x", UINT_MAX},
		{"actmon", UINT_MAX}
	},
	.private_data		= &host1xb04_info,
};

#if IS_ENABLED(CONFIG_VIDEO_TEGRA_VI)
struct nvhost_device_data t23x_vi0_thi_info = {
	.devfs_name		= "vi0-thi",
	.moduleid		= NVHOST_MODULE_VI,
};

struct nvhost_device_data t23x_vi0_info = {
	.devfs_name		= "vi0",
	.moduleid		= NVHOST_MODULE_VI,
	.clocks = {
		{"vi", UINT_MAX},
	},
	.num_ppc		= 8,
	.aggregate_constraints	= nvhost_vi5_aggregate_constraints,
	.pre_virt_init		= vi5_priv_early_probe,
	.post_virt_init		= vi5_priv_late_probe,
};

struct nvhost_device_data t23x_vi1_thi_info = {
	.devfs_name		= "vi1-thi",
	.moduleid		= NVHOST_MODULE_VI2,
};

struct nvhost_device_data t23x_vi1_info = {
	.devfs_name		= "vi1",
	.moduleid		= NVHOST_MODULE_VI2,
	.clocks = {
		{"vi", UINT_MAX},
	},
	.num_ppc		= 8,
	.aggregate_constraints	= nvhost_vi5_aggregate_constraints,
	.pre_virt_init		= vi5_priv_early_probe,
	.post_virt_init		= vi5_priv_late_probe,
};
#endif

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_NVCSI)
struct nvhost_device_data t23x_nvcsi_info = {
	.moduleid		= NVHOST_MODULE_NVCSI,
	.clocks			= {
		{"nvcsi", UINT_MAX},
	},
	.devfs_name		= "nvcsi",
	.pre_virt_init		= t194_nvcsi_early_probe,
	.post_virt_init		= t194_nvcsi_late_probe,
};
#endif

#ifdef CONFIG_TEGRA_GRHOST_ISP
struct nvhost_device_data t23x_isp_thi_info = {
	.devfs_name		= "isp-thi",
	.moduleid		= NVHOST_MODULE_ISP,
};

struct nvhost_device_data t23x_isp5_info = {
	.devfs_name		= "isp",
	.moduleid		= NVHOST_MODULE_ISP,
	.clocks			= {
		{"isp", UINT_MAX},
	},
	.pre_virt_init		= isp5_priv_early_probe,
	.post_virt_init		= isp5_priv_late_probe,
};
#endif

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_NVENC)
struct nvhost_device_data t23x_msenc_info = {
	.version		= NVHOST_ENCODE_FLCN_VER(8, 0),
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
	.firmware_name		= "nvhost_nvenc080.fw",
	.firmware_not_in_subdir = true,
	.serialize		= true,
	.push_work_done		= true,
	.resource_policy	= RESOURCE_PER_CHANNEL_INSTANCE,
	.vm_regs		= {{0x30, true}, {0x34, false} },
	.transcfg_addr		= 0x1844,
	.transcfg_val		= 0x20,
	.icc_id			= TEGRA_ICC_NVENC,
	.get_reloc_phys_addr	= nvhost_t23x_get_reloc_phys_addr,
	.engine_cg_regs		= t23x_nvenc_gating_registers,
	.engine_can_cg		= false,
	.can_powergate		= true,
	.isolate_contexts	= true,
	.enable_timestamps	= flcn_enable_timestamps,
	.scaling_init		= nvhost_scale_emc_init,
	.scaling_deinit		= nvhost_scale_emc_deinit,
	.scaling_post_cb	= &nvhost_scale_emc_callback,
	.actmon_regs		= HOST1X_THOST_ACTMON_NVENC,
	.actmon_enabled         = true,
	.actmon_irq		= 2,
	.actmon_weight_count	= 216,
	.actmon_setting_regs	= t23x_nvenc_actmon_registers,
	.devfreq_governor	= "userspace",
};
#endif

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_NVDEC)
struct nvhost_device_data t23x_nvdec_info = {
	.version		= NVHOST_ENCODE_NVDEC_VER(5, 0),
	.devfs_name		= "nvdec",
	.modulemutexes		= {NV_HOST1X_MLOCK_ID_NVDEC},
	.class			= NV_NVDEC_CLASS_ID,
	.autosuspend_delay      = 500,
	.clocks			= {
		{"nvdec", UINT_MAX},
		{"kfuse", 0, 0},
		{"efuse", 0, 0},
		{"tsec_pka", 204000000, 0},
		{"emc", 0,
		 NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		 TEGRA_SET_EMC_FLOOR}
	},
	.poweron_reset		= true,
	.finalize_poweron	= nvhost_nvdec_finalize_poweron_t23x,
	.prepare_poweroff	= nvhost_nvdec_prepare_poweroff_t23x,
	.moduleid		= NVHOST_MODULE_NVDEC,
	.ctrl_ops		= &tegra_nvdec_ctrl_ops,
	.num_channels		= 1,
	.serialize		= true,
	.push_work_done		= true,
	.resource_policy	= RESOURCE_PER_CHANNEL_INSTANCE,
	.vm_regs		= {{0x30, true}, {0x34, false} },
	.transcfg_addr		= 0x4e44,
	.transcfg_val		= 0x20,
	.icc_id			= TEGRA_ICC_NVDEC,
	.get_reloc_phys_addr	= nvhost_t23x_get_reloc_phys_addr,
	.engine_cg_regs		= t23x_nvdec_gating_registers,
	.engine_can_cg		= false,
	.can_powergate		= true,
	.isolate_contexts	= true,
	.enable_riscv_boot	= true,
	.riscv_desc_bin		= "nvhost_nvdec050_desc_dev.bin",
	.riscv_image_bin	= "nvhost_nvdec050_sim.fw",
	.scaling_init		= nvhost_scale_emc_init,
	.scaling_deinit		= nvhost_scale_emc_deinit,
	.scaling_post_cb	= &nvhost_scale_emc_callback,
	.actmon_regs		= HOST1X_THOST_ACTMON_NVDEC,
	.actmon_enabled         = true,
	.actmon_irq		= 4,
	.actmon_weight_count	= 216,
	.actmon_setting_regs	= t23x_nvdec_actmon_registers,
	.devfreq_governor	= "userspace",
};
#endif

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_NVJPG)
struct nvhost_device_data t23x_nvjpg_info = {
	.version		= NVHOST_ENCODE_FLCN_VER(1, 3),
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
	.firmware_name		= "nvhost_nvjpg013.fw",
	.serialize		= true,
	.push_work_done		= true,
	.resource_policy	= RESOURCE_PER_CHANNEL_INSTANCE,
	.vm_regs		= {{0x30, true}, {0x34, false} },
	.transcfg_addr		= 0x1444,
	.transcfg_val		= 0x20,
	.icc_id			= TEGRA_ICC_NVJPG_0,
	.engine_cg_regs		= t23x_nvjpg_gating_registers,
	.engine_can_cg		= false,
	.can_powergate		= true,
	.isolate_contexts	= true,
};

struct nvhost_device_data t23x_nvjpg1_info = {
	.version		= NVHOST_ENCODE_FLCN_VER(1, 3),
	.devfs_name		= "nvjpg1",
	.modulemutexes		= {NV_HOST1X_MLOCK_ID_NVJPG1},
	.class			= NV_NVJPG1_CLASS_ID,
	.autosuspend_delay      = 500,
	.clocks			= {
		{"nvjpg", UINT_MAX},
		{"emc", 0,
		 NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		 TEGRA_SET_EMC_SHARED_BW}
	},
	.poweron_reset		= true,
	.finalize_poweron	= nvhost_flcn_finalize_poweron_t194,
	.moduleid		= NVHOST_MODULE_NVJPG1,
	.num_channels		= 1,
	.firmware_name		= "nvhost_nvjpg013.fw",
	.serialize		= true,
	.push_work_done		= true,
	.resource_policy	= RESOURCE_PER_CHANNEL_INSTANCE,
	.vm_regs		= {{0x30, true}, {0x34, false} },
	.transcfg_addr		= 0x1444,
	.transcfg_val		= 0x20,
	.icc_id			= TEGRA_ICC_NVJPG_1,
	.engine_cg_regs		= t23x_nvjpg_gating_registers,
	.engine_can_cg		= false,
	.can_powergate		= true,
	.isolate_contexts	= true,
};
#endif

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_OFA)
struct nvhost_device_data t23x_ofa_info = {
	.version		= NVHOST_ENCODE_FLCN_VER(1, 2),
	.devfs_name		= "ofa",
	.modulemutexes		= {NV_HOST1X_MLOCK_ID_OFA},
	.class			= NV_OFA_CLASS_ID,
	.autosuspend_delay      = 500,
	.clocks			= {
		{"ofa", UINT_MAX},
		{"emc", 0,
		 NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		 TEGRA_SET_EMC_SHARED_BW}
	},
	.poweron_reset		= true,
	.finalize_poweron	= nvhost_flcn_finalize_poweron_t194,
	.memory_init		= ofa_safety_ram_init,
	.moduleid		= NVHOST_MODULE_OFA,
	.num_channels		= 1,
	.firmware_name		= "nvhost_ofa012.fw",
	.firmware_not_in_subdir = true,
	.serialize		= true,
	.push_work_done		= true,
	.resource_policy	= RESOURCE_PER_CHANNEL_INSTANCE,
	.vm_regs		= {{0x30, true}, {0x34, false} },
	.transcfg_addr		= 0x1444,
	.transcfg_val		= 0x20,
	.icc_id			= TEGRA_ICC_OFAA,
	.get_reloc_phys_addr	= nvhost_t23x_get_reloc_phys_addr,
	.engine_cg_regs		= t23x_ofa_gating_registers,
	.engine_can_cg		= false,
	.can_powergate		= true,
	.isolate_contexts	= true,
	.enable_timestamps	= flcn_enable_timestamps,
};
#endif

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_TSEC)
struct nvhost_device_data t23x_tsec_info = {
	.num_channels		= 1,
	.devfs_name		= "tsec",
	.version		= NVHOST_ENCODE_TSEC_VER(1, 0),
	.modulemutexes		= {NV_HOST1X_MLOCK_ID_TSEC},
	.class			= NV_TSEC_CLASS_ID,
	.clocks			= {
		{"tsec", 192000000},
		{"efuse", 0, 0},
		{"tsec_pka", 204000000, 0},
		{"emc", 0,
		 NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		 TEGRA_SET_EMC_FLOOR}
	},
	.autosuspend_delay      = 500,
	.keepalive		= true,
	.moduleid		= NVHOST_MODULE_TSEC,
	.poweron_reset		= true,
	.finalize_poweron	= nvhost_tsec_finalize_poweron_t23x,
	.prepare_poweroff	= nvhost_tsec_prepare_poweroff_t23x,
	.serialize		= true,
	.push_work_done		= true,
	.resource_policy	= RESOURCE_PER_CHANNEL_INSTANCE,
	.vm_regs		= {{0x30, true}, {0x34, false} },
	.transcfg_addr		= 0x1644,
	.transcfg_val		= 0x20,
	.icc_id			= TEGRA_ICC_TSEC,
	.engine_cg_regs		= t23x_tsec_gating_registers,
	.engine_can_cg		= false,
	.can_powergate		= false,
	.isolate_contexts	= true,
	.enable_riscv_boot	= true,
	.riscv_desc_bin		= "nvhost_tsec_desc.fw",
	.riscv_image_bin	= "nvhost_tsec_riscv.fw",
};
#endif

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_VIC)
struct nvhost_device_data t23x_vic_info = {
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
	.icc_id			= TEGRA_ICC_VIC,
	.scaling_init		= nvhost_scale_emc_init,
	.scaling_deinit		= nvhost_scale_emc_deinit,
	.scaling_post_cb	= &nvhost_scale_emc_callback,
	.get_reloc_phys_addr	= nvhost_t23x_get_reloc_phys_addr,
	.module_irq		= 1,
	.engine_cg_regs		= t23x_vic_gating_registers,
	.engine_can_cg		= false,
	.can_powergate		= true,
	.isolate_contexts	= true,
	.actmon_regs		= HOST1X_THOST_ACTMON_VIC,
	.actmon_enabled         = true,
	.actmon_irq		= 3,
	.actmon_weight_count	= 216,
	.actmon_setting_regs	= t23x_vic_actmon_registers,
	.devfreq_governor	= "userspace",
};
#endif

#include "host1x/host1x_channel_t194.c"

static void t23x_set_nvhost_chanops(struct nvhost_channel *ch)
{
	if (!ch)
		return;

	ch->ops = host1x_channel_ops;

	/* Disable gather filter in simulator */
	if (tegra_platform_is_vdk())
		ch->ops.init_gather_filter = NULL;
}

int nvhost_init_t23x_channel_support(struct nvhost_master *host,
				     struct nvhost_chip_support *op)
{
	op->nvhost_dev.set_nvhost_chanops = t23x_set_nvhost_chanops;

	return 0;
}

static void t23x_remove_support(struct nvhost_chip_support *op)
{
	kfree(op->priv);
	op->priv = NULL;
}

#define SYNCPT_RAM_INIT_TIMEOUT_MS	1000

static void t23x_init_gating_regs(struct platform_device *pdev, bool prod)
{
	struct nvhost_gating_register *cg_regs = t23x_host1x_gating_registers;

	if (nvhost_dev_is_virtual(pdev) == true) {
		return;
	}

	while (cg_regs->addr) {
		u32 val = prod ? cg_regs->prod : cg_regs->disable;

		host1x_common_writel(pdev, cg_regs->addr, val);
		cg_regs++;
	}

}

static void t23x_init_map_regs(struct platform_device *pdev)
{
	struct nvhost_streamid_mapping *map_regs = t23x_host1x_streamid_mapping;
	u32 i;

	/* Write the client streamid map registers */
	while (map_regs->host1x_offset) {
		host1x_hypervisor_writel(pdev,
					 map_regs->host1x_offset,
					 map_regs->client_offset);
		host1x_hypervisor_writel(pdev,
					 map_regs->host1x_offset + sizeof(u32),
					 map_regs->client_limit);
		map_regs++;
	}

	/* Allow all VMs to access all streamid */
	for (i = 0; i < t23x_strmid_vm_regs_nb; i++) {
		host1x_hypervisor_writel(pdev,
					t23x_host1x_strmid_vm_r + (i * 4),
					0xff);
	}

	/* Update common_thost_classid registers */
	for (i = 0; i < ARRAY_SIZE(t23x_host1x_classid_vm_r); i++) {
		host1x_hypervisor_writel(pdev, t23x_host1x_classid_vm_r[i],
					0xff);
	}

	/* Update relevant MMIO-to-VM mapping table entries to VM1 (0x1) */
	for (i = 0; i < ARRAY_SIZE(t23x_host1x_mmio_vm_r); i++) {
		host1x_hypervisor_writel(pdev, t23x_host1x_mmio_vm_r[i],
					0x1);
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

static void host1x08_intr_resume(struct nvhost_intr *intr)
{
	struct nvhost_master *dev = intr_to_dev(intr);
	const int nb_pts = nvhost_syncpt_nb_hw_pts(&dev->syncpt);
	const int nb_syncpt_irqs = nvhost_syncpt_nb_irqs(&dev->syncpt);
	const int pts_per_irq = nb_pts / nb_syncpt_irqs;
	const int routed_equally = nb_syncpt_irqs * pts_per_irq;
	unsigned int i;

	host1x_intr_ops.resume(intr);

	/*
	 * Configure Host1x to send syncpoint threshold interrupts to the
	 * interrupt lines as below:
	 * - Available syncpoints are equally distributed among 8 interrupt
	 *   lines
	 * - First 1/8 syncpoints shall trigger 1st line
	 * - Second 1/8 syncpoints shall trigger 2nd line
	 * - Similarly, for all other lines
	 * - Left-over(if any) are mapped to last line
	 */
	for (i = 0; i < nb_pts; i++) {
		const int reg = host1x_common_vm1_syncpt_intr_dest_vm_r() +
				i * 4;

		if (i < routed_equally)
			host1x_writel(dev->dev, reg, (i / pts_per_irq));
		else
			host1x_writel(dev->dev, reg, (nb_syncpt_irqs-1));
	}
}

int nvhost_init_t23x_support(struct nvhost_master *host,
			     struct nvhost_chip_support *op)
{
	int err;

	op->soc_name = "tegra23x";

	/* don't worry about cleaning up on failure... "remove" does it. */
	err = nvhost_init_t23x_channel_support(host, op);
	if (err)
		return err;

	op->cdma = host1x_cdma_ops;
	op->push_buffer = host1x_pushbuffer_ops;
	op->debug = host1x_debug_ops;

	host->sync_aperture = host->aperture;
	op->syncpt = host1x_syncpt_ops;
	op->intr = host1x_intr_ops;
	op->intr.resume = host1x08_intr_resume;

	op->vm = host1x_vm_ops;
	op->vm.init_syncpt_interface = nvhost_syncpt_unit_interface_init;
#if IS_ENABLED(CONFIG_TEGRA_GRHOST_SCALE)
	op->actmon = host1x_actmon_ops;
#endif
	op->nvhost_dev.load_gating_regs = t23x_init_gating_regs;
	op->nvhost_dev.load_map_regs = t23x_init_map_regs;

	op->syncpt.reset = t194_syncpt_reset;
	op->syncpt.mark_used = t194_syncpt_mark_used;
	op->syncpt.mark_unused = t194_syncpt_mark_unused;

	op->syncpt.mutex_owner = t194_syncpt_mutex_owner;

	op->remove_support = t23x_remove_support;

	return 0;
}
