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

#include <soc/tegra/fuse.h>
#include <uapi/linux/tegra-soc-hwpm-uapi.h>

#include <tegra_hwpm.h>
#include <tegra_hwpm_io.h>
#include <tegra_hwpm_log.h>
#include <tegra_hwpm_common.h>
#include <tegra_hwpm_static_analysis.h>
#include <hal/t234/t234_hwpm_internal.h>
#include <hal/t234/hw/t234_addr_map_soc_hwpm.h>

/*
 * This function is invoked by register_ip API.
 * Convert the external resource enum to internal IP index.
 * Extract given ip_ops and update corresponding IP structure.
 */
int t234_hwpm_extract_ip_ops(struct tegra_soc_hwpm *hwpm,
	struct tegra_soc_hwpm_ip_ops *hwpm_ip_ops, bool available)
{
	int ret = 0;
	u32 ip_idx = 0U;

	tegra_hwpm_fn(hwpm, " ");

	tegra_hwpm_dbg(hwpm, hwpm_dbg_ip_register,
			"Extract IP ops for resource enum %d info",
			hwpm_ip_ops->resource_enum);

	/* Convert tegra_soc_hwpm_resource to internal enum */
	if (!(t234_hwpm_is_resource_active(hwpm,
			hwpm_ip_ops->resource_enum, &ip_idx))) {
		tegra_hwpm_dbg(hwpm, hwpm_dbg_ip_register,
			"SOC hwpm resource %d (base 0x%llx) is unconfigured",
			hwpm_ip_ops->resource_enum,
			hwpm_ip_ops->ip_base_address);
		goto fail;
	}

	switch (ip_idx) {
#if defined(CONFIG_SOC_HWPM_IP_VI)
	case T234_HWPM_IP_VI:
#endif
#if defined(CONFIG_SOC_HWPM_IP_ISP)
	case T234_HWPM_IP_ISP:
#endif
#if defined(CONFIG_SOC_HWPM_IP_VIC)
	case T234_HWPM_IP_VIC:
#endif
#if defined(CONFIG_SOC_HWPM_IP_OFA)
	case T234_HWPM_IP_OFA:
#endif
#if defined(CONFIG_SOC_HWPM_IP_PVA)
	case T234_HWPM_IP_PVA:
#endif
#if defined(CONFIG_SOC_HWPM_IP_NVDLA)
	case T234_HWPM_IP_NVDLA:
#endif
#if defined(CONFIG_SOC_HWPM_IP_MGBE)
	case T234_HWPM_IP_MGBE:
#endif
#if defined(CONFIG_SOC_HWPM_IP_SCF)
	case T234_HWPM_IP_SCF:
#endif
#if defined(CONFIG_SOC_HWPM_IP_NVDEC)
	case T234_HWPM_IP_NVDEC:
#endif
#if defined(CONFIG_SOC_HWPM_IP_NVENC)
	case T234_HWPM_IP_NVENC:
#endif
#if defined(CONFIG_SOC_HWPM_IP_PCIE)
	case T234_HWPM_IP_PCIE:
#endif
#if defined(CONFIG_SOC_HWPM_IP_DISPLAY)
	case T234_HWPM_IP_DISPLAY:
#endif
#if defined(CONFIG_SOC_HWPM_IP_MSS_GPU_HUB)
	case T234_HWPM_IP_MSS_GPU_HUB:
#endif
		ret = tegra_hwpm_set_fs_info_ip_ops(hwpm, hwpm_ip_ops,
			hwpm_ip_ops->ip_base_address, ip_idx, available);
		if (ret != 0) {
			tegra_hwpm_err(hwpm,
				"Failed to %s fs/ops for IP %d (base 0x%llx)",
				available == true ? "set" : "reset",
				ip_idx, hwpm_ip_ops->ip_base_address);
			goto fail;
		}
		break;
#if defined(CONFIG_SOC_HWPM_IP_MSS_CHANNEL)
	case T234_HWPM_IP_MSS_CHANNEL:
#endif
#if defined(CONFIG_SOC_HWPM_IP_MSS_ISO_NISO_HUBS)
	case T234_HWPM_IP_MSS_ISO_NISO_HUBS:
#endif
#if defined(CONFIG_SOC_HWPM_IP_MSS_MCF)
	case T234_HWPM_IP_MSS_MCF:
#endif
		/* MSS channel, ISO NISO hubs and MCF share MC channels */

		/* Check base address in T234_HWPM_IP_MSS_CHANNEL */
#if defined(CONFIG_SOC_HWPM_IP_MSS_CHANNEL)
		ip_idx = T234_HWPM_IP_MSS_CHANNEL;
		ret = tegra_hwpm_set_fs_info_ip_ops(hwpm, hwpm_ip_ops,
			hwpm_ip_ops->ip_base_address, ip_idx, available);
		if (ret != 0) {
			/*
			 * Return value of ENODEV will indicate that the base
			 * address doesn't belong to this IP.
			 * This case is valid, as not all base addresses are
			 * shared between MSS IPs.
			 * In this case, reset return value to 0.
			 */
			if (ret != -ENODEV) {
				tegra_hwpm_err(hwpm,
					"IP %d base 0x%llx:Failed to %s fs/ops",
					ip_idx, hwpm_ip_ops->ip_base_address,
					available == true ? "set" : "reset");
				goto fail;
			}
			ret = 0;
		}
#endif
#if defined(CONFIG_SOC_HWPM_IP_MSS_ISO_NISO_HUBS)
		/* Check base address in T234_HWPM_IP_MSS_ISO_NISO_HUBS */
		ip_idx = T234_HWPM_IP_MSS_ISO_NISO_HUBS;
		ret = tegra_hwpm_set_fs_info_ip_ops(hwpm, hwpm_ip_ops,
			hwpm_ip_ops->ip_base_address, ip_idx, available);
		if (ret != 0) {
			/*
			 * Return value of ENODEV will indicate that the base
			 * address doesn't belong to this IP.
			 * This case is valid, as not all base addresses are
			 * shared between MSS IPs.
			 * In this case, reset return value to 0.
			 */
			if (ret != -ENODEV) {
				tegra_hwpm_err(hwpm,
					"IP %d base 0x%llx:Failed to %s fs/ops",
					ip_idx, hwpm_ip_ops->ip_base_address,
					available == true ? "set" : "reset");
				goto fail;
			}
			ret = 0;
		}
#endif
#if defined(CONFIG_SOC_HWPM_IP_MSS_MCF)
		/* Check base address in T234_HWPM_IP_MSS_MCF */
		ip_idx = T234_HWPM_IP_MSS_MCF;
		ret = tegra_hwpm_set_fs_info_ip_ops(hwpm, hwpm_ip_ops,
			hwpm_ip_ops->ip_base_address, ip_idx, available);
		if (ret != 0) {
			/*
			 * Return value of ENODEV will indicate that the base
			 * address doesn't belong to this IP.
			 * This case is valid, as not all base addresses are
			 * shared between MSS IPs.
			 * In this case, reset return value to 0.
			 */
			if (ret != -ENODEV) {
				tegra_hwpm_err(hwpm,
					"IP %d base 0x%llx:Failed to %s fs/ops",
					ip_idx, hwpm_ip_ops->ip_base_address,
					available == true ? "set" : "reset");
				goto fail;
			}
			ret = 0;
		}
#endif
		break;
	case T234_HWPM_IP_PMA:
	case T234_HWPM_IP_RTR:
	default:
		tegra_hwpm_err(hwpm, "Invalid IP %d for ip_ops", ip_idx);
		break;
	}

fail:
	return ret;
}

int t234_hwpm_validate_current_config(struct tegra_soc_hwpm *hwpm)
{
	u32 production_mode = 0U;
	u32 security_mode = 0U;
	u32 fa_mode = 0U;
	u32 hwpm_global_disable = 0U;
	u32 idx = 0U;
	int err;
	struct tegra_soc_hwpm_chip *active_chip = hwpm->active_chip;
	struct hwpm_ip *chip_ip = NULL;

	tegra_hwpm_fn(hwpm, " ");

	if (!tegra_platform_is_silicon()) {
		return 0;
	}

	/* Read production mode fuse */
	err = tegra_fuse_readl(TEGRA_FUSE_PRODUCTION_MODE, &production_mode);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "prod mode fuse read failed");
		return err;
	}

#define TEGRA_FUSE_SECURITY_MODE		0xA0U
	err = tegra_fuse_readl(TEGRA_FUSE_SECURITY_MODE, &security_mode);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "security mode fuse read failed");
		return err;
	}

#define TEGRA_FUSE_FA_MODE			0x48U
	err = tegra_fuse_readl(TEGRA_FUSE_FA_MODE, &fa_mode);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "fa mode fuse read failed");
		return err;
	}

#define TEGRA_HWPM_GLOBAL_DISABLE_OFFSET	0x3CU
#define TEGRA_HWPM_GLOBAL_DISABLE_DISABLED	0x0U
	err = tegra_hwpm_read_sticky_bits(hwpm, addr_map_pmc_misc_base_r(),
		TEGRA_HWPM_GLOBAL_DISABLE_OFFSET, &hwpm_global_disable);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "hwpm global disable read failed");
		return err;
	}

	tegra_hwpm_dbg(hwpm, hwpm_info, "PROD_MODE fuse = 0x%x "
		"SECURITY_MODE fuse = 0x%x FA mode fuse = 0x%x"
		"HWPM_GLOBAL_DISABLE = 0x%x",
		production_mode, security_mode, fa_mode, hwpm_global_disable);

	/* Do not enable override if FA mode fuse is set */
	if (fa_mode != 0U) {
		tegra_hwpm_dbg(hwpm, hwpm_info,
			"fa mode fuse enabled, no override required");
		return 0;
	}

	/* Override enable depends on security mode and global hwpm disable */
	if ((security_mode == 0U) &&
		(hwpm_global_disable == TEGRA_HWPM_GLOBAL_DISABLE_DISABLED)) {
		tegra_hwpm_dbg(hwpm, hwpm_info,
			"security fuses are disabled, no override required");
		return 0;
	}

	for (idx = 0U; idx < active_chip->get_ip_max_idx(hwpm); idx++) {
		chip_ip = active_chip->chip_ips[idx];

		if ((hwpm_global_disable !=
			TEGRA_HWPM_GLOBAL_DISABLE_DISABLED) &&
			((chip_ip->dependent_fuse_mask &
			TEGRA_HWPM_FUSE_HWPM_GLOBAL_DISABLE_MASK) != 0U)) {
			/* HWPM disable is true */
			/* IP depends on HWPM global disable */
			chip_ip->override_enable = true;
		} else {
			/* HWPM disable is false */
			if ((security_mode != 0U) &&
				((chip_ip->dependent_fuse_mask &
				TEGRA_HWPM_FUSE_SECURITY_MODE_MASK) != 0U)) {
				/* Security mode fuse is set */
				/* IP depends on security mode fuse */
				chip_ip->override_enable = true;
			} else {
				/*
				 * This is a valid case since not all IPs
				 * depend on security fuse.
				 */
				tegra_hwpm_dbg(hwpm, hwpm_info,
					"IP %d not overridden", idx);
			}
		}
	}

	return 0;
}

int t234_hwpm_force_enable_ips(struct tegra_soc_hwpm *hwpm)
{

	int ret = 0;

	tegra_hwpm_fn(hwpm, " ");
#if defined(CONFIG_HWPM_ALLOW_FORCE_ENABLE)

#if defined(CONFIG_SOC_HWPM_IP_MSS_CHANNEL)

	if (is_tegra_hypervisor_mode()) {
		ret = tegra_hwpm_set_fs_info_ip_ops(hwpm, NULL,
			addr_map_mc0_base_r(),
			T234_HWPM_IP_MSS_CHANNEL, true);
		if (ret != 0) {
			tegra_hwpm_err(hwpm,
				"T234_HWPM_IP_MSS_CHANNEL force enable failed");
			return ret;
		}
	}
#endif

#if defined(CONFIG_SOC_HWPM_IP_MSS_GPU_HUB)

		/* MSS GPU HUB */
		ret = tegra_hwpm_set_fs_info_ip_ops(hwpm, NULL,
			addr_map_mss_nvlink_1_base_r(),
			T234_HWPM_IP_MSS_GPU_HUB, true);
		if (ret != 0) {
			tegra_hwpm_err(hwpm,
				"T234_HWPM_IP_MSS_GPU_HUB force enable failed");
			return ret;
		}
#endif

	if (tegra_platform_is_silicon()) {
		/* Static IP instances corresponding to silicon */
		/* VI */
/*
#if defined(CONFIG_SOC_HWPM_IP_VI)
		ret = tegra_hwpm_set_fs_info_ip_ops(hwpm, NULL,
			addr_map_vi_thi_base_r(),
			T234_HWPM_IP_VI, true);
		if (ret != 0) {
			tegra_hwpm_err(hwpm,
				"T234_HWPM_IP_VI force enable failed");
			return ret;
		}
		ret = tegra_hwpm_set_fs_info_ip_ops(hwpm, NULL,
			addr_map_vi2_thi_base_r(),
			T234_HWPM_IP_VI, true);
		if (ret != 0) {
			tegra_hwpm_err(hwpm,
				"T234_HWPM_IP_VI force enable failed");
			return ret;
		}
#endif
*/
#if defined(CONFIG_SOC_HWPM_IP_ISP)
		/* ISP */
		ret = tegra_hwpm_set_fs_info_ip_ops(hwpm, NULL,
			addr_map_isp_thi_base_r(),
			T234_HWPM_IP_ISP, true);
		if (ret != 0) {
			tegra_hwpm_err(hwpm,
				"T234_HWPM_IP_ISP force enable failed");
			return ret;
		}
#endif

		/* MGBE */
/*
#if defined(CONFIG_SOC_HWPM_IP_MGBE)
		ret = tegra_hwpm_set_fs_info_ip_ops(hwpm, NULL,
			addr_map_mgbe0_mac_rm_base_r(),
			T234_HWPM_IP_MGBE, true);
		if (ret != 0) {
			tegra_hwpm_err(hwpm,
				"T234_HWPM_IP_MGBE force enable failed");
			return ret;
		}
#endif
*/

#if defined(CONFIG_SOC_HWPM_IP_NVDEC)

		/* NVDEC */
		ret = tegra_hwpm_set_fs_info_ip_ops(hwpm, NULL,
			addr_map_nvdec_base_r(),
			T234_HWPM_IP_NVDEC, true);
		if (ret != 0) {
			tegra_hwpm_err(hwpm,
				"T234_HWPM_IP_NVDEC force enable failed");
			return ret;
		}
#endif

		/* PCIE */
/*
#if defined(CONFIG_SOC_HWPM_IP_PCIE)
		ret = tegra_hwpm_set_fs_info_ip_ops(hwpm, NULL,
			addr_map_pcie_c1_ctl_base_r(),
			T234_HWPM_IP_PCIE, true);
		if (ret != 0) {
			tegra_hwpm_err(hwpm,
				"T234_HWPM_IP_PCIE force enable failed");
			return ret;
		}
		ret = tegra_hwpm_set_fs_info_ip_ops(hwpm, NULL,
			addr_map_pcie_c4_ctl_base_r(),
			T234_HWPM_IP_PCIE, true);
		if (ret != 0) {
			tegra_hwpm_err(hwpm,
				"T234_HWPM_IP_PCIE force enable failed");
			return ret;
		}
		ret = tegra_hwpm_set_fs_info_ip_ops(hwpm, NULL,
			addr_map_pcie_c5_ctl_base_r(),
			T234_HWPM_IP_PCIE, true);
		if (ret != 0) {
			tegra_hwpm_err(hwpm,
				"T234_HWPM_IP_PCIE force enable failed");
			return ret;
		}
#endif
*/

		/* DISPLAY */
/*
#if defined(CONFIG_SOC_HWPM_IP_DISPLAY)
		ret = tegra_hwpm_set_fs_info_ip_ops(hwpm, NULL,
			addr_map_disp_base_r(),
			T234_HWPM_IP_DISPLAY, true);
		if (ret != 0) {
			tegra_hwpm_err(hwpm,
				"T234_HWPM_IP_DISPLAY force enable failed");
			return ret;
		}
#endif
*/

	}
#endif
		/*
		 * SCF is an independent IP with a single perfmon only.
		 * SCF should not be part of force enable config flag.
		 */
#if defined(CONFIG_SOC_HWPM_IP_SCF)
		/* SCF */
		ret = tegra_hwpm_set_fs_info_ip_ops(hwpm, NULL,
			addr_map_rpg_pm_scf_base_r(),
			T234_HWPM_IP_SCF, true);
		if (ret != 0) {
			tegra_hwpm_err(hwpm,
				"T234_HWPM_IP_SCF force enable failed");
			return ret;
		}
#endif

	return ret;
}

int t234_hwpm_get_fs_info(struct tegra_soc_hwpm *hwpm,
	u32 ip_enum, u64 *fs_mask, u8 *ip_status)
{
	u32 ip_idx = 0U, inst_idx = 0U, element_mask_shift = 0U;
	u64 floorsweep = 0ULL;
	struct tegra_soc_hwpm_chip *active_chip = NULL;
	struct hwpm_ip *chip_ip = NULL;
	struct hwpm_ip_inst *ip_inst = NULL;

	tegra_hwpm_fn(hwpm, " ");

	/* Convert tegra_soc_hwpm_ip enum to internal ip index */
	if (hwpm->active_chip->is_ip_active(hwpm, ip_enum, &ip_idx)) {
		active_chip = hwpm->active_chip;
		chip_ip = active_chip->chip_ips[ip_idx];
		if (!(chip_ip->override_enable) && chip_ip->inst_fs_mask) {
			for (inst_idx = 0U; inst_idx < chip_ip->num_instances;
				inst_idx++) {
				ip_inst = &chip_ip->ip_inst_static_array[
					inst_idx];
				element_mask_shift = (inst_idx == 0U ? 0U :
					ip_inst->num_core_elements_per_inst);

				if (ip_inst->hw_inst_mask &
					chip_ip->inst_fs_mask) {
					floorsweep |= ((u64)
						ip_inst->element_fs_mask <<
							element_mask_shift);
				}
			}
			*fs_mask = floorsweep;
			*ip_status = TEGRA_SOC_HWPM_IP_STATUS_VALID;

			return 0;
		}
	}

	tegra_hwpm_dbg(hwpm, hwpm_dbg_floorsweep_info,
		"SOC hwpm IP %d is unavailable", ip_enum);

	*ip_status = TEGRA_SOC_HWPM_IP_STATUS_INVALID;
	*fs_mask = 0ULL;

	return 0;
}

int t234_hwpm_get_resource_info(struct tegra_soc_hwpm *hwpm,
	u32 resource_enum, u8 *status)
{
	u32 ip_idx = 0U;
	struct tegra_soc_hwpm_chip *active_chip = hwpm->active_chip;
	struct hwpm_ip *chip_ip = NULL;

	tegra_hwpm_fn(hwpm, " ");

	/* Convert tegra_soc_hwpm_resource to internal enum */
	if (hwpm->active_chip->is_resource_active(hwpm, resource_enum, &ip_idx)) {
		chip_ip = active_chip->chip_ips[ip_idx];

		if (!(chip_ip->override_enable)) {
			*status = tegra_hwpm_safe_cast_u32_to_u8(
				chip_ip->resource_status);
			return 0;
		}
	}

	*status = tegra_hwpm_safe_cast_u32_to_u8(
		TEGRA_HWPM_RESOURCE_STATUS_INVALID);

	return 0;
}
