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

#ifndef T234_HWPM_INTERNAL_H
#define T234_HWPM_INTERNAL_H

#include <hal/t234/ip/vi/t234_hwpm_ip_vi.h>
#include <hal/t234/ip/isp/t234_hwpm_ip_isp.h>
#include <hal/t234/ip/vic/t234_hwpm_ip_vic.h>
#include <hal/t234/ip/ofa/t234_hwpm_ip_ofa.h>
#include <hal/t234/ip/pva/t234_hwpm_ip_pva.h>
#include <hal/t234/ip/nvdla/t234_hwpm_ip_nvdla.h>
#include <hal/t234/ip/mgbe/t234_hwpm_ip_mgbe.h>
#include <hal/t234/ip/scf/t234_hwpm_ip_scf.h>
#include <hal/t234/ip/nvdec/t234_hwpm_ip_nvdec.h>
#include <hal/t234/ip/nvenc/t234_hwpm_ip_nvenc.h>
#include <hal/t234/ip/pcie/t234_hwpm_ip_pcie.h>
#include <hal/t234/ip/display/t234_hwpm_ip_display.h>
#include <hal/t234/ip/mss_channel/t234_hwpm_ip_mss_channel.h>
#include <hal/t234/ip/mss_gpu_hub/t234_hwpm_ip_mss_gpu_hub.h>
#include <hal/t234/ip/mss_iso_niso_hubs/t234_hwpm_ip_mss_iso_niso_hubs.h>
#include <hal/t234/ip/mss_mcf/t234_hwpm_ip_mss_mcf.h>
#include <hal/t234/ip/pma/t234_hwpm_ip_pma.h>
#include <hal/t234/ip/rtr/t234_hwpm_ip_rtr.h>

#define T234_HWPM_ACTIVE_IP_MAX		T234_HWPM_IP_MAX

#define T234_ACTIVE_IPS							\
	DEFINE_SOC_HWPM_ACTIVE_IP(T234_HWPM_ACTIVE_IP_PMA)		\
	DEFINE_SOC_HWPM_ACTIVE_IP(T234_HWPM_ACTIVE_IP_RTR)		\
	DEFINE_SOC_HWPM_ACTIVE_IP(T234_HWPM_ACTIVE_IP_VI)		\
	DEFINE_SOC_HWPM_ACTIVE_IP(T234_HWPM_ACTIVE_IP_ISP)		\
	DEFINE_SOC_HWPM_ACTIVE_IP(T234_HWPM_ACTIVE_IP_VIC)		\
	DEFINE_SOC_HWPM_ACTIVE_IP(T234_HWPM_ACTIVE_IP_OFA)		\
	DEFINE_SOC_HWPM_ACTIVE_IP(T234_HWPM_ACTIVE_IP_PVA)		\
	DEFINE_SOC_HWPM_ACTIVE_IP(T234_HWPM_ACTIVE_IP_NVDLA)		\
	DEFINE_SOC_HWPM_ACTIVE_IP(T234_HWPM_ACTIVE_IP_MGBE)		\
	DEFINE_SOC_HWPM_ACTIVE_IP(T234_HWPM_ACTIVE_IP_SCF)		\
	DEFINE_SOC_HWPM_ACTIVE_IP(T234_HWPM_ACTIVE_IP_NVDEC)		\
	DEFINE_SOC_HWPM_ACTIVE_IP(T234_HWPM_ACTIVE_IP_NVENC)		\
	DEFINE_SOC_HWPM_ACTIVE_IP(T234_HWPM_ACTIVE_IP_PCIE)		\
	DEFINE_SOC_HWPM_ACTIVE_IP(T234_HWPM_ACTIVE_IP_DISPLAY)		\
	DEFINE_SOC_HWPM_ACTIVE_IP(T234_HWPM_ACTIVE_IP_MSS_CHANNEL)	\
	DEFINE_SOC_HWPM_ACTIVE_IP(T234_HWPM_ACTIVE_IP_MSS_GPU_HUB)	\
	DEFINE_SOC_HWPM_ACTIVE_IP(T234_HWPM_ACTIVE_IP_MSS_ISO_NISO_HUBS) \
	DEFINE_SOC_HWPM_ACTIVE_IP(T234_HWPM_ACTIVE_IP_MSS_MCF)		\
	DEFINE_SOC_HWPM_ACTIVE_IP(T234_HWPM_ACTIVE_IP_MAX)

#undef DEFINE_SOC_HWPM_ACTIVE_IP
#define DEFINE_SOC_HWPM_ACTIVE_IP(name)	name
enum t234_hwpm_active_ips {
	T234_ACTIVE_IPS
};
#undef DEFINE_SOC_HWPM_ACTIVE_IP

enum tegra_soc_hwpm_ip;
enum tegra_soc_hwpm_resource;
struct tegra_soc_hwpm;
struct hwpm_ip_aperture;

bool t234_hwpm_is_ip_active(struct tegra_soc_hwpm *hwpm,
	u32 ip_index, u32 *config_ip_index);
bool t234_hwpm_is_resource_active(struct tegra_soc_hwpm *hwpm,
	u32 res_index, u32 *config_ip_index);

u32 t234_get_rtr_int_idx(struct tegra_soc_hwpm *hwpm);
u32 t234_get_ip_max_idx(struct tegra_soc_hwpm *hwpm);

int t234_hwpm_extract_ip_ops(struct tegra_soc_hwpm *hwpm,
	struct tegra_soc_hwpm_ip_ops *hwpm_ip_ops, bool available);
int t234_hwpm_force_enable_ips(struct tegra_soc_hwpm *hwpm);
int t234_hwpm_validate_current_config(struct tegra_soc_hwpm *hwpm);
int t234_hwpm_get_fs_info(struct tegra_soc_hwpm *hwpm,
	u32 ip_enum, u64 *fs_mask, u8 *ip_status);
int t234_hwpm_get_resource_info(struct tegra_soc_hwpm *hwpm,
	u32 resource_enum, u8 *status);

int t234_hwpm_init_prod_values(struct tegra_soc_hwpm *hwpm);
int t234_hwpm_disable_slcg(struct tegra_soc_hwpm *hwpm);
int t234_hwpm_enable_slcg(struct tegra_soc_hwpm *hwpm);

int t234_hwpm_disable_triggers(struct tegra_soc_hwpm *hwpm);
int t234_hwpm_perfmon_enable(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_aperture *perfmon);
int t234_hwpm_perfmux_disable(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_aperture *perfmux);
int t234_hwpm_perfmon_disable(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_aperture *perfmon);

int t234_hwpm_disable_mem_mgmt(struct tegra_soc_hwpm *hwpm);
int t234_hwpm_enable_mem_mgmt(struct tegra_soc_hwpm *hwpm,
	struct tegra_soc_hwpm_alloc_pma_stream *alloc_pma_stream);
int t234_hwpm_invalidate_mem_config(struct tegra_soc_hwpm *hwpm);
int t234_hwpm_stream_mem_bytes(struct tegra_soc_hwpm *hwpm);
int t234_hwpm_disable_pma_streaming(struct tegra_soc_hwpm *hwpm);
int t234_hwpm_update_mem_bytes_get_ptr(struct tegra_soc_hwpm *hwpm,
	u64 mem_bump);
u64 t234_hwpm_get_mem_bytes_put_ptr(struct tegra_soc_hwpm *hwpm);
bool t234_hwpm_membuf_overflow_status(struct tegra_soc_hwpm *hwpm);

size_t t234_hwpm_get_alist_buf_size(struct tegra_soc_hwpm *hwpm);
int t234_hwpm_zero_alist_regs(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_inst *ip_inst, struct hwpm_ip_aperture *aperture);
int t234_hwpm_copy_alist(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_aperture *aperture, u64 *full_alist,
	u64 *full_alist_idx);
bool t234_hwpm_check_alist(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_aperture *aperture, u64 phys_addr);

#endif /* T234_HWPM_INTERNAL_H */
