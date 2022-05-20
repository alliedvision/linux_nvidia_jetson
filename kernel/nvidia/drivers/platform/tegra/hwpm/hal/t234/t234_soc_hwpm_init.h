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
 */

#ifndef T234_SOC_HWPM_INIT_H
#define T234_SOC_HWPM_INIT_H

#include <hal/tegra-soc-hwpm-structures.h>

void __iomem **t234_soc_hwpm_init_dt_apertures(void);
struct tegra_soc_hwpm_ip_ops *t234_soc_hwpm_init_ip_ops_info(void);
bool t234_soc_hwpm_is_perfmon(u32 dt_aperture);
u64 t234_soc_hwpm_get_perfmon_base(u32 dt_aperture);
bool t234_soc_hwpm_is_dt_aperture(u32 dt_aperture);
u32 t234_soc_hwpm_get_ip_aperture(struct tegra_soc_hwpm *hwpm,
	u64 phys_address, u64 *ip_base_addr);
int t234_soc_hwpm_fs_info_init(struct tegra_soc_hwpm *hwpm);
int t234_soc_hwpm_disable_pma_triggers(struct tegra_soc_hwpm *hwpm);
u32 **t234_soc_hwpm_get_mc_fake_regs(struct tegra_soc_hwpm *hwpm,
			struct hwpm_resource_aperture *aperture);
void t234_soc_hwpm_set_mc_fake_regs(struct tegra_soc_hwpm *hwpm,
			struct hwpm_resource_aperture *aperture,
			bool set_null);
int t234_soc_hwpm_pma_rtr_map(struct tegra_soc_hwpm *hwpm);
int t234_soc_hwpm_pma_rtr_unmap(struct tegra_soc_hwpm *hwpm);
int t234_soc_hwpm_disable_slcg(struct tegra_soc_hwpm *hwpm);
int t234_soc_hwpm_enable_slcg(struct tegra_soc_hwpm *hwpm);
struct hwpm_resource_aperture *t234_soc_hwpm_find_aperture(
		struct tegra_soc_hwpm *hwpm, u64 phys_addr,
		bool use_absolute_base, bool check_reservation,
		u64 *updated_pa);

void t234_soc_hwpm_zero_alist_regs(struct tegra_soc_hwpm *hwpm,
		struct hwpm_resource_aperture *aperture);
int t234_soc_hwpm_update_allowlist(struct tegra_soc_hwpm *hwpm,
				 void *ioctl_struct);
bool t234_soc_hwpm_allowlist_check(struct hwpm_resource_aperture *aperture,
			    u64 phys_addr, bool use_absolute_base,
			    u64 *updated_pa);
void t234_soc_hwpm_get_full_allowlist(struct tegra_soc_hwpm *hwpm);

int t234_soc_hwpm_update_mem_bytes(struct tegra_soc_hwpm *hwpm,
		struct tegra_soc_hwpm_update_get_put *update_get_put);
int t234_soc_hwpm_clear_pipeline(struct tegra_soc_hwpm *hwpm);
int t234_soc_hwpm_stream_buf_map(struct tegra_soc_hwpm *hwpm,
	struct tegra_soc_hwpm_alloc_pma_stream *alloc_pma_stream);

bool t234_soc_hwpm_is_dt_aperture_reserved(struct tegra_soc_hwpm *hwpm,
		struct hwpm_resource_aperture *aperture, u32 rsrc_id);
int t234_soc_hwpm_reserve_given_resource(
		struct tegra_soc_hwpm *hwpm, u32 resource);
void t234_soc_hwpm_reset_resources(struct tegra_soc_hwpm *hwpm);
void t234_soc_hwpm_disable_perfmons(struct tegra_soc_hwpm *hwpm);
int t234_soc_hwpm_bind_resources(struct tegra_soc_hwpm *hwpm);

#endif /* T234_SOC_HWPM_INIT_H */
