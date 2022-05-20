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
 *
 * This header contains interfaces to soc specific functions.
 */

#ifndef TEGRA_SOC_HWPM_INIT_H
#define TEGRA_SOC_HWPM_INIT_H

#include <hal/tegra-soc-hwpm-structures.h>
#include <uapi/linux/tegra-soc-hwpm-uapi.h>

void __iomem **tegra_soc_hwpm_init_dt_apertures(void);
struct tegra_soc_hwpm_ip_ops *tegra_soc_hwpm_init_ip_ops_info(void);
bool tegra_soc_hwpm_is_perfmon(u32 dt_aperture);
u64 tegra_soc_hwpm_get_perfmon_base(u32 dt_aperture);
bool tegra_soc_hwpm_is_dt_aperture(u32 dt_aperture);
bool tegra_soc_hwpm_is_dt_aperture_reserved(struct tegra_soc_hwpm *hwpm,
	struct hwpm_resource_aperture *aperture, u32 rsrc_id);
u32 tegra_soc_hwpm_get_ip_aperture(struct tegra_soc_hwpm *hwpm,
	u64 phys_address, u64 *ip_base_addr);
struct hwpm_resource_aperture *tegra_soc_hwpm_find_aperture(
		struct tegra_soc_hwpm *hwpm, u64 phys_addr,
		bool use_absolute_base, bool check_reservation,
		u64 *updated_pa);
int tegra_soc_hwpm_fs_info_init(struct tegra_soc_hwpm *hwpm);
int tegra_soc_hwpm_disable_pma_triggers(struct tegra_soc_hwpm *hwpm);
u32 **tegra_soc_hwpm_get_mc_fake_regs(struct tegra_soc_hwpm *hwpm,
			struct hwpm_resource_aperture *aperture);
void tegra_soc_hwpm_set_mc_fake_regs(struct tegra_soc_hwpm *hwpm,
			struct hwpm_resource_aperture *aperture,
			bool set_null);
int tegra_soc_hwpm_disable_slcg(struct tegra_soc_hwpm *hwpm);
int tegra_soc_hwpm_enable_slcg(struct tegra_soc_hwpm *hwpm);
void tegra_soc_hwpm_zero_alist_regs(struct tegra_soc_hwpm *hwpm,
		struct hwpm_resource_aperture *aperture);
int tegra_soc_hwpm_update_allowlist(struct tegra_soc_hwpm *hwpm,
				 void *ioctl_struct);
bool tegra_soc_hwpm_allowlist_check(struct hwpm_resource_aperture *aperture,
			    u64 phys_addr, bool use_absolute_base,
			    u64 *updated_pa);
void tegra_soc_hwpm_get_full_allowlist(struct tegra_soc_hwpm *hwpm);
int tegra_soc_hwpm_update_mem_bytes(struct tegra_soc_hwpm *hwpm,
		struct tegra_soc_hwpm_update_get_put *update_get_put);
int tegra_soc_hwpm_clear_pipeline(struct tegra_soc_hwpm *hwpm);
int tegra_soc_hwpm_stream_buf_map(struct tegra_soc_hwpm *hwpm,
	struct tegra_soc_hwpm_alloc_pma_stream *alloc_pma_stream);
int tegra_soc_hwpm_pma_rtr_map(struct tegra_soc_hwpm *hwpm);
int tegra_soc_hwpm_pma_rtr_unmap(struct tegra_soc_hwpm *hwpm);
int tegra_soc_hwpm_reserve_given_resource(
	struct tegra_soc_hwpm *hwpm, u32 resource);
void tegra_soc_hwpm_reset_resources(struct tegra_soc_hwpm *hwpm);
void tegra_soc_hwpm_disable_perfmons(struct tegra_soc_hwpm *hwpm);
int tegra_soc_hwpm_bind_resources(struct tegra_soc_hwpm *hwpm);

#endif /* TEGRA_SOC_HWPM_INIT_H */
