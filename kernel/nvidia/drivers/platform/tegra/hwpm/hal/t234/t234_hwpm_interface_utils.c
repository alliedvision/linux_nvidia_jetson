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

#include <linux/slab.h>
#include <uapi/linux/tegra-soc-hwpm-uapi.h>

#include <tegra_hwpm_log.h>
#include <tegra_hwpm_common.h>
#include <tegra_hwpm.h>
#include <tegra_hwpm_static_analysis.h>
#include <hal/t234/t234_hwpm_init.h>
#include <hal/t234/t234_hwpm_internal.h>

static struct tegra_soc_hwpm_chip t234_chip_info = {
	.chip_ips = NULL,

	/* HALs */
	.is_ip_active = t234_hwpm_is_ip_active,
	.is_resource_active = t234_hwpm_is_resource_active,

	.get_rtr_int_idx = t234_get_rtr_int_idx,
	.get_ip_max_idx = t234_get_ip_max_idx,

	.extract_ip_ops = t234_hwpm_extract_ip_ops,
	.force_enable_ips = t234_hwpm_force_enable_ips,
	.validate_current_config = t234_hwpm_validate_current_config,
	.get_fs_info = t234_hwpm_get_fs_info,
	.get_resource_info = t234_hwpm_get_resource_info,

	.init_prod_values = t234_hwpm_init_prod_values,
	.disable_slcg = t234_hwpm_disable_slcg,
	.enable_slcg = t234_hwpm_enable_slcg,

	.reserve_rtr = tegra_hwpm_reserve_rtr,
	.release_rtr = tegra_hwpm_release_rtr,

	.perfmon_enable = t234_hwpm_perfmon_enable,
	.perfmon_disable = t234_hwpm_perfmon_disable,
	.perfmux_disable = t234_hwpm_perfmux_disable,
	.disable_triggers = t234_hwpm_disable_triggers,

	.disable_mem_mgmt = t234_hwpm_disable_mem_mgmt,
	.enable_mem_mgmt = t234_hwpm_enable_mem_mgmt,
	.invalidate_mem_config = t234_hwpm_invalidate_mem_config,
	.stream_mem_bytes = t234_hwpm_stream_mem_bytes,
	.disable_pma_streaming = t234_hwpm_disable_pma_streaming,
	.update_mem_bytes_get_ptr = t234_hwpm_update_mem_bytes_get_ptr,
	.get_mem_bytes_put_ptr = t234_hwpm_get_mem_bytes_put_ptr,
	.membuf_overflow_status = t234_hwpm_membuf_overflow_status,

	.get_alist_buf_size = t234_hwpm_get_alist_buf_size,
	.zero_alist_regs = t234_hwpm_zero_alist_regs,
	.copy_alist = t234_hwpm_copy_alist,
	.check_alist = t234_hwpm_check_alist,

	.release_sw_setup = tegra_hwpm_release_sw_setup,
};

static bool t234_hwpm_validate_hals(struct tegra_soc_hwpm *hwpm)
{
	if (hwpm->active_chip->is_ip_active == NULL) {
		tegra_hwpm_err(hwpm, "is_ip_active HAL uninitialized");
		return false;
	}

	if (hwpm->active_chip->is_resource_active == NULL) {
		tegra_hwpm_err(hwpm, "is_resource_active HAL uninitialized");
		return false;
	}

	if (hwpm->active_chip->get_rtr_int_idx == NULL) {
		tegra_hwpm_err(hwpm, "get_rtr_int_idx HAL uninitialized");
		return false;
	}

	if (hwpm->active_chip->get_ip_max_idx == NULL) {
		tegra_hwpm_err(hwpm, "get_ip_max_idx HAL uninitialized");
		return false;
	}

	if (hwpm->active_chip->extract_ip_ops == NULL) {
		tegra_hwpm_err(hwpm, "extract_ip_ops uninitialized");
		return false;
	}

	if (hwpm->active_chip->force_enable_ips == NULL) {
		tegra_hwpm_err(hwpm, "force_enable_ips uninitialized");
		return false;
	}

	if (hwpm->active_chip->validate_current_config == NULL) {
		tegra_hwpm_err(hwpm, "validate_current_config uninitialized");
		return false;
	}

	if (hwpm->active_chip->get_fs_info == NULL) {
		tegra_hwpm_err(hwpm, "get_fs_info uninitialized");
		return false;
	}

	if (hwpm->active_chip->get_resource_info == NULL) {
		tegra_hwpm_err(hwpm, "get_resource_info uninitialized");
		return false;
	}

	if (hwpm->active_chip->init_prod_values == NULL) {
		tegra_hwpm_err(hwpm, "init_prod_values uninitialized");
		return false;
	}

	if (hwpm->active_chip->disable_slcg == NULL) {
		tegra_hwpm_err(hwpm, "disable_slcg uninitialized");
		return false;
	}

	if (hwpm->active_chip->enable_slcg == NULL) {
		tegra_hwpm_err(hwpm, "enable_slcg uninitialized");
		return false;
	}

	if (hwpm->active_chip->reserve_rtr == NULL) {
		tegra_hwpm_err(hwpm, "reserve_rtr uninitialized");
		return false;
	}

	if (hwpm->active_chip->release_rtr == NULL) {
		tegra_hwpm_err(hwpm, "release_rtr uninitialized");
		return false;
	}

	if (hwpm->active_chip->perfmon_enable == NULL) {
		tegra_hwpm_err(hwpm, "perfmon_enable HAL uninitialized");
		return false;
	}

	if (hwpm->active_chip->perfmon_disable == NULL) {
		tegra_hwpm_err(hwpm, "perfmon_disable HAL uninitialized");
		return false;
	}

	if (hwpm->active_chip->perfmux_disable == NULL) {
		tegra_hwpm_err(hwpm, "perfmux_disable HAL uninitialized");
		return false;
	}

	if (hwpm->active_chip->disable_triggers == NULL) {
		tegra_hwpm_err(hwpm, "disable_triggers uninitialized");
		return false;
	}

	if (hwpm->active_chip->disable_mem_mgmt == NULL) {
		tegra_hwpm_err(hwpm, "disable_mem_mgmt HAL uninitialized");
		return false;
	}

	if (hwpm->active_chip->enable_mem_mgmt == NULL) {
		tegra_hwpm_err(hwpm, "enable_mem_mgmt HAL uninitialized");
		return false;
	}

	if (hwpm->active_chip->invalidate_mem_config == NULL) {
		tegra_hwpm_err(hwpm, "invalidate_mem_config HAL uninitialized");
		return false;
	}

	if (hwpm->active_chip->stream_mem_bytes == NULL) {
		tegra_hwpm_err(hwpm, "stream_mem_bytes uninitialized");
		return false;
	}

	if (hwpm->active_chip->disable_pma_streaming == NULL) {
		tegra_hwpm_err(hwpm, "disable_pma_streaming uninitialized");
		return false;
	}

	if (hwpm->active_chip->update_mem_bytes_get_ptr == NULL) {
		tegra_hwpm_err(hwpm, "update_mem_bytes_get_ptr uninitialized");
		return false;
	}

	if (hwpm->active_chip->get_mem_bytes_put_ptr == NULL) {
		tegra_hwpm_err(hwpm, "get_mem_bytes_put_ptr uninitialized");
		return false;
	}

	if (hwpm->active_chip->membuf_overflow_status == NULL) {
		tegra_hwpm_err(hwpm, "membuf_overflow_status uninitialized");
		return false;
	}

	if (hwpm->active_chip->get_alist_buf_size == NULL) {
		tegra_hwpm_err(hwpm, "alist_buf_size uninitialized");
		return false;
	}

	if (hwpm->active_chip->zero_alist_regs == NULL) {
		tegra_hwpm_err(hwpm, "zero_alist_regs HAL uninitialized");
		return false;
	}

	if (hwpm->active_chip->copy_alist == NULL) {
		tegra_hwpm_err(hwpm, "copy_alist HAL uninitialized");
		return false;
	}

	if (hwpm->active_chip->check_alist == NULL) {
		tegra_hwpm_err(hwpm, "check_alist uninitialized");
		return false;
	}

	if (hwpm->active_chip->release_sw_setup == NULL) {
		tegra_hwpm_err(hwpm, "release_sw_setup uninitialized");
		return false;
	}

	return true;
}

bool t234_hwpm_is_ip_active(struct tegra_soc_hwpm *hwpm,
	u32 ip_enum, u32 *config_ip_index)
{
	u32 config_ip = TEGRA_SOC_HWPM_IP_INACTIVE;

	switch (ip_enum) {
	case TEGRA_SOC_HWPM_IP_VI:
#if defined(CONFIG_SOC_HWPM_IP_VI)
		config_ip = T234_HWPM_IP_VI;
#endif
		break;
	case TEGRA_SOC_HWPM_IP_ISP:
#if defined(CONFIG_SOC_HWPM_IP_ISP)
		config_ip = T234_HWPM_IP_ISP;
#endif
		break;
	case TEGRA_SOC_HWPM_IP_VIC:
#if defined(CONFIG_SOC_HWPM_IP_VIC)
		config_ip = T234_HWPM_IP_VIC;
#endif
		break;
	case TEGRA_SOC_HWPM_IP_OFA:
#if defined(CONFIG_SOC_HWPM_IP_OFA)
		config_ip = T234_HWPM_IP_OFA;
#endif
		break;
	case TEGRA_SOC_HWPM_IP_PVA:
#if defined(CONFIG_SOC_HWPM_IP_PVA)
		config_ip = T234_HWPM_IP_PVA;
#endif
		break;
	case TEGRA_SOC_HWPM_IP_NVDLA:
#if defined(CONFIG_SOC_HWPM_IP_NVDLA)
		config_ip = T234_HWPM_IP_NVDLA;
#endif
		break;
	case TEGRA_SOC_HWPM_IP_MGBE:
#if defined(CONFIG_SOC_HWPM_IP_MGBE)
		config_ip = T234_HWPM_IP_MGBE;
#endif
		break;
	case TEGRA_SOC_HWPM_IP_SCF:
#if defined(CONFIG_SOC_HWPM_IP_SCF)
		config_ip = T234_HWPM_IP_SCF;
#endif
		break;
	case TEGRA_SOC_HWPM_IP_NVDEC:
#if defined(CONFIG_SOC_HWPM_IP_NVDEC)
		config_ip = T234_HWPM_IP_NVDEC;
#endif
		break;
	case TEGRA_SOC_HWPM_IP_NVENC:
#if defined(CONFIG_SOC_HWPM_IP_NVENC)
		config_ip = T234_HWPM_IP_NVENC;
#endif
		break;
	case TEGRA_SOC_HWPM_IP_PCIE:
#if defined(CONFIG_SOC_HWPM_IP_PCIE)
		config_ip = T234_HWPM_IP_PCIE;
#endif
		break;
	case TEGRA_SOC_HWPM_IP_DISPLAY:
#if defined(CONFIG_SOC_HWPM_IP_DISPLAY)
		config_ip = T234_HWPM_IP_DISPLAY;
#endif
		break;
	case TEGRA_SOC_HWPM_IP_MSS_CHANNEL:
#if defined(CONFIG_SOC_HWPM_IP_MSS_CHANNEL)
		config_ip = T234_HWPM_IP_MSS_CHANNEL;
#endif
		break;
	case TEGRA_SOC_HWPM_IP_MSS_GPU_HUB:
#if defined(CONFIG_SOC_HWPM_IP_MSS_GPU_HUB)
		config_ip = T234_HWPM_IP_MSS_GPU_HUB;
#endif
		break;
	case TEGRA_SOC_HWPM_IP_MSS_ISO_NISO_HUBS:
#if defined(CONFIG_SOC_HWPM_IP_MSS_ISO_NISO_HUBS)
		config_ip = T234_HWPM_IP_MSS_ISO_NISO_HUBS;
#endif
		break;
	case TEGRA_SOC_HWPM_IP_MSS_MCF:
#if defined(CONFIG_SOC_HWPM_IP_MSS_MCF)
		config_ip = T234_HWPM_IP_MSS_MCF;
#endif
		break;
	default:
		tegra_hwpm_err(hwpm,
			"Queried enum tegra_soc_hwpm_ip %d invalid", ip_enum);
		break;
	}

	*config_ip_index = config_ip;
	return (config_ip != TEGRA_SOC_HWPM_IP_INACTIVE);
}

bool t234_hwpm_is_resource_active(struct tegra_soc_hwpm *hwpm,
	u32 res_index, u32 *config_ip_index)
{
	u32 config_ip = TEGRA_SOC_HWPM_IP_INACTIVE;

	switch (res_index) {
	case TEGRA_SOC_HWPM_RESOURCE_VI:
#if defined(CONFIG_SOC_HWPM_IP_VI)
		config_ip = T234_HWPM_IP_VI;
#endif
		break;
	case TEGRA_SOC_HWPM_RESOURCE_ISP:
#if defined(CONFIG_SOC_HWPM_IP_ISP)
		config_ip = T234_HWPM_IP_ISP;
#endif
		break;
	case TEGRA_SOC_HWPM_RESOURCE_VIC:
#if defined(CONFIG_SOC_HWPM_IP_VIC)
		config_ip = T234_HWPM_IP_VIC;
#endif
		break;
	case TEGRA_SOC_HWPM_RESOURCE_OFA:
#if defined(CONFIG_SOC_HWPM_IP_OFA)
		config_ip = T234_HWPM_IP_OFA;
#endif
		break;
	case TEGRA_SOC_HWPM_RESOURCE_PVA:
#if defined(CONFIG_SOC_HWPM_IP_PVA)
		config_ip = T234_HWPM_IP_PVA;
#endif
		break;
	case TEGRA_SOC_HWPM_RESOURCE_NVDLA:
#if defined(CONFIG_SOC_HWPM_IP_NVDLA)
		config_ip = T234_HWPM_IP_NVDLA;
#endif
		break;
	case TEGRA_SOC_HWPM_RESOURCE_MGBE:
#if defined(CONFIG_SOC_HWPM_IP_MGBE)
		config_ip = T234_HWPM_IP_MGBE;
#endif
		break;
	case TEGRA_SOC_HWPM_RESOURCE_SCF:
#if defined(CONFIG_SOC_HWPM_IP_SCF)
		config_ip = T234_HWPM_IP_SCF;
#endif
		break;
	case TEGRA_SOC_HWPM_RESOURCE_NVDEC:
#if defined(CONFIG_SOC_HWPM_IP_NVDEC)
		config_ip = T234_HWPM_IP_NVDEC;
#endif
		break;
	case TEGRA_SOC_HWPM_RESOURCE_NVENC:
#if defined(CONFIG_SOC_HWPM_IP_NVENC)
		config_ip = T234_HWPM_IP_NVENC;
#endif
		break;
	case TEGRA_SOC_HWPM_RESOURCE_PCIE:
#if defined(CONFIG_SOC_HWPM_IP_PCIE)
		config_ip = T234_HWPM_IP_PCIE;
#endif
		break;
	case TEGRA_SOC_HWPM_RESOURCE_DISPLAY:
#if defined(CONFIG_SOC_HWPM_IP_DISPLAY)
		config_ip = T234_HWPM_IP_DISPLAY;
#endif
		break;
	case TEGRA_SOC_HWPM_RESOURCE_MSS_CHANNEL:
#if defined(CONFIG_SOC_HWPM_IP_MSS_CHANNEL)
		config_ip = T234_HWPM_IP_MSS_CHANNEL;
#endif
		break;
	case TEGRA_SOC_HWPM_RESOURCE_MSS_GPU_HUB:
#if defined(CONFIG_SOC_HWPM_IP_MSS_GPU_HUB)
		config_ip = T234_HWPM_IP_MSS_GPU_HUB;
#endif
		break;
	case TEGRA_SOC_HWPM_RESOURCE_MSS_ISO_NISO_HUBS:
#if defined(CONFIG_SOC_HWPM_IP_MSS_ISO_NISO_HUBS)
		config_ip = T234_HWPM_IP_MSS_ISO_NISO_HUBS;
#endif
		break;
	case TEGRA_SOC_HWPM_RESOURCE_MSS_MCF:
#if defined(CONFIG_SOC_HWPM_IP_MSS_MCF)
		config_ip = T234_HWPM_IP_MSS_MCF;
#endif
		break;
	case TEGRA_SOC_HWPM_RESOURCE_PMA:
		config_ip = T234_HWPM_IP_PMA;
		break;
	case TEGRA_SOC_HWPM_RESOURCE_CMD_SLICE_RTR:
		config_ip = T234_HWPM_IP_RTR;
		break;
	default:
		tegra_hwpm_err(hwpm, "Queried resource %d invalid",
			res_index);
		break;
	}

	*config_ip_index = config_ip;
	return (config_ip != TEGRA_SOC_HWPM_IP_INACTIVE);
}

u32 t234_get_rtr_int_idx(struct tegra_soc_hwpm *hwpm)
{
	return T234_HWPM_IP_RTR;
}

u32 t234_get_ip_max_idx(struct tegra_soc_hwpm *hwpm)
{
	return T234_HWPM_IP_MAX;
}

int t234_hwpm_init_chip_info(struct tegra_soc_hwpm *hwpm)
{
	struct hwpm_ip **t234_active_ip_info;

	/* Allocate array of pointers to hold active IP structures */
	t234_chip_info.chip_ips =
		kzalloc(sizeof(struct hwpm_ip *) * T234_HWPM_IP_MAX,
						GFP_KERNEL);

	/* Add active chip structure link to hwpm super-structure */
	hwpm->active_chip = &t234_chip_info;

	/* Temporary pointer to make below assignments legible */
	t234_active_ip_info = t234_chip_info.chip_ips;

	t234_active_ip_info[T234_HWPM_IP_PMA] = &t234_hwpm_ip_pma;
	t234_active_ip_info[T234_HWPM_IP_RTR] = &t234_hwpm_ip_rtr;

#if defined(CONFIG_SOC_HWPM_IP_DISPLAY)
	t234_active_ip_info[T234_HWPM_IP_DISPLAY] = &t234_hwpm_ip_display;
#endif
#if defined(CONFIG_SOC_HWPM_IP_ISP)
	t234_active_ip_info[T234_HWPM_IP_ISP] = &t234_hwpm_ip_isp;
#endif
#if defined(CONFIG_SOC_HWPM_IP_MGBE)
	t234_active_ip_info[T234_HWPM_IP_MGBE] = &t234_hwpm_ip_mgbe;
#endif
#if defined(CONFIG_SOC_HWPM_IP_MSS_CHANNEL)
	t234_active_ip_info[T234_HWPM_IP_MSS_CHANNEL] =
			&t234_hwpm_ip_mss_channel;
#endif
#if defined(CONFIG_SOC_HWPM_IP_MSS_GPU_HUB)
	t234_active_ip_info[T234_HWPM_IP_MSS_GPU_HUB] =
			&t234_hwpm_ip_mss_gpu_hub;
#endif
#if defined(CONFIG_SOC_HWPM_IP_MSS_ISO_NISO_HUBS)
	t234_active_ip_info[T234_HWPM_IP_MSS_ISO_NISO_HUBS] =
			&t234_hwpm_ip_mss_iso_niso_hubs;
#endif
#if defined(CONFIG_SOC_HWPM_IP_MSS_MCF)
	t234_active_ip_info[T234_HWPM_IP_MSS_MCF] = &t234_hwpm_ip_mss_mcf;
#endif
#if defined(CONFIG_SOC_HWPM_IP_NVDEC)
	t234_active_ip_info[T234_HWPM_IP_NVDEC] = &t234_hwpm_ip_nvdec;
#endif
#if defined(CONFIG_SOC_HWPM_IP_NVDLA)
	t234_active_ip_info[T234_HWPM_IP_NVDLA] = &t234_hwpm_ip_nvdla;
#endif
#if defined(CONFIG_SOC_HWPM_IP_NVENC)
	t234_active_ip_info[T234_HWPM_IP_NVENC] = &t234_hwpm_ip_nvenc;
#endif
#if defined(CONFIG_SOC_HWPM_IP_OFA)
	t234_active_ip_info[T234_HWPM_IP_OFA] = &t234_hwpm_ip_ofa;
#endif
#if defined(CONFIG_SOC_HWPM_IP_PCIE)
	t234_active_ip_info[T234_HWPM_IP_PCIE] = &t234_hwpm_ip_pcie;
#endif
#if defined(CONFIG_SOC_HWPM_IP_PVA)
	t234_active_ip_info[T234_HWPM_IP_PVA] = &t234_hwpm_ip_pva;
#endif
#if defined(CONFIG_SOC_HWPM_IP_SCF)
	t234_active_ip_info[T234_HWPM_IP_SCF] = &t234_hwpm_ip_scf;
#endif
#if defined(CONFIG_SOC_HWPM_IP_VI)
	t234_active_ip_info[T234_HWPM_IP_VI] = &t234_hwpm_ip_vi;
#endif
#if defined(CONFIG_SOC_HWPM_IP_VIC)
	t234_active_ip_info[T234_HWPM_IP_VIC] = &t234_hwpm_ip_vic;
#endif

	if (!t234_hwpm_validate_hals(hwpm)) {
		return -EINVAL;
	}
	return 0;
}
