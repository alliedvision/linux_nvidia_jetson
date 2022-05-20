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
 */

#include <soc/tegra/fuse.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/of_address.h>
#include <linux/dma-buf.h>

#include <tegra-soc-hwpm-log.h>
#include <hal/tegra-soc-hwpm-structures.h>
#include <uapi/linux/tegra-soc-hwpm-uapi.h>
#include <hal/t234/t234_soc_hwpm_perfmon_dt.h>
#include <hal/t234/t234_soc_hwpm_ip_map.h>
#include <hal/t234/t234_soc_hwpm_init.h>
#include <hal/t234/hw/t234_addr_map_soc_hwpm.h>

void __iomem *t234_dt_apertures[T234_SOC_HWPM_NUM_DT_APERTURES];
struct tegra_soc_hwpm_ip_ops t234_ip_info[T234_SOC_HWPM_NUM_DT_APERTURES];

/*
 * Normally there is a 1-to-1 mapping between an MMIO aperture and a
 * hwpm_resource_aperture struct. But the PMA MMIO aperture is used in
 * multiple hwpm_resource_aperture structs. Therefore, we have to share the fake
 * register array between these hwpm_resource_aperture structs. This is why we
 * have to define the fake register array globally. For all other 1-to-1
 * mapping apertures the fake register arrays are directly embedded inside the
 * hwpm_resource_aperture structs.
 */
u32 *t234_pma_fake_regs;

struct hwpm_resource t234_hwpm_resources[TERGA_SOC_HWPM_NUM_RESOURCES] = {
	[TEGRA_SOC_HWPM_RESOURCE_VI] = {
		.reserved = false,
		.map_size = ARRAY_SIZE(t234_vi_map),
		.map = t234_vi_map,
	},
	[TEGRA_SOC_HWPM_RESOURCE_ISP] = {
		.reserved = false,
		.map_size = ARRAY_SIZE(t234_isp_map),
		.map = t234_isp_map,
	},
	[TEGRA_SOC_HWPM_RESOURCE_VIC] = {
		.reserved = false,
		.map_size = ARRAY_SIZE(t234_vic_map),
		.map = t234_vic_map,
	},
	[TEGRA_SOC_HWPM_RESOURCE_OFA] = {
		.reserved = false,
		.map_size = ARRAY_SIZE(t234_ofa_map),
		.map = t234_ofa_map,
	},
	[TEGRA_SOC_HWPM_RESOURCE_PVA] = {
		.reserved = false,
		.map_size = ARRAY_SIZE(t234_pva_map),
		.map = t234_pva_map,
	},
	[TEGRA_SOC_HWPM_RESOURCE_NVDLA] = {
		.reserved = false,
		.map_size = ARRAY_SIZE(t234_nvdla_map),
		.map = t234_nvdla_map,
	},
	[TEGRA_SOC_HWPM_RESOURCE_MGBE] = {
		.reserved = false,
		.map_size = ARRAY_SIZE(t234_mgbe_map),
		.map = t234_mgbe_map,
	},
	[TEGRA_SOC_HWPM_RESOURCE_SCF] = {
		.reserved = false,
		.map_size = ARRAY_SIZE(t234_scf_map),
		.map = t234_scf_map,
	},
	[TEGRA_SOC_HWPM_RESOURCE_NVDEC] = {
		.reserved = false,
		.map_size = ARRAY_SIZE(t234_nvdec_map),
		.map = t234_nvdec_map,
	},
	[TEGRA_SOC_HWPM_RESOURCE_NVENC] = {
		.reserved = false,
		.map_size = ARRAY_SIZE(t234_nvenc_map),
		.map = t234_nvenc_map,
	},
	[TEGRA_SOC_HWPM_RESOURCE_PCIE] = {
		.reserved = false,
		.map_size = ARRAY_SIZE(t234_pcie_map),
		.map = t234_pcie_map,
	},
	[TEGRA_SOC_HWPM_RESOURCE_DISPLAY] = {
		.reserved = false,
		.map_size = ARRAY_SIZE(t234_display_map),
		.map = t234_display_map,
	},
	[TEGRA_SOC_HWPM_RESOURCE_MSS_CHANNEL] = {
		.reserved = false,
		.map_size = ARRAY_SIZE(t234_mss_channel_map),
		.map = t234_mss_channel_map,
	},
	[TEGRA_SOC_HWPM_RESOURCE_MSS_GPU_HUB] = {
		.reserved = false,
		.map_size = ARRAY_SIZE(t234_mss_gpu_hub_map),
		.map = t234_mss_gpu_hub_map,
	},
	[TEGRA_SOC_HWPM_RESOURCE_MSS_ISO_NISO_HUBS] = {
		.reserved = false,
		.map_size = ARRAY_SIZE(t234_mss_iso_niso_hub_map),
		.map = t234_mss_iso_niso_hub_map,
	},
	[TEGRA_SOC_HWPM_RESOURCE_MSS_MCF] = {
		.reserved = false,
		.map_size = ARRAY_SIZE(t234_mss_mcf_map),
		.map = t234_mss_mcf_map,
	},
	[TEGRA_SOC_HWPM_RESOURCE_PMA] = {
		.reserved = false,
		.map_size = ARRAY_SIZE(t234_pma_map),
		.map = t234_pma_map,
	},
	[TEGRA_SOC_HWPM_RESOURCE_CMD_SLICE_RTR] = {
		.reserved = false,
		.map_size = ARRAY_SIZE(t234_cmd_slice_rtr_map),
		.map = t234_cmd_slice_rtr_map,
	},
};

void __iomem **t234_soc_hwpm_init_dt_apertures(void)
{
	return t234_dt_apertures;
}

struct tegra_soc_hwpm_ip_ops *t234_soc_hwpm_init_ip_ops_info(void)
{
	return t234_ip_info;
}

bool t234_soc_hwpm_is_perfmon(u32 dt_aperture)
{
	return IS_PERFMON(dt_aperture);
}

u64 t234_soc_hwpm_get_perfmon_base(u32 dt_aperture)
{
	if (t234_soc_hwpm_is_perfmon(dt_aperture)) {
		return PERFMON_BASE(dt_aperture);
	} else if (dt_aperture == T234_SOC_HWPM_PMA_DT) {
		return addr_map_pma_base_r();
	} else if (dt_aperture == T234_SOC_HWPM_RTR_DT) {
		return addr_map_rtr_base_r();
	} else {
		return 0ULL;
	}
}

bool t234_soc_hwpm_is_dt_aperture(u32 dt_aperture)
{
	return (dt_aperture < T234_SOC_HWPM_NUM_DT_APERTURES);
}

u32 t234_soc_hwpm_get_ip_aperture(struct tegra_soc_hwpm *hwpm,
	u64 phys_address, u64 *ip_base_addr)
{
	enum t234_soc_hwpm_dt_aperture aperture =
					TEGRA_SOC_HWPM_DT_APERTURE_INVALID;

	if ((phys_address >= addr_map_vi_thi_base_r()) &&
		(phys_address <= addr_map_vi_thi_limit_r())) {
		aperture = T234_SOC_HWPM_VI0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_vi_thi_base_r();
		}
	} else if ((phys_address >= addr_map_vi2_thi_base_r()) &&
		(phys_address <= addr_map_vi2_thi_limit_r())) {
		aperture = T234_SOC_HWPM_VI1_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_vi2_thi_base_r();
		}
	} else if ((phys_address >= addr_map_isp_thi_base_r()) &&
		(phys_address <= addr_map_isp_thi_limit_r())) {
		aperture = T234_SOC_HWPM_ISP0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_isp_thi_base_r();
		}
	} else if ((phys_address >= addr_map_vic_base_r()) &&
		(phys_address <= addr_map_vic_limit_r())) {
		aperture = T234_SOC_HWPM_VICA0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_vic_base_r();
		}
	} else if ((phys_address >= addr_map_ofa_base_r()) &&
		(phys_address <= addr_map_ofa_limit_r())) {
		aperture = T234_SOC_HWPM_OFAA0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_ofa_base_r();
		}
	} else if ((phys_address >= addr_map_pva0_pm_base_r()) &&
		(phys_address <= addr_map_pva0_pm_limit_r())) {
		aperture = T234_SOC_HWPM_PVAV0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_pva0_pm_base_r();
		}
	} else if ((phys_address >= addr_map_nvdla0_base_r()) &&
		(phys_address <= addr_map_nvdla0_limit_r())) {
		aperture = T234_SOC_HWPM_NVDLAB0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_nvdla0_base_r();
		}
	} else if ((phys_address >= addr_map_nvdla1_base_r()) &&
		(phys_address <= addr_map_nvdla1_limit_r())) {
		aperture = T234_SOC_HWPM_NVDLAB1_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_nvdla1_base_r();
		}
	} else if ((phys_address >= addr_map_disp_base_r()) &&
		(phys_address <= addr_map_disp_limit_r())) {
		aperture = T234_SOC_HWPM_NVDISPLAY0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_disp_base_r();
		}
	} else if ((phys_address >= addr_map_mgbe0_base_r()) &&
		(phys_address <= addr_map_mgbe0_limit_r())) {
		aperture = T234_SOC_HWPM_MGBE0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mgbe0_base_r();
		}
	} else if ((phys_address >= addr_map_mgbe1_base_r()) &&
		(phys_address <= addr_map_mgbe1_limit_r())) {
		aperture = T234_SOC_HWPM_MGBE1_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mgbe1_base_r();
		}
	} else if ((phys_address >= addr_map_mgbe2_base_r()) &&
		(phys_address <= addr_map_mgbe2_limit_r())) {
		aperture = T234_SOC_HWPM_MGBE2_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mgbe2_base_r();
		}
	} else if ((phys_address >= addr_map_mgbe3_base_r()) &&
		(phys_address <= addr_map_mgbe3_limit_r())) {
		aperture = T234_SOC_HWPM_MGBE3_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mgbe3_base_r();
		}
	} else if ((phys_address >= addr_map_nvdec_base_r()) &&
		(phys_address <= addr_map_nvdec_limit_r())) {
		aperture = T234_SOC_HWPM_NVDECA0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_nvdec_base_r();
		}
	} else if ((phys_address >= addr_map_nvenc_base_r()) &&
		(phys_address <= addr_map_nvenc_limit_r())) {
		aperture = T234_SOC_HWPM_NVENCA0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_nvenc_base_r();
		}
	} else if ((phys_address >= addr_map_mss_nvlink_1_base_r()) &&
		(phys_address <= addr_map_mss_nvlink_1_limit_r())) {
		aperture = T234_SOC_HWPM_MSSNVLHSH0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mss_nvlink_1_base_r();
		}
	} else if ((phys_address >= addr_map_mss_nvlink_2_base_r()) &&
		(phys_address <= addr_map_mss_nvlink_2_limit_r())) {
		aperture = T234_SOC_HWPM_MSSNVLHSH0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mss_nvlink_2_base_r();
		}
	} else if ((phys_address >= addr_map_mss_nvlink_3_base_r()) &&
		(phys_address <= addr_map_mss_nvlink_3_limit_r())) {
		aperture = T234_SOC_HWPM_MSSNVLHSH0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mss_nvlink_3_base_r();
		}
	} else if ((phys_address >= addr_map_mss_nvlink_4_base_r()) &&
		(phys_address <= addr_map_mss_nvlink_4_limit_r())) {
		aperture = T234_SOC_HWPM_MSSNVLHSH0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mss_nvlink_4_base_r();
		}
	} else if ((phys_address >= addr_map_mss_nvlink_5_base_r()) &&
		(phys_address <= addr_map_mss_nvlink_5_limit_r())) {
		aperture = T234_SOC_HWPM_MSSNVLHSH0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mss_nvlink_5_base_r();
		}
	} else if ((phys_address >= addr_map_mss_nvlink_6_base_r()) &&
		(phys_address <= addr_map_mss_nvlink_6_limit_r())) {
		aperture = T234_SOC_HWPM_MSSNVLHSH0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mss_nvlink_6_base_r();
		}
	} else if ((phys_address >= addr_map_mss_nvlink_7_base_r()) &&
		(phys_address <= addr_map_mss_nvlink_7_limit_r())) {
		aperture = T234_SOC_HWPM_MSSNVLHSH0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mss_nvlink_7_base_r();
		}
	} else if ((phys_address >= addr_map_mss_nvlink_8_base_r()) &&
		(phys_address <= addr_map_mss_nvlink_8_limit_r())) {
		aperture = T234_SOC_HWPM_MSSNVLHSH0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mss_nvlink_8_base_r();
		}
	} else if ((phys_address >= addr_map_pcie_c0_ctl_base_r()) &&
		(phys_address <= addr_map_pcie_c0_ctl_limit_r())) {
		aperture = T234_SOC_HWPM_PCIE0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_pcie_c0_ctl_base_r();
		}
	} else if ((phys_address >= addr_map_pcie_c1_ctl_base_r()) &&
		(phys_address <= addr_map_pcie_c1_ctl_limit_r())) {
		aperture = T234_SOC_HWPM_PCIE1_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_pcie_c1_ctl_base_r();
		}
	} else if ((phys_address >= addr_map_pcie_c2_ctl_base_r()) &&
		(phys_address <= addr_map_pcie_c2_ctl_limit_r())) {
		aperture = T234_SOC_HWPM_PCIE2_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_pcie_c2_ctl_base_r();
		}
	} else if ((phys_address >= addr_map_pcie_c3_ctl_base_r()) &&
		(phys_address <= addr_map_pcie_c3_ctl_limit_r())) {
		aperture = T234_SOC_HWPM_PCIE3_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_pcie_c3_ctl_base_r();
		}
	} else if ((phys_address >= addr_map_pcie_c4_ctl_base_r()) &&
		(phys_address <= addr_map_pcie_c4_ctl_limit_r())) {
		aperture = T234_SOC_HWPM_PCIE4_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_pcie_c4_ctl_base_r();
		}
	} else if ((phys_address >= addr_map_pcie_c5_ctl_base_r()) &&
		(phys_address <= addr_map_pcie_c5_ctl_limit_r())) {
		aperture = T234_SOC_HWPM_PCIE5_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_pcie_c5_ctl_base_r();
		}
	} else if ((phys_address >= addr_map_pcie_c6_ctl_base_r()) &&
		(phys_address <= addr_map_pcie_c6_ctl_limit_r())) {
		aperture = T234_SOC_HWPM_PCIE6_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_pcie_c6_ctl_base_r();
		}
	} else if ((phys_address >= addr_map_pcie_c7_ctl_base_r()) &&
		(phys_address <= addr_map_pcie_c7_ctl_limit_r())) {
		aperture = T234_SOC_HWPM_PCIE7_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_pcie_c7_ctl_base_r();
		}
	} else if ((phys_address >= addr_map_pcie_c8_ctl_base_r()) &&
		(phys_address <= addr_map_pcie_c8_ctl_limit_r())) {
		aperture = T234_SOC_HWPM_PCIE8_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_pcie_c8_ctl_base_r();
		}
	} else if ((phys_address >= addr_map_pcie_c9_ctl_base_r()) &&
		(phys_address <= addr_map_pcie_c9_ctl_limit_r())) {
		aperture = T234_SOC_HWPM_PCIE9_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_pcie_c9_ctl_base_r();
		}
	} else if ((phys_address >= addr_map_pcie_c10_ctl_base_r()) &&
		(phys_address <= addr_map_pcie_c10_ctl_limit_r())) {
		aperture = T234_SOC_HWPM_PCIE10_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_pcie_c10_ctl_base_r();
		}
	} else if ((phys_address >= addr_map_mc0_base_r()) &&
		(phys_address <= addr_map_mc0_limit_r())) {
		aperture = T234_SOC_HWPM_MSSCHANNELPARTA0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mc0_base_r();
		}
	} else if ((phys_address >= addr_map_mc1_base_r()) &&
		(phys_address <= addr_map_mc1_limit_r())) {
		aperture = T234_SOC_HWPM_MSSCHANNELPARTA1_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mc1_base_r();
		}
	} else if ((phys_address >= addr_map_mc2_base_r()) &&
			(phys_address <= addr_map_mc2_limit_r())) {
		aperture = T234_SOC_HWPM_MSSCHANNELPARTA2_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mc2_base_r();
		}
	} else if ((phys_address >= addr_map_mc3_base_r()) &&
		(phys_address <= addr_map_mc3_limit_r())) {
		aperture = T234_SOC_HWPM_MSSCHANNELPARTA3_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mc3_base_r();
		}
	} else if ((phys_address >= addr_map_mc4_base_r()) &&
		(phys_address <= addr_map_mc4_limit_r())) {
		aperture = T234_SOC_HWPM_MSSCHANNELPARTB0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mc4_base_r();
		}
	} else if ((phys_address >= addr_map_mc5_base_r()) &&
		(phys_address <= addr_map_mc5_limit_r())) {
		aperture = T234_SOC_HWPM_MSSCHANNELPARTB1_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mc5_base_r();
		}
	} else if ((phys_address >= addr_map_mc6_base_r()) &&
			(phys_address <= addr_map_mc6_limit_r())) {
		aperture = T234_SOC_HWPM_MSSCHANNELPARTB2_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mc6_base_r();
		}
	} else if ((phys_address >= addr_map_mc7_base_r()) &&
		(phys_address <= addr_map_mc7_limit_r())) {
		aperture = T234_SOC_HWPM_MSSCHANNELPARTB3_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mc7_base_r();
		}
	} else if ((phys_address >= addr_map_mc8_base_r()) &&
		(phys_address <= addr_map_mc8_limit_r())) {
		aperture = T234_SOC_HWPM_MSSCHANNELPARTC0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mc8_base_r();
		}
	} else if ((phys_address >= addr_map_mc9_base_r()) &&
		(phys_address <= addr_map_mc9_limit_r())) {
		aperture = T234_SOC_HWPM_MSSCHANNELPARTC1_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mc9_base_r();
		}
	} else if ((phys_address >= addr_map_mc10_base_r()) &&
			(phys_address <= addr_map_mc10_limit_r())) {
		aperture = T234_SOC_HWPM_MSSCHANNELPARTC2_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mc10_base_r();
		}
	} else if ((phys_address >= addr_map_mc11_base_r()) &&
		(phys_address <= addr_map_mc11_limit_r())) {
		aperture = T234_SOC_HWPM_MSSCHANNELPARTC3_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mc11_base_r();
		}
	} else if ((phys_address >= addr_map_mc4_base_r()) &&
		(phys_address <= addr_map_mc12_limit_r())) {
		aperture = T234_SOC_HWPM_MSSCHANNELPARTD0_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mc12_base_r();
		}
	} else if ((phys_address >= addr_map_mc13_base_r()) &&
		(phys_address <= addr_map_mc13_limit_r())) {
		aperture = T234_SOC_HWPM_MSSCHANNELPARTD1_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mc13_base_r();
		}
	} else if ((phys_address >= addr_map_mc14_base_r()) &&
			(phys_address <= addr_map_mc14_limit_r())) {
		aperture = T234_SOC_HWPM_MSSCHANNELPARTD2_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mc14_base_r();
		}
	} else if ((phys_address >= addr_map_mc15_base_r()) &&
		(phys_address <= addr_map_mc15_limit_r())) {
		aperture = T234_SOC_HWPM_MSSCHANNELPARTD3_PERFMON_DT;
		if (ip_base_addr) {
			*ip_base_addr = addr_map_mc15_base_r();
		}
	}

	return (u32)aperture;
}

int t234_soc_hwpm_fs_info_init(struct tegra_soc_hwpm *hwpm)
{
	hwpm->hwpm_resources = t234_hwpm_resources;

	if (tegra_platform_is_vsp()) {
		/* Static IP instances as per VSP netlist */
		hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_VIC] = 0x1;
		hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_MSS_CHANNEL] = 0xF;
		hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_MSS_GPU_HUB] = 0x1;
		hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_MSS_ISO_NISO_HUBS] = 0x1;
		hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_MSS_MCF] = 0x1;
		hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_MSS_NVLINK] = 0x1;
	}
	if (tegra_platform_is_silicon()) {
		/* Static IP instances corresponding to silicon */
		// hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_VI] = 0x3;
		hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_ISP] = 0x1;
		hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_VIC] = 0x1;
		hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_OFA] = 0x1;
		hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_PVA] = 0x1;
		hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_NVDLA] = 0x3;
		// hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_MGBE] = 0xF;
		hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_SCF] = 0x1;
		hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_NVDEC] = 0x1;
		hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_NVENC] = 0x1;
		// hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_PCIE] = 0x32;
		// hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_DISPLAY] = 0x1;
		hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_MSS_CHANNEL] = 0xFFFF;
		hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_MSS_GPU_HUB] = 0x1;
		hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_MSS_ISO_NISO_HUBS] = 0x1;
		hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_MSS_MCF] = 0x1;
		hwpm->ip_fs_info[TEGRA_SOC_HWPM_IP_MSS_NVLINK] = 0x1;
	}
	return 0;
}

int t234_soc_hwpm_pma_rtr_map(struct tegra_soc_hwpm *hwpm)
{
	struct resource *res = NULL;
	u64 num_regs = 0ULL;

	hwpm->dt_apertures[T234_SOC_HWPM_PMA_DT] =
				of_iomap(hwpm->np, T234_SOC_HWPM_PMA_DT);
	if (!hwpm->dt_apertures[T234_SOC_HWPM_PMA_DT]) {
		tegra_soc_hwpm_err("Couldn't map the PMA aperture");
		return -ENOMEM;
	}
	res = platform_get_resource(hwpm->pdev,
					IORESOURCE_MEM,
					T234_SOC_HWPM_PMA_DT);
	if ((!res) || (res->start == 0) || (res->end == 0)) {
		tegra_soc_hwpm_err("Invalid resource for PMA");
		return -ENOMEM;
	}
	t234_pma_map[1].start_pa = res->start;
	t234_pma_map[1].end_pa = res->end;
	t234_cmd_slice_rtr_map[0].start_pa = res->start;
	t234_cmd_slice_rtr_map[0].end_pa = res->end;
	if (hwpm->fake_registers_enabled) {
		num_regs = (res->end + 1 - res->start) / sizeof(*t234_pma_fake_regs);
		t234_pma_fake_regs = (u32 *)kzalloc(sizeof(*t234_pma_fake_regs) * num_regs,
						GFP_KERNEL);
		if (!t234_pma_fake_regs) {
			tegra_soc_hwpm_err("Couldn't allocate memory for PMA"
					   " fake registers");
			return -ENOMEM;
		}
		t234_pma_map[1].fake_registers = t234_pma_fake_regs;
		t234_cmd_slice_rtr_map[0].fake_registers = t234_pma_fake_regs;
	}

	hwpm->hwpm_resources[TEGRA_SOC_HWPM_RESOURCE_PMA].reserved = true;

	hwpm->dt_apertures[T234_SOC_HWPM_RTR_DT] =
				of_iomap(hwpm->np, T234_SOC_HWPM_RTR_DT);
	if (!hwpm->dt_apertures[T234_SOC_HWPM_RTR_DT]) {
		tegra_soc_hwpm_err("Couldn't map the RTR aperture");
		return -ENOMEM;
	}
	res = platform_get_resource(hwpm->pdev,
				    IORESOURCE_MEM,
				    T234_SOC_HWPM_RTR_DT);
	if ((!res) || (res->start == 0) || (res->end == 0)) {
		tegra_soc_hwpm_err("Invalid resource for RTR");
		return -ENOMEM;
	}
	t234_cmd_slice_rtr_map[1].start_pa = res->start;
	t234_cmd_slice_rtr_map[1].end_pa = res->end;
	if (hwpm->fake_registers_enabled) {
		num_regs = (res->end + 1 - res->start) /
				sizeof(*t234_cmd_slice_rtr_map[1].fake_registers);
		t234_cmd_slice_rtr_map[1].fake_registers =
			(u32 *)kzalloc(
				sizeof(*t234_cmd_slice_rtr_map[1].fake_registers) *
									num_regs,
				GFP_KERNEL);
		if (!t234_cmd_slice_rtr_map[1].fake_registers) {
			tegra_soc_hwpm_err("Couldn't allocate memory for RTR"
					   " fake registers");
			return -ENOMEM;
		}
	}
	hwpm->hwpm_resources[TEGRA_SOC_HWPM_RESOURCE_CMD_SLICE_RTR].reserved = true;
	return 0;
}

int t234_soc_hwpm_pma_rtr_unmap(struct tegra_soc_hwpm *hwpm)
{
	if (hwpm->dt_apertures[T234_SOC_HWPM_PMA_DT]) {
		iounmap(hwpm->dt_apertures[T234_SOC_HWPM_PMA_DT]);
		hwpm->dt_apertures[T234_SOC_HWPM_PMA_DT] = NULL;
	}
	t234_pma_map[1].start_pa = 0;
	t234_pma_map[1].end_pa = 0;
	t234_cmd_slice_rtr_map[0].start_pa = 0;
	t234_cmd_slice_rtr_map[0].end_pa = 0;
	if (t234_pma_fake_regs) {
		kfree(t234_pma_fake_regs);
		t234_pma_fake_regs = NULL;
		t234_pma_map[1].fake_registers = NULL;
		t234_cmd_slice_rtr_map[0].fake_registers = NULL;
	}
	hwpm->hwpm_resources[TEGRA_SOC_HWPM_RESOURCE_PMA].reserved = false;

	if (hwpm->dt_apertures[T234_SOC_HWPM_RTR_DT]) {
		iounmap(hwpm->dt_apertures[T234_SOC_HWPM_RTR_DT]);
		hwpm->dt_apertures[T234_SOC_HWPM_RTR_DT] = NULL;
	}
	t234_cmd_slice_rtr_map[1].start_pa = 0;
	t234_cmd_slice_rtr_map[1].end_pa = 0;
	if (t234_cmd_slice_rtr_map[1].fake_registers) {
		kfree(t234_cmd_slice_rtr_map[1].fake_registers);
		t234_cmd_slice_rtr_map[1].fake_registers = NULL;
	}
	hwpm->hwpm_resources[TEGRA_SOC_HWPM_RESOURCE_CMD_SLICE_RTR].reserved = false;

	return 0;
}


int t234_soc_hwpm_disable_pma_triggers(struct tegra_soc_hwpm *hwpm)
{
	int err = 0;
	int ret = 0;
	bool timeout = false;
	u32 field_mask = 0;
	u32 field_val = 0;

	/* Disable PMA triggers */
	err = reg_rmw(hwpm, NULL, T234_SOC_HWPM_PMA_DT,
		pmasys_trigger_config_user_r(0) - addr_map_pma_base_r(),
		pmasys_trigger_config_user_pma_pulse_m(),
		pmasys_trigger_config_user_pma_pulse_disable_f(),
		false, false);
	RELEASE_FAIL("Unable to disable PMA triggers");

	hwpm_writel(hwpm, T234_SOC_HWPM_PMA_DT,
		pmasys_sys_trigger_start_mask_r() - addr_map_pma_base_r(), 0);
	hwpm_writel(hwpm, T234_SOC_HWPM_PMA_DT,
		pmasys_sys_trigger_start_maskb_r() - addr_map_pma_base_r(), 0);
	hwpm_writel(hwpm, T234_SOC_HWPM_PMA_DT,
		pmasys_sys_trigger_stop_mask_r() - addr_map_pma_base_r(), 0);
	hwpm_writel(hwpm, T234_SOC_HWPM_PMA_DT,
		pmasys_sys_trigger_stop_maskb_r() - addr_map_pma_base_r(), 0);

	/* Wait for PERFMONs, ROUTER, and PMA to idle */
	timeout = HWPM_TIMEOUT(pmmsys_sys0router_perfmonstatus_merged_v(
		hwpm_readl(hwpm, T234_SOC_HWPM_RTR_DT,
			pmmsys_sys0router_perfmonstatus_r() -
			addr_map_rtr_base_r())) == 0U,
		"NV_PERF_PMMSYS_SYS0ROUTER_PERFMONSTATUS_MERGED_EMPTY");
	if (timeout && ret == 0) {
		ret = -EIO;
	}

	timeout = HWPM_TIMEOUT(pmmsys_sys0router_enginestatus_status_v(
		hwpm_readl(hwpm, T234_SOC_HWPM_RTR_DT,
			pmmsys_sys0router_enginestatus_r() -
			addr_map_rtr_base_r())) ==
		pmmsys_sys0router_enginestatus_status_empty_v(),
		"NV_PERF_PMMSYS_SYS0ROUTER_ENGINESTATUS_STATUS_EMPTY");
	if (timeout && ret == 0) {
		ret = -EIO;
	}

	field_mask = pmasys_enginestatus_status_m() |
		     pmasys_enginestatus_rbufempty_m();
	field_val = pmasys_enginestatus_status_empty_f() |
		pmasys_enginestatus_rbufempty_empty_f();
	timeout = HWPM_TIMEOUT((hwpm_readl(hwpm, T234_SOC_HWPM_PMA_DT,
			pmasys_enginestatus_r() -
			addr_map_pma_base_r()) & field_mask) == field_val,
		"NV_PERF_PMASYS_ENGINESTATUS");
	if (timeout && ret == 0) {
		ret = -EIO;
	}

	hwpm->hwpm_resources[TEGRA_SOC_HWPM_RESOURCE_PMA].reserved = false;
	hwpm->hwpm_resources[TEGRA_SOC_HWPM_RESOURCE_CMD_SLICE_RTR].reserved = false;

	return ret;
}

int t234_soc_hwpm_disable_slcg(struct tegra_soc_hwpm *hwpm)
{
	int ret;
	u32 field_mask = 0U;
	u32 field_val = 0U;

	ret = reg_rmw(hwpm, NULL, T234_SOC_HWPM_PMA_DT,
		pmasys_cg2_r() - addr_map_pma_base_r(),
		pmasys_cg2_slcg_m(), pmasys_cg2_slcg_disabled_f(),
		false, false);
	if (ret < 0) {
		tegra_soc_hwpm_err("Unable to disable PMA SLCG");
		ret = -EIO;
		goto fail;
	}

	field_mask = pmmsys_sys0router_cg2_slcg_perfmon_m() |
		pmmsys_sys0router_cg2_slcg_router_m() |
		pmmsys_sys0router_cg2_slcg_m();
	field_val = pmmsys_sys0router_cg2_slcg_perfmon_disabled_f() |
		pmmsys_sys0router_cg2_slcg_router_disabled_f() |
		pmmsys_sys0router_cg2_slcg_disabled_f();
	ret = reg_rmw(hwpm, NULL, T234_SOC_HWPM_RTR_DT,
		pmmsys_sys0router_cg2_r() - addr_map_rtr_base_r(),
		field_mask, field_val, false, false);
	if (ret < 0) {
		tegra_soc_hwpm_err("Unable to disable ROUTER SLCG");
		ret = -EIO;
		goto fail;
	}

	/* Program PROD values */
	ret = reg_rmw(hwpm, NULL, T234_SOC_HWPM_PMA_DT,
		pmasys_controlb_r() - addr_map_pma_base_r(),
		pmasys_controlb_coalesce_timeout_cycles_m(),
		pmasys_controlb_coalesce_timeout_cycles__prod_f(),
		false, false);
	if (ret < 0) {
		tegra_soc_hwpm_err("Unable to program PROD value");
		ret = -EIO;
		goto fail;
	}

	ret = reg_rmw(hwpm, NULL, T234_SOC_HWPM_PMA_DT,
		pmasys_channel_config_user_r(0) - addr_map_pma_base_r(),
		pmasys_channel_config_user_coalesce_timeout_cycles_m(),
		pmasys_channel_config_user_coalesce_timeout_cycles__prod_f(),
		false, false);
	if (ret < 0) {
		tegra_soc_hwpm_err("Unable to program PROD value");
		ret = -EIO;
		goto fail;
	}

	goto success;

fail:
	t234_soc_hwpm_pma_rtr_unmap(hwpm);
success:
	return ret;
}

int t234_soc_hwpm_enable_slcg(struct tegra_soc_hwpm *hwpm)
{
	int err, ret = 0;
	u32 field_mask = 0U;
	u32 field_val = 0U;

	err = reg_rmw(hwpm, NULL, T234_SOC_HWPM_PMA_DT,
		pmasys_cg2_r() - addr_map_pma_base_r(),
		pmasys_cg2_slcg_m(),
		pmasys_cg2_slcg_enabled_f(), false, false);
	RELEASE_FAIL("Unable to enable PMA SLCG");

	field_mask = pmmsys_sys0router_cg2_slcg_perfmon_m() |
		pmmsys_sys0router_cg2_slcg_router_m() |
		pmmsys_sys0router_cg2_slcg_m();
	field_val = pmmsys_sys0router_cg2_slcg_perfmon__prod_f() |
		pmmsys_sys0router_cg2_slcg_router__prod_f() |
		pmmsys_sys0router_cg2_slcg__prod_f();
	err = reg_rmw(hwpm, NULL, T234_SOC_HWPM_RTR_DT,
		pmmsys_sys0router_cg2_r() - addr_map_rtr_base_r(),
		field_mask, field_val, false, false);
	RELEASE_FAIL("Unable to enable ROUTER SLCG");

	return err;
}

static bool t234_soc_hwpm_ip_reg_check(struct hwpm_resource_aperture *aperture,
			    u64 phys_addr, bool use_absolute_base,
			    u64 *updated_pa)
{
	u64 start_pa = 0ULL;
	u64 end_pa = 0ULL;

	if (!aperture) {
		tegra_soc_hwpm_err("Aperture is NULL");
		return false;
	}

	if (use_absolute_base) {
		start_pa = aperture->start_abs_pa;
		end_pa = aperture->end_abs_pa;
	} else {
		start_pa = aperture->start_pa;
		end_pa = aperture->end_pa;
	}

	if ((phys_addr >= start_pa) && (phys_addr <= end_pa)) {
		tegra_soc_hwpm_dbg("Found aperture:"
			" phys_addr(0x%llx), aperture(0x%llx - 0x%llx)",
			phys_addr, start_pa, end_pa);
		*updated_pa = phys_addr - start_pa + aperture->start_pa;
		return true;
	}
	return false;
}

/*
 * Find an aperture in which phys_addr lies. If check_reservation is true, then
 * we also have to do a allowlist check.
 */
struct hwpm_resource_aperture *t234_soc_hwpm_find_aperture(
		struct tegra_soc_hwpm *hwpm, u64 phys_addr,
		bool use_absolute_base, bool check_reservation,
		u64 *updated_pa)
{
	struct hwpm_resource_aperture *aperture = NULL;
	int res_idx = 0;
	int aprt_idx = 0;

	for (res_idx = 0; res_idx < TERGA_SOC_HWPM_NUM_RESOURCES; res_idx++) {
		if (check_reservation && !hwpm->hwpm_resources[res_idx].reserved)
			continue;

		for (aprt_idx = 0;
		     aprt_idx < hwpm->hwpm_resources[res_idx].map_size;
		     aprt_idx++) {
			aperture = &(hwpm->hwpm_resources[res_idx].map[aprt_idx]);
			if (check_reservation) {
				if (t234_soc_hwpm_allowlist_check(aperture, phys_addr,
					use_absolute_base, updated_pa)) {
					return aperture;
				}
			} else {
				if (t234_soc_hwpm_ip_reg_check(aperture, phys_addr,
					use_absolute_base, updated_pa)) {
					return aperture;
				}
			}
		}
	}

	tegra_soc_hwpm_err("Unable to find aperture: phys(0x%llx)", phys_addr);
	return NULL;
}

void t234_soc_hwpm_zero_alist_regs(struct tegra_soc_hwpm *hwpm,
		struct hwpm_resource_aperture *aperture)
{
	u32 alist_idx = 0U;

	for (alist_idx = 0; alist_idx < aperture->alist_size; alist_idx++) {
		if (aperture->alist[alist_idx].zero_at_init) {
			ioctl_writel(hwpm, aperture,
				aperture->start_pa +
				aperture->alist[alist_idx].reg_offset, 0);
		}
	}
}

int t234_soc_hwpm_update_allowlist(struct tegra_soc_hwpm *hwpm,
				 void *ioctl_struct)
{
	int err = 0;
	int res_idx = 0;
	int aprt_idx = 0;
	u32 full_alist_idx = 0;
	u32 aprt_alist_idx = 0;
	long pinned_pages = 0;
	long page_idx = 0;
	u64 alist_buf_size = 0;
	u64 num_pages = 0;
	u64 *full_alist_u64 = NULL;
	void *full_alist = NULL;
	struct page **pages = NULL;
	struct hwpm_resource_aperture *aperture = NULL;
	struct tegra_soc_hwpm_query_allowlist *query_allowlist =
			(struct tegra_soc_hwpm_query_allowlist *)ioctl_struct;
	unsigned long user_va = (unsigned long)(query_allowlist->allowlist);
	unsigned long offset = user_va & ~PAGE_MASK;

	if (hwpm->full_alist_size < 0) {
		tegra_soc_hwpm_err("Invalid allowlist size");
		return -EINVAL;
	}
	alist_buf_size = hwpm->full_alist_size * sizeof(struct allowlist);

	/* Memory map user buffer into kernel address space */
	num_pages = DIV_ROUND_UP(offset + alist_buf_size, PAGE_SIZE);
	pages = (struct page **)kzalloc(sizeof(*pages) * num_pages, GFP_KERNEL);
	if (!pages) {
		tegra_soc_hwpm_err("Couldn't allocate memory for pages array");
		err = -ENOMEM;
		goto alist_unmap;
	}
	pinned_pages = get_user_pages(user_va & PAGE_MASK, num_pages, 0,
				pages, NULL);
	if (pinned_pages != num_pages) {
		tegra_soc_hwpm_err("Requested %llu pages / Got %ld pages",
				num_pages, pinned_pages);
		err = -ENOMEM;
		goto alist_unmap;
	}
	full_alist = vmap(pages, num_pages, VM_MAP, PAGE_KERNEL);
	if (!full_alist) {
		tegra_soc_hwpm_err("Couldn't map allowlist buffer into"
				   " kernel address space");
		err = -ENOMEM;
		goto alist_unmap;
	}
	full_alist_u64 = (u64 *)(full_alist + offset);

	/* Fill in allowlist buffer */
	for (res_idx = 0, full_alist_idx = 0;
	     res_idx < TERGA_SOC_HWPM_NUM_RESOURCES;
	     res_idx++) {
		if (!(hwpm->hwpm_resources[res_idx].reserved))
			continue;
		tegra_soc_hwpm_dbg("Found reserved IP(%d)", res_idx);

		for (aprt_idx = 0;
		     aprt_idx < hwpm->hwpm_resources[res_idx].map_size;
		     aprt_idx++) {
			aperture = &(hwpm->hwpm_resources[res_idx].map[aprt_idx]);
			if (aperture->alist) {
				for (aprt_alist_idx = 0;
				     aprt_alist_idx < aperture->alist_size;
				     aprt_alist_idx++, full_alist_idx++) {
					full_alist_u64[full_alist_idx] =
						aperture->start_pa +
						aperture->alist[aprt_alist_idx].reg_offset;
				}
			} else {
				tegra_soc_hwpm_err("NULL allowlist in aperture(0x%llx - 0x%llx)",
						   aperture->start_pa,
						   aperture->end_pa);
			}
		}
	}

alist_unmap:
	if (full_alist)
		vunmap(full_alist);
	if (pinned_pages > 0) {
		for (page_idx = 0; page_idx < pinned_pages; page_idx++) {
			set_page_dirty(pages[page_idx]);
			put_page(pages[page_idx]);
		}
	}
	if (pages) {
		kfree(pages);
	}

	return err;
}

bool t234_soc_hwpm_allowlist_check(struct hwpm_resource_aperture *aperture,
			    u64 phys_addr, bool use_absolute_base,
			    u64 *updated_pa)
{
	u32 idx = 0U;
	u64 start_pa = 0ULL;

	if (!aperture) {
		tegra_soc_hwpm_err("Aperture is NULL");
		return false;
	}
	if (!aperture->alist) {
		tegra_soc_hwpm_err("NULL allowlist in dt_aperture(%d)",
				aperture->dt_aperture);
		return false;
	}

	start_pa = use_absolute_base ? aperture->start_abs_pa :
					aperture->start_pa;

	for (idx = 0; idx < aperture->alist_size; idx++) {
		if (phys_addr == start_pa + aperture->alist[idx].reg_offset) {
			*updated_pa = aperture->start_pa +
						aperture->alist[idx].reg_offset;
			return true;
		}
	}

	return false;
}

void t234_soc_hwpm_get_full_allowlist(struct tegra_soc_hwpm *hwpm)
{
	int res_idx = 0;
	int aprt_idx = 0;
	struct hwpm_resource_aperture *aperture = NULL;

	for (res_idx = 0; res_idx < TERGA_SOC_HWPM_NUM_RESOURCES; res_idx++) {
		if (!(hwpm->hwpm_resources[res_idx].reserved))
			continue;
		tegra_soc_hwpm_dbg("Found reserved IP(%d)", res_idx);

		for (aprt_idx = 0;
		     aprt_idx < hwpm->hwpm_resources[res_idx].map_size;
		     aprt_idx++) {
			aperture = &(hwpm->hwpm_resources[res_idx].map[aprt_idx]);
			if (aperture->alist) {
				hwpm->full_alist_size += aperture->alist_size;
			} else {
				tegra_soc_hwpm_err(
				"NULL allowlist in aperture(0x%llx - 0x%llx)",
					aperture->start_pa, aperture->end_pa);
			}
		}
	}
}
