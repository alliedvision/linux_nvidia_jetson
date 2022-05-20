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

#include <soc/tegra/fuse.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/of_address.h>
#include <linux/dma-buf.h>

#include <uapi/linux/tegra-soc-hwpm-uapi.h>

#include <tegra-soc-hwpm-log.h>
#include <tegra-soc-hwpm-io.h>
#include <hal/tegra-soc-hwpm-structures.h>
#include <hal/t234/t234_soc_hwpm_perfmon_dt.h>
#include <hal/t234/t234_soc_hwpm_init.h>
#include <hal/t234/hw/t234_addr_map_soc_hwpm.h>

/*
 * Normally there is a 1-to-1 mapping between an MMIO aperture and a
 * hwpm_resource_aperture struct. But MC MMIO apertures are used in multiple
 * hwpm_resource_aperture structs. Therefore, we have to share the fake register
 * arrays between these hwpm_resource_aperture structs. This is why we have to
 * define the fake register arrays globally. For all other 1-to-1 mapping
 * apertures the fake register arrays are directly embedded inside the
 * hwpm_resource_aperture structs.
 */
u32 *t234_mc_fake_regs[16] = {NULL};

bool t234_soc_hwpm_is_dt_aperture_reserved(struct tegra_soc_hwpm *hwpm,
	struct hwpm_resource_aperture *aperture, u32 rsrc_id)
{
	return ((aperture->dt_aperture == T234_SOC_HWPM_PMA_DT) ||
		(aperture->dt_aperture == T234_SOC_HWPM_RTR_DT) ||
		(aperture->dt_aperture == T234_SOC_HWPM_SYS0_PERFMON_DT) ||
		((aperture->index_mask & hwpm->ip_fs_info[rsrc_id]) != 0));
}

u32 **t234_soc_hwpm_get_mc_fake_regs(struct tegra_soc_hwpm *hwpm,
			      struct hwpm_resource_aperture *aperture)
{
	if (!hwpm->fake_registers_enabled)
		return NULL;
	if (!aperture) {
		tegra_soc_hwpm_err("aperture is NULL");
		return NULL;
	}

	switch (aperture->start_pa) {
	case addr_map_mc0_base_r():
		return &t234_mc_fake_regs[0];
	case addr_map_mc1_base_r():
		return &t234_mc_fake_regs[1];
	case addr_map_mc2_base_r():
		return &t234_mc_fake_regs[2];
	case addr_map_mc3_base_r():
		return &t234_mc_fake_regs[3];
	case addr_map_mc4_base_r():
		return &t234_mc_fake_regs[4];
	case addr_map_mc5_base_r():
		return &t234_mc_fake_regs[5];
	case addr_map_mc6_base_r():
		return &t234_mc_fake_regs[6];
	case addr_map_mc7_base_r():
		return &t234_mc_fake_regs[7];
	case addr_map_mc8_base_r():
		return &t234_mc_fake_regs[8];
	case addr_map_mc9_base_r():
		return &t234_mc_fake_regs[9];
	case addr_map_mc10_base_r():
		return &t234_mc_fake_regs[10];
	case addr_map_mc11_base_r():
		return &t234_mc_fake_regs[11];
	case addr_map_mc12_base_r():
		return &t234_mc_fake_regs[12];
	case addr_map_mc13_base_r():
		return &t234_mc_fake_regs[13];
	case addr_map_mc14_base_r():
		return &t234_mc_fake_regs[14];
	case addr_map_mc15_base_r():
		return &t234_mc_fake_regs[15];
	default:
		return NULL;
	}
}

void t234_soc_hwpm_set_mc_fake_regs(struct tegra_soc_hwpm *hwpm,
			     struct hwpm_resource_aperture *aperture,
			     bool set_null)
{
	u32 *fake_regs = NULL;

	/* Get pointer to array of MSS channel apertures */
	struct hwpm_resource_aperture *l_mss_channel_map =
		hwpm->hwpm_resources[TEGRA_SOC_HWPM_RESOURCE_MSS_CHANNEL].map;
	/* Get pointer to array of MSS ISO/NISO hub apertures */
	struct hwpm_resource_aperture *l_mss_iso_niso_map =
		hwpm->hwpm_resources[TEGRA_SOC_HWPM_RESOURCE_MSS_ISO_NISO_HUBS].map;
	/* Get pointer to array of MSS MCF apertures */
	struct hwpm_resource_aperture *l_mss_mcf_map =
		hwpm->hwpm_resources[TEGRA_SOC_HWPM_RESOURCE_MSS_MCF].map;

	if (!aperture) {
		tegra_soc_hwpm_err("aperture is NULL");
		return;
	}

	switch (aperture->start_pa) {
	case addr_map_mc0_base_r():
		fake_regs = (!hwpm->fake_registers_enabled || set_null) ?
							NULL : t234_mc_fake_regs[0];
		l_mss_channel_map[0].fake_registers = fake_regs;
		l_mss_iso_niso_map[0].fake_registers = fake_regs;
		l_mss_mcf_map[0].fake_registers = fake_regs;
		break;
	case addr_map_mc1_base_r():
		fake_regs = (!hwpm->fake_registers_enabled || set_null) ?
							NULL : t234_mc_fake_regs[1];
		l_mss_channel_map[1].fake_registers = fake_regs;
		l_mss_iso_niso_map[1].fake_registers = fake_regs;
		l_mss_mcf_map[1].fake_registers = fake_regs;
		break;
	case addr_map_mc2_base_r():
		fake_regs = (!hwpm->fake_registers_enabled || set_null) ?
							NULL : t234_mc_fake_regs[2];
		l_mss_channel_map[2].fake_registers = fake_regs;
		l_mss_iso_niso_map[2].fake_registers = fake_regs;
		l_mss_mcf_map[2].fake_registers = fake_regs;
		break;
	case addr_map_mc3_base_r():
		fake_regs = (!hwpm->fake_registers_enabled || set_null) ?
							NULL : t234_mc_fake_regs[3];
		l_mss_channel_map[3].fake_registers = fake_regs;
		l_mss_iso_niso_map[3].fake_registers = fake_regs;
		l_mss_mcf_map[3].fake_registers = fake_regs;
		break;
	case addr_map_mc4_base_r():
		fake_regs = (!hwpm->fake_registers_enabled || set_null) ?
							NULL : t234_mc_fake_regs[4];
		l_mss_channel_map[4].fake_registers = fake_regs;
		l_mss_iso_niso_map[4].fake_registers = fake_regs;
		l_mss_mcf_map[4].fake_registers = fake_regs;
		break;
	case addr_map_mc5_base_r():
		fake_regs = (!hwpm->fake_registers_enabled || set_null) ?
							NULL : t234_mc_fake_regs[5];
		l_mss_channel_map[5].fake_registers = fake_regs;
		l_mss_iso_niso_map[5].fake_registers = fake_regs;
		l_mss_mcf_map[5].fake_registers = fake_regs;
		break;
	case addr_map_mc6_base_r():
		fake_regs = (!hwpm->fake_registers_enabled || set_null) ?
							NULL : t234_mc_fake_regs[6];
		l_mss_channel_map[6].fake_registers = fake_regs;
		l_mss_iso_niso_map[6].fake_registers = fake_regs;
		l_mss_mcf_map[6].fake_registers = fake_regs;
		break;
	case addr_map_mc7_base_r():
		fake_regs = (!hwpm->fake_registers_enabled || set_null) ?
							NULL : t234_mc_fake_regs[7];
		l_mss_channel_map[7].fake_registers = fake_regs;
		l_mss_iso_niso_map[7].fake_registers = fake_regs;
		l_mss_mcf_map[7].fake_registers = fake_regs;
		break;
	case addr_map_mc8_base_r():
		fake_regs = (!hwpm->fake_registers_enabled || set_null) ?
							NULL : t234_mc_fake_regs[8];
		l_mss_channel_map[8].fake_registers = fake_regs;
		l_mss_iso_niso_map[8].fake_registers = fake_regs;
		break;
	case addr_map_mc9_base_r():
		fake_regs = (!hwpm->fake_registers_enabled || set_null) ?
							NULL : t234_mc_fake_regs[9];
		l_mss_channel_map[9].fake_registers = fake_regs;
		break;
	case addr_map_mc10_base_r():
		fake_regs = (!hwpm->fake_registers_enabled || set_null) ?
							NULL : t234_mc_fake_regs[10];
		l_mss_channel_map[10].fake_registers = fake_regs;
		break;
	case addr_map_mc11_base_r():
		fake_regs = (!hwpm->fake_registers_enabled || set_null) ?
							NULL : t234_mc_fake_regs[11];
		l_mss_channel_map[11].fake_registers = fake_regs;
		break;
	case addr_map_mc12_base_r():
		fake_regs = (!hwpm->fake_registers_enabled || set_null) ?
							NULL : t234_mc_fake_regs[12];
		l_mss_channel_map[12].fake_registers = fake_regs;
		break;
	case addr_map_mc13_base_r():
		fake_regs = (!hwpm->fake_registers_enabled || set_null) ?
							NULL : t234_mc_fake_regs[13];
		l_mss_channel_map[13].fake_registers = fake_regs;
		break;
	case addr_map_mc14_base_r():
		fake_regs = (!hwpm->fake_registers_enabled || set_null) ?
							NULL : t234_mc_fake_regs[14];
		l_mss_channel_map[14].fake_registers = fake_regs;
		break;
	case addr_map_mc15_base_r():
		fake_regs = (!hwpm->fake_registers_enabled || set_null) ?
							NULL : t234_mc_fake_regs[15];
		l_mss_channel_map[15].fake_registers = fake_regs;
		break;
	default:
		break;
	}
}

int t234_soc_hwpm_reserve_given_resource(struct tegra_soc_hwpm *hwpm, u32 resource)
{
	struct hwpm_resource_aperture *aperture = NULL;
	int aprt_idx = 0;
	int ret = 0, err;
	struct tegra_soc_hwpm_ip_ops *ip_ops;

	/* Map reserved apertures and allocate fake register arrays if needed */
	for (aprt_idx = 0;
	     aprt_idx < hwpm->hwpm_resources[resource].map_size;
	     aprt_idx++) {
		aperture = &(hwpm->hwpm_resources[resource].map[aprt_idx]);
		if ((aperture->dt_aperture == T234_SOC_HWPM_PMA_DT) ||
		    (aperture->dt_aperture == T234_SOC_HWPM_RTR_DT)) {
			/* PMA and RTR apertures are handled in open(fd) */
			continue;
		} else if (t234_soc_hwpm_is_dt_aperture_reserved(hwpm,
				aperture, resource)) {
			if (t234_soc_hwpm_is_perfmon(aperture->dt_aperture)) {
				struct resource *res = NULL;
				u64 num_regs = 0;

				tegra_soc_hwpm_dbg("Found PERFMON(0x%llx - 0x%llx)",
					aperture->start_pa, aperture->end_pa);
				ip_ops = &hwpm->ip_info[aperture->dt_aperture];
				if (ip_ops && (*ip_ops->hwpm_ip_pm)) {
					err = (*ip_ops->hwpm_ip_pm)
							(ip_ops->ip_dev, true);
					if (err) {
						tegra_soc_hwpm_err(
							"Disable Runtime PM(%d) Failed",
								aperture->dt_aperture);
					}
				} else {
					tegra_soc_hwpm_dbg(
						"No Runtime PM(%d) for IP",
								aperture->dt_aperture);
				}
				hwpm->dt_apertures[aperture->dt_aperture] =
						of_iomap(hwpm->np, aperture->dt_aperture);
				if (!hwpm->dt_apertures[aperture->dt_aperture]) {
					tegra_soc_hwpm_err("Couldn't map PERFMON(%d)",
						aperture->dt_aperture);
					ret = -ENOMEM;
					goto fail;
				}

				res = platform_get_resource(hwpm->pdev,
								IORESOURCE_MEM,
								aperture->dt_aperture);
				if ((!res) || (res->start == 0) || (res->end == 0)) {
					tegra_soc_hwpm_err("Invalid resource for PERFMON(%d)",
						aperture->dt_aperture);
					ret = -ENOMEM;
					goto fail;
				}
				aperture->start_pa = res->start;
				aperture->end_pa = res->end;

				if (hwpm->fake_registers_enabled) {
					num_regs = (aperture->end_pa + 1 - aperture->start_pa) /
						sizeof(*aperture->fake_registers);
					aperture->fake_registers =
						(u32 *)kzalloc(sizeof(*aperture->fake_registers) *
											num_regs,
								GFP_KERNEL);
					if (!aperture->fake_registers) {
						tegra_soc_hwpm_err("Aperture(0x%llx - 0x%llx):"
							" Couldn't allocate memory for fake"
							" registers",
							aperture->start_pa,
							aperture->end_pa);
						ret = -ENOMEM;
						goto fail;
					}
				}
			} else { /* IP apertures */
				if (hwpm->fake_registers_enabled) {
					u64 num_regs = 0;
					u32 **fake_regs =
						t234_soc_hwpm_get_mc_fake_regs(hwpm, aperture);

					if (!fake_regs)
						fake_regs = &aperture->fake_registers;

					num_regs = (aperture->end_pa + 1 - aperture->start_pa) /
						sizeof(*(*fake_regs));
					*fake_regs =
						(u32 *)kzalloc(sizeof(*(*fake_regs)) * num_regs,
								GFP_KERNEL);
					if (!(*fake_regs)) {
						tegra_soc_hwpm_err("Aperture(0x%llx - 0x%llx):"
							" Couldn't allocate memory for fake"
							" registers",
							aperture->start_pa,
							aperture->end_pa);
						ret = -ENOMEM;
						goto fail;
					}

					t234_soc_hwpm_set_mc_fake_regs(hwpm, aperture, false);
				}
			}
		} else {
			tegra_soc_hwpm_dbg("resource %d index_mask %d not available",
				resource, aperture->index_mask);
		}
	}

	hwpm->hwpm_resources[resource].reserved = true;
	goto success;

fail:
	for (aprt_idx = 0;
	     aprt_idx < hwpm->hwpm_resources[resource].map_size;
	     aprt_idx++) {
		aperture = &(hwpm->hwpm_resources[resource].map[aprt_idx]);
		if ((aperture->dt_aperture == T234_SOC_HWPM_PMA_DT) ||
		    (aperture->dt_aperture == T234_SOC_HWPM_RTR_DT)) {
			/* PMA and RTR apertures are handled in open(fd) */
			continue;
		} else if (t234_soc_hwpm_is_dt_aperture_reserved(hwpm,
			aperture, resource)) {
			if (t234_soc_hwpm_is_perfmon(aperture->dt_aperture)) {
				if (hwpm->dt_apertures[aperture->dt_aperture]) {
					iounmap(hwpm->dt_apertures[aperture->dt_aperture]);
					hwpm->dt_apertures[aperture->dt_aperture] = NULL;
				}

				aperture->start_pa = 0;
				aperture->end_pa = 0;

				if (aperture->fake_registers) {
					kfree(aperture->fake_registers);
					aperture->fake_registers = NULL;
				}
			} else { /* IP apertures */
				if (aperture->fake_registers) {
					kfree(aperture->fake_registers);
					aperture->fake_registers = NULL;
					t234_soc_hwpm_set_mc_fake_regs(hwpm, aperture, true);
				}
			}
		}
	}

	hwpm->hwpm_resources[resource].reserved = false;

success:
	return ret;
}

void t234_soc_hwpm_reset_resources(struct tegra_soc_hwpm *hwpm)
{
	int res_idx = 0;
	int aprt_idx = 0;
	struct hwpm_resource_aperture *aperture = NULL;

	/* Reset resource and aperture state */
	for (res_idx = 0; res_idx < TERGA_SOC_HWPM_NUM_RESOURCES; res_idx++) {
		if (!hwpm->hwpm_resources[res_idx].reserved)
			continue;
		hwpm->hwpm_resources[res_idx].reserved = false;

		for (aprt_idx = 0;
		     aprt_idx < hwpm->hwpm_resources[res_idx].map_size;
		     aprt_idx++) {
			aperture = &(hwpm->hwpm_resources[res_idx].map[aprt_idx]);
			if ((aperture->dt_aperture == T234_SOC_HWPM_PMA_DT) ||
			    (aperture->dt_aperture == T234_SOC_HWPM_RTR_DT)) {
				/* PMA and RTR apertures are handled separately */
				continue;
			} else if (t234_soc_hwpm_is_perfmon(aperture->dt_aperture)) {
				if (hwpm->dt_apertures[aperture->dt_aperture]) {
					iounmap(hwpm->dt_apertures[aperture->dt_aperture]);
					hwpm->dt_apertures[aperture->dt_aperture] = NULL;
				}

				aperture->start_pa = 0;
				aperture->end_pa = 0;

				if (aperture->fake_registers) {
					kfree(aperture->fake_registers);
					aperture->fake_registers = NULL;
				}
			} else { /* IP apertures */
				if (aperture->fake_registers) {
					kfree(aperture->fake_registers);
					aperture->fake_registers = NULL;
					t234_soc_hwpm_set_mc_fake_regs(hwpm, aperture, true);
				}
			}
		}
	}
}

void t234_soc_hwpm_disable_perfmons(struct tegra_soc_hwpm *hwpm)
{
	int res_idx = 0;
	int aprt_idx = 0;
	struct hwpm_resource_aperture *aperture = NULL;
	struct tegra_soc_hwpm_ip_ops *ip_ops;
	int err, ret = 0;

	for (res_idx = 0; res_idx < TERGA_SOC_HWPM_NUM_RESOURCES; res_idx++) {
		if (!hwpm->hwpm_resources[res_idx].reserved)
			continue;
		tegra_soc_hwpm_dbg("Found reserved IP(%d)", res_idx);

		for (aprt_idx = 0;
		     aprt_idx < hwpm->hwpm_resources[res_idx].map_size;
		     aprt_idx++) {
			aperture = &(hwpm->hwpm_resources[res_idx].map[aprt_idx]);
			if (t234_soc_hwpm_is_perfmon(aperture->dt_aperture)) {
				if (t234_soc_hwpm_is_dt_aperture_reserved(hwpm,
					aperture, res_idx)) {
					tegra_soc_hwpm_dbg("Found PERFMON(0x%llx - 0x%llx)",
							   aperture->start_pa,
							   aperture->end_pa);
					err = reg_rmw(hwpm, NULL, aperture->dt_aperture,
						pmmsys_control_r(0) - addr_map_rpg_pm_base_r(),
						pmmsys_control_mode_m(),
						pmmsys_control_mode_disable_f(),
						false, false);
					RELEASE_FAIL("Unable to disable PERFMON(0x%llx - 0x%llx)",
						     aperture->start_pa,
						     aperture->end_pa);
					ip_ops = &hwpm->ip_info[aperture->dt_aperture];
					if (ip_ops && (*ip_ops->hwpm_ip_pm)) {
						err = (*ip_ops->hwpm_ip_pm)
									(ip_ops->ip_dev, false);
						if (err) {
							tegra_soc_hwpm_err(
								"Enable Runtime PM(%d) Failed",
									aperture->dt_aperture);
						}
					} else {
						tegra_soc_hwpm_dbg(
							"No Runtime PM(%d) for IP",
									aperture->dt_aperture);
					}
				}
			}
		}
	}
}

int t234_soc_hwpm_bind_resources(struct tegra_soc_hwpm *hwpm)
{
	int ret = 0;
	int res_idx = 0;
	int aprt_idx = 0;
	struct hwpm_resource_aperture *aperture = NULL;

	for (res_idx = 0; res_idx < TERGA_SOC_HWPM_NUM_RESOURCES; res_idx++) {
		if (!hwpm->hwpm_resources[res_idx].reserved)
			continue;
		tegra_soc_hwpm_dbg("Found reserved IP(%d)", res_idx);

		for (aprt_idx = 0;
		     aprt_idx < hwpm->hwpm_resources[res_idx].map_size;
		     aprt_idx++) {
			aperture = &(hwpm->hwpm_resources[res_idx].map[aprt_idx]);

			if (t234_soc_hwpm_is_dt_aperture_reserved(hwpm,
				aperture, res_idx)) {

				/* Zero out necessary registers */
				if (aperture->alist) {
					t234_soc_hwpm_zero_alist_regs(hwpm, aperture);
				} else {
					tegra_soc_hwpm_err(
					"NULL allowlist in aperture(0x%llx - 0x%llx)",
						aperture->start_pa, aperture->end_pa);
				}

				/*
				 * Enable reporting of PERFMON status to
				 * NV_PERF_PMMSYS_SYS0ROUTER_PERFMONSTATUS_MERGED
				 */
				if (t234_soc_hwpm_is_perfmon(aperture->dt_aperture)) {
					tegra_soc_hwpm_dbg("Found PERFMON(0x%llx - 0x%llx)",
							   aperture->start_pa,
							   aperture->end_pa);
					ret = reg_rmw(hwpm, NULL, aperture->dt_aperture,
						pmmsys_sys0_enginestatus_r(0) -
							addr_map_rpg_pm_base_r(),
						pmmsys_sys0_enginestatus_enable_m(),
						pmmsys_sys0_enginestatus_enable_out_f(),
						false, false);
					if (ret < 0) {
						tegra_soc_hwpm_err(
							"Unable to set PMM ENGINESTATUS_ENABLE"
							" for PERFMON(0x%llx - 0x%llx)",
							aperture->start_pa,
							aperture->end_pa);
						return -EIO;
					}
				}
			}
		}
	}
	return 0;
}
