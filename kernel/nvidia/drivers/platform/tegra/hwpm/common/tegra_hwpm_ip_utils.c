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

#include <tegra_hwpm.h>
#include <tegra_hwpm_io.h>
#include <tegra_hwpm_log.h>
#include <tegra_hwpm_common.h>
#include <tegra_hwpm_static_analysis.h>

int tegra_hwpm_get_floorsweep_info(struct tegra_soc_hwpm *hwpm,
	struct tegra_soc_hwpm_ip_floorsweep_info *fs_info)
{
	int ret = 0;
	u32 i = 0U;

	tegra_hwpm_fn(hwpm, " ");

	for (i = 0U; i < fs_info->num_queries; i++) {
		ret = hwpm->active_chip->get_fs_info(
			hwpm, (u32)fs_info->ip_fsinfo[i].ip,
			&fs_info->ip_fsinfo[i].ip_inst_mask,
			&fs_info->ip_fsinfo[i].status);
		if (ret < 0) {
			/* Print error for debug purpose. */
			tegra_hwpm_err(hwpm, "Failed to get fs_info");
		}

		tegra_hwpm_dbg(hwpm, hwpm_info | hwpm_dbg_floorsweep_info,
			"Query %d: ip %d: ip_status: %d inst_mask 0x%llx",
			i, fs_info->ip_fsinfo[i].ip,
			fs_info->ip_fsinfo[i].status,
			fs_info->ip_fsinfo[i].ip_inst_mask);
	}
	return ret;
}

int tegra_hwpm_get_resource_info(struct tegra_soc_hwpm *hwpm,
	struct tegra_soc_hwpm_resource_info *rsrc_info)
{
	int ret = 0;
	u32 i = 0U;

	tegra_hwpm_fn(hwpm, " ");

	for (i = 0U; i < rsrc_info->num_queries; i++) {
		ret = hwpm->active_chip->get_resource_info(
			hwpm, (u32)rsrc_info->resource_info[i].resource,
			&rsrc_info->resource_info[i].status);
		if (ret < 0) {
			/* Print error for debug purpose. */
			tegra_hwpm_err(hwpm, "Failed to get rsrc_info");
		}

		tegra_hwpm_dbg(hwpm, hwpm_info | hwpm_dbg_resource_info,
			"Query %d: resource %d: status: %d",
			i, rsrc_info->resource_info[i].resource,
			rsrc_info->resource_info[i].status);
	}
	return ret;
}

int tegra_hwpm_ip_handle_power_mgmt(struct tegra_soc_hwpm *hwpm,
	struct hwpm_ip_inst *ip_inst, bool disable)
{
	int err = 0;
	/*
	 * Since perfmux is controlled by IP, indicate monitoring enabled
	 * by disabling IP power management.
	 * disable = false: start of profiling session
	 * disable = true: end of profiling session
	 */
	/* Make sure that ip_ops are initialized */
	if ((ip_inst->ip_ops.ip_dev != NULL) &&
		(ip_inst->ip_ops.hwpm_ip_pm != NULL)) {
		err = (*ip_inst->ip_ops.hwpm_ip_pm)(
			ip_inst->ip_ops.ip_dev, disable);
		if (err != 0) {
			tegra_hwpm_err(hwpm, "Runtime PM %s failed",
				disable == true ? "disable" : "enable");
		}
	} else {
		tegra_hwpm_dbg(hwpm, hwpm_dbg_reserve_resource,
			"Runtime PM not configured");
	}

	return err;
}

static int tegra_hwpm_update_ip_inst_fs_mask(struct tegra_soc_hwpm *hwpm,
	u32 ip_idx, u32 a_type, u32 inst_idx, bool available)
{
	struct tegra_soc_hwpm_chip *active_chip = hwpm->active_chip;
	struct hwpm_ip *chip_ip = active_chip->chip_ips[ip_idx];
	struct hwpm_ip_inst_per_aperture_info *inst_a_info =
		&chip_ip->inst_aperture_info[a_type];
	struct hwpm_ip_inst *ip_inst = inst_a_info->inst_arr[inst_idx];
	int ret = 0;

	tegra_hwpm_fn(hwpm, " ");

	/* Update inst fs info */
	if (available) {
		chip_ip->inst_fs_mask |= ip_inst->hw_inst_mask;
		chip_ip->resource_status = TEGRA_HWPM_RESOURCE_STATUS_VALID;

		if (hwpm->device_opened) {
			/*
			 * IP fs_info is updated during device open call
			 * However, if IP registers after HWPM device was open,
			 * this function call will update IP element mask
			 */
			ret = tegra_hwpm_func_single_ip(hwpm, NULL,
				TEGRA_HWPM_UPDATE_IP_INST_MASK, ip_idx);
			if (ret != 0) {
				tegra_hwpm_err(hwpm,
					"IP %d Failed to update fs_info",
					ip_idx);
				return ret;
			}
		}
	} else {
		chip_ip->inst_fs_mask &= ~(ip_inst->hw_inst_mask);
		if (chip_ip->inst_fs_mask == 0U) {
			chip_ip->resource_status =
				TEGRA_HWPM_RESOURCE_STATUS_INVALID;
		}
	}

	return 0;
}

static int tegra_hwpm_update_ip_ops_info(struct tegra_soc_hwpm *hwpm,
	struct tegra_soc_hwpm_ip_ops *hwpm_ip_ops,
	u32 ip_idx, u32 a_type, u32 inst_idx, bool available)
{
	struct tegra_soc_hwpm_chip *active_chip = hwpm->active_chip;
	struct hwpm_ip *chip_ip = active_chip->chip_ips[ip_idx];
	struct hwpm_ip_inst_per_aperture_info *inst_a_info =
		&chip_ip->inst_aperture_info[a_type];
	struct hwpm_ip_inst *ip_inst = inst_a_info->inst_arr[inst_idx];
	/* Update IP ops info for the instance */
	struct tegra_hwpm_ip_ops *ip_ops = &ip_inst->ip_ops;

	tegra_hwpm_fn(hwpm, " ");

	if (available) {
		ip_ops->ip_dev = hwpm_ip_ops->ip_dev;
		ip_ops->hwpm_ip_pm = hwpm_ip_ops->hwpm_ip_pm;
		ip_ops->hwpm_ip_reg_op = hwpm_ip_ops->hwpm_ip_reg_op;
	} else {
		ip_ops->ip_dev = NULL;
		ip_ops->hwpm_ip_pm = NULL;
		ip_ops->hwpm_ip_reg_op = NULL;
	}

	return 0;
}

/*
 * Find IP hw instance mask and update IP floorsweep info and IP ops.
 */
int tegra_hwpm_set_fs_info_ip_ops(struct tegra_soc_hwpm *hwpm,
	struct tegra_soc_hwpm_ip_ops *hwpm_ip_ops,
	u64 base_address, u32 ip_idx, bool available)
{
	int ret = 0;
	bool found = false;
	u32 idx = ip_idx;
	u32 inst_idx = 0U, element_idx = 0U;
	u32 a_type = 0U;
	enum tegra_hwpm_element_type element_type = HWPM_ELEMENT_INVALID;

	tegra_hwpm_fn(hwpm, " ");

	/* Find IP aperture containing phys_addr in allowlist */
	found = tegra_hwpm_aperture_for_address(hwpm,
		TEGRA_HWPM_MATCH_BASE_ADDRESS, base_address,
		&idx, &inst_idx, &element_idx, &element_type);
	if (!found) {
		tegra_hwpm_err(hwpm, "Base addr 0x%llx not in IP %d",
			base_address, idx);
		return -EINVAL;
	}

	tegra_hwpm_dbg(hwpm, hwpm_dbg_ip_register,
		"Found addr 0x%llx IP %d inst_idx %d element_idx %d e_type %d",
		base_address, idx, inst_idx, element_idx, element_type);

	switch (element_type) {
	case HWPM_ELEMENT_PERFMON:
		a_type = TEGRA_HWPM_APERTURE_TYPE_PERFMON;
		break;
	case HWPM_ELEMENT_PERFMUX:
	case IP_ELEMENT_PERFMUX:
		a_type = TEGRA_HWPM_APERTURE_TYPE_PERFMUX;
		break;
	case IP_ELEMENT_BROADCAST:
		a_type = TEGRA_HWPM_APERTURE_TYPE_BROADCAST;
		break;
	case HWPM_ELEMENT_INVALID:
	default:
		tegra_hwpm_err(hwpm, "Invalid element type %d", element_type);
	}

	if (hwpm_ip_ops != NULL) {
		/* Update IP ops */
		ret = tegra_hwpm_update_ip_ops_info(hwpm, hwpm_ip_ops,
			ip_idx, a_type, inst_idx, available);
		if (ret != 0) {
			tegra_hwpm_err(hwpm,
				"IP %d inst_idx %d: Failed to update ip_ops",
				ip_idx, inst_idx);
			goto fail;
		}
	}

	ret = tegra_hwpm_update_ip_inst_fs_mask(hwpm, ip_idx, a_type,
		inst_idx, available);
	if (ret != 0) {
		tegra_hwpm_err(hwpm,
			"IP %d inst_idx %d: Failed to update fs_info",
			ip_idx, inst_idx);
		goto fail;
	}

fail:
	return ret;
}

static int tegra_hwpm_complete_ip_register(struct tegra_soc_hwpm *hwpm)
{
	int ret = 0;
	struct hwpm_ip_register_list *node = ip_register_list_head;

	tegra_hwpm_fn(hwpm, " ");

	while (node != NULL) {
		ret = hwpm->active_chip->extract_ip_ops(
			hwpm, &node->ip_ops, true);
		if (ret != 0) {
			tegra_hwpm_err(hwpm,
				"Resource enum %d extract IP ops failed",
				node->ip_ops.resource_enum);
			return ret;
		}
		node = node->next;
	}
	return ret;
}

/*
 * There are 3 ways to get info about available IPs
 * 1. IP register to HWPM driver
 * 2. IP register to HWPM before HWPM driver is probed
 * 3. Force enabled IPs
 *
 * This function will handle case 2 and 3
 */
int tegra_hwpm_finalize_chip_info(struct tegra_soc_hwpm *hwpm)
{
	int ret = 0;

	tegra_hwpm_fn(hwpm, " ");

	/*
	 * Go through IP registration requests received before HWPM
	 * driver was probed.
	 */
	ret = tegra_hwpm_complete_ip_register(hwpm);
	if (ret != 0) {
		tegra_hwpm_err(hwpm, "Failed register IPs");
		return ret;
	}

	ret = hwpm->active_chip->force_enable_ips(hwpm);
	if (ret != 0) {
		tegra_hwpm_err(hwpm, "Failed to force enable IPs");
		/* Do not fail because of force enable failure */
		return 0;
	}

	return 0;
}

static bool tegra_hwpm_addr_in_single_element(struct tegra_soc_hwpm *hwpm,
	enum tegra_hwpm_funcs iia_func,
	u64 find_addr, u32 *ip_idx, u32 *inst_idx, u32 *element_idx,
	enum tegra_hwpm_element_type *element_type, u32 a_type)
{
	struct tegra_soc_hwpm_chip *active_chip = hwpm->active_chip;
	struct hwpm_ip *chip_ip = active_chip->chip_ips[*ip_idx];
	struct hwpm_ip_inst_per_aperture_info *inst_a_info =
			&chip_ip->inst_aperture_info[a_type];
	struct hwpm_ip_inst *ip_inst = inst_a_info->inst_arr[*inst_idx];
	struct hwpm_ip_element_info *e_info = &ip_inst->element_info[a_type];
	struct hwpm_ip_aperture *element = e_info->element_arr[*element_idx];

	if (element == NULL) {
		tegra_hwpm_dbg(hwpm, hwpm_verbose,
			"IP %d addr 0x%llx inst_idx %d "
			"a_type %d: element_idx %d not populated",
			*ip_idx, find_addr, *inst_idx, a_type, *element_idx);
		return false;
	}

	if (iia_func == TEGRA_HWPM_FIND_GIVEN_ADDRESS) {
		/* Make sure this instance is available */
		if ((element->element_index_mask &
			ip_inst->element_fs_mask) == 0U) {
			tegra_hwpm_dbg(hwpm, hwpm_dbg_regops,
				"IP %d addr 0x%llx inst_idx %d "
				"a_type %d: element_idx %d: not available",
				*ip_idx, find_addr, *inst_idx, a_type,
				*element_idx);
			return false;
		}

		/* Make sure phys addr belongs to this element */
		if ((find_addr < element->start_abs_pa) ||
			(find_addr > element->end_abs_pa)) {
			tegra_hwpm_err(hwpm, "IP %d addr 0x%llx inst_idx %d "
				"a_type %d element_idx %d: out of bounds",
				*ip_idx, find_addr, *inst_idx, a_type,
				*element_idx);
			return false;
		}

		if (hwpm->active_chip->check_alist(hwpm, element, find_addr)) {
			*element_type = element->element_type;
			return true;
		}

		tegra_hwpm_dbg(hwpm, hwpm_dbg_regops,
			"IP %d addr 0x%llx inst_idx %d "
			"a_type %d element_idx %d address not in alist",
			*ip_idx, find_addr, *inst_idx, a_type,
			*element_idx);
		return false;
	}

	if (iia_func == TEGRA_HWPM_MATCH_BASE_ADDRESS) {
		/* Confirm that given addr is base address of this element */
		if (find_addr != element->start_abs_pa) {
			tegra_hwpm_dbg(hwpm, hwpm_dbg_ip_register,
				"IP %d addr 0x%llx inst_idx %d "
				"a_type %d element_idx %d: addr != start addr",
				*ip_idx, find_addr, *inst_idx, a_type,
				*element_idx);
			return false;
		}
		*element_type = element->element_type;
		return true;
	}

	/* All cases handled, execution shouldn't reach here */
	return false;
}

static bool tegra_hwpm_addr_in_all_elements(struct tegra_soc_hwpm *hwpm,
	enum tegra_hwpm_funcs iia_func,
	u64 find_addr, u32 *ip_idx, u32 *inst_idx, u32 *element_idx,
	enum tegra_hwpm_element_type *element_type, u32 a_type)
{
	struct tegra_soc_hwpm_chip *active_chip = hwpm->active_chip;
	struct hwpm_ip *chip_ip = active_chip->chip_ips[*ip_idx];
	struct hwpm_ip_inst_per_aperture_info *inst_a_info =
			&chip_ip->inst_aperture_info[a_type];
	struct hwpm_ip_inst *ip_inst = inst_a_info->inst_arr[*inst_idx];
	struct hwpm_ip_element_info *e_info = &ip_inst->element_info[a_type];
	u64 element_offset = 0ULL;
	u32 idx;

	/* Make sure address falls in elements of a_type */
	if (e_info->num_element_per_inst == 0U) {
		tegra_hwpm_dbg(hwpm, hwpm_verbose,
			"IP %d addr 0x%llx: inst_idx %d no type %d elements",
			*ip_idx, find_addr, *inst_idx, a_type);
		return false;
	}

	if ((find_addr < e_info->range_start) ||
		(find_addr > e_info->range_end)) {
		/* Address not in this instance corresponding to a_type */
		tegra_hwpm_dbg(hwpm, hwpm_verbose, "IP %d inst_idx %d: "
			"addr 0x%llx not in type %d elements",
			*ip_idx, *inst_idx, find_addr, a_type);
		return false;
	}

	/* Find element index to which address belongs to */
	element_offset = tegra_hwpm_safe_sub_u64(
		find_addr, e_info->range_start);
	idx = tegra_hwpm_safe_cast_u64_to_u32(
		element_offset / e_info->element_stride);

	/* Make sure element index is valid */
	if (idx >= e_info->element_slots) {
		tegra_hwpm_err(hwpm, "IP %d addr 0x%llx inst_idx %d a_type %d: "
			"element_idx %d out of bounds",
			*ip_idx, find_addr, *inst_idx, a_type, idx);
		return false;
	}

	*element_idx = idx;

	/* Process further and return */
	return tegra_hwpm_addr_in_single_element(hwpm, iia_func,
		find_addr, ip_idx, inst_idx, element_idx, element_type, a_type);
}

static bool tegra_hwpm_addr_in_single_instance(struct tegra_soc_hwpm *hwpm,
	enum tegra_hwpm_funcs iia_func,
	u64 find_addr, u32 *ip_idx, u32 *inst_idx, u32 *element_idx,
	enum tegra_hwpm_element_type *element_type, u32 a_type)
{
	struct tegra_soc_hwpm_chip *active_chip = hwpm->active_chip;
	struct hwpm_ip *chip_ip = active_chip->chip_ips[*ip_idx];
	struct hwpm_ip_inst_per_aperture_info *inst_a_info =
			&chip_ip->inst_aperture_info[a_type];
	struct hwpm_ip_inst *ip_inst = inst_a_info->inst_arr[*inst_idx];

	tegra_hwpm_fn(hwpm, " ");

	if (ip_inst == NULL) {
		tegra_hwpm_dbg(hwpm, hwpm_verbose, "IP %d addr 0x%llx: "
			"a_type %d inst_idx %d not populated",
			*ip_idx, find_addr, a_type, *inst_idx);
		return false;
	}

	if (iia_func == TEGRA_HWPM_FIND_GIVEN_ADDRESS) {
		/* Make sure this instance is available */
		if ((chip_ip->inst_fs_mask & ip_inst->hw_inst_mask) == 0U) {
			tegra_hwpm_dbg(hwpm, hwpm_dbg_regops,
				"IP %d addr 0x%llx: "
				"a_type %d inst_idx %d not available",
				*ip_idx, find_addr, a_type, *inst_idx);
			return false;
		}
	}

	/* Process further and return */
	return tegra_hwpm_addr_in_all_elements(hwpm, iia_func,
		find_addr, ip_idx, inst_idx, element_idx, element_type, a_type);
}

static bool tegra_hwpm_addr_in_all_instances(struct tegra_soc_hwpm *hwpm,
	enum tegra_hwpm_funcs iia_func,
	u64 find_addr, u32 *ip_idx, u32 *inst_idx, u32 *element_idx,
	enum tegra_hwpm_element_type *element_type, u32 a_type)
{
	struct tegra_soc_hwpm_chip *active_chip = hwpm->active_chip;
	struct hwpm_ip *chip_ip = active_chip->chip_ips[*ip_idx];
	struct hwpm_ip_inst_per_aperture_info *inst_a_info =
			&chip_ip->inst_aperture_info[a_type];
	u64 inst_offset = 0ULL;
	u32 idx = 0U;

	tegra_hwpm_fn(hwpm, " ");

	/* Find instance to which address belongs to */
	inst_offset = tegra_hwpm_safe_sub_u64(
		find_addr, inst_a_info->range_start);
	idx = tegra_hwpm_safe_cast_u64_to_u32(
		inst_offset / inst_a_info->inst_stride);

	/* Make sure instance index is valid */
	if (idx >= inst_a_info->inst_slots) {
		tegra_hwpm_err(hwpm, "IP %d addr 0x%llx a_type %d: "
			"inst_idx %d out of bounds",
			*ip_idx, find_addr, a_type, idx);
		return false;
	}

	*inst_idx = idx;

	/* Process further and return */
	return tegra_hwpm_addr_in_single_instance(hwpm, iia_func,
		find_addr, ip_idx, inst_idx, element_idx,
		element_type, a_type);
}

static bool tegra_hwpm_addr_in_single_ip(struct tegra_soc_hwpm *hwpm,
	enum tegra_hwpm_funcs iia_func,
	u64 find_addr, u32 *ip_idx, u32 *inst_idx, u32 *element_idx,
	enum tegra_hwpm_element_type *element_type)
{
	struct tegra_soc_hwpm_chip *active_chip = hwpm->active_chip;
	struct hwpm_ip *chip_ip = active_chip->chip_ips[*ip_idx];
	u32 a_type;
	bool found = false;

	tegra_hwpm_fn(hwpm, " ");

	if (chip_ip == NULL) {
		tegra_hwpm_err(hwpm, "IP %d not populated as expected", *ip_idx);
		return false;
	}

	if (chip_ip->override_enable) {
		/* This IP should not be configured for HWPM */
		tegra_hwpm_dbg(hwpm, hwpm_verbose, "IP %d override enabled",
			*ip_idx);
		return false;
	}

	if (iia_func == TEGRA_HWPM_FIND_GIVEN_ADDRESS) {
		/* Make sure this instance is available */
		if (!chip_ip->reserved) {
			tegra_hwpm_dbg(hwpm, hwpm_dbg_regops,
				"IP %d not reserved", *ip_idx);
			return false;
		}
	}

	if (chip_ip->num_instances == 0U) {
		/* No instances in this IP */
		tegra_hwpm_dbg(hwpm, hwpm_verbose,
			"IP %d no instances", *ip_idx);
		return false;
	}

	/* Figure out which aperture type this address belongs to */
	for (a_type = 0U; a_type < TEGRA_HWPM_APERTURE_TYPE_MAX; a_type++) {
		struct hwpm_ip_inst_per_aperture_info *inst_a_info =
			&chip_ip->inst_aperture_info[a_type];

		if ((find_addr < inst_a_info->range_start) ||
			(find_addr > inst_a_info->range_end)) {
			/* Address not in this IP for this a_type */
			tegra_hwpm_dbg(hwpm, hwpm_verbose,
				"IP %d addr 0x%llx not in a_type %d elements",
				*ip_idx, find_addr, a_type);
			continue;
		}

		/* Process further and return */
		found = tegra_hwpm_addr_in_all_instances(hwpm, iia_func,
			find_addr, ip_idx, inst_idx, element_idx,
			element_type, a_type);
		if (found) {
			break;
		}
		/*
		 * Address can belong to other type.
		 * For example, for MC IPs, broadcast aperture base address
		 * falls between perfmux address range. And, element
		 * corresponding to broadcast address in perfmux array is
		 * set to NULL.
		 */
	}
	return found;
}

static bool tegra_hwpm_addr_in_all_ip(struct tegra_soc_hwpm *hwpm,
	enum tegra_hwpm_funcs iia_func,
	u64 find_addr, u32 *ip_idx, u32 *inst_idx, u32 *element_idx,
	enum tegra_hwpm_element_type *element_type)
{
	u32 idx;
	bool found = false;
	struct tegra_soc_hwpm_chip *active_chip = hwpm->active_chip;

	tegra_hwpm_fn(hwpm, " ");

	for (idx = 0U; idx < active_chip->get_ip_max_idx(hwpm); idx++) {
		struct hwpm_ip *chip_ip = active_chip->chip_ips[idx];

		if (chip_ip == NULL) {
			tegra_hwpm_err(hwpm, "IP %d not populated as expected",
				idx);
			return false;
		}

		if (!chip_ip->reserved) {
			tegra_hwpm_dbg(hwpm, hwpm_verbose,
				"IP %d not reserved", *ip_idx);
			continue;
		}

		found = tegra_hwpm_addr_in_single_ip(hwpm, iia_func, find_addr,
			&idx, inst_idx, element_idx, element_type);
		if (found) {
			*ip_idx = idx;
			return true;
		}
	}

	return found;
}

bool tegra_hwpm_aperture_for_address(struct tegra_soc_hwpm *hwpm,
	enum tegra_hwpm_funcs iia_func,
	u64 find_addr, u32 *ip_idx, u32 *inst_idx, u32 *element_idx,
	enum tegra_hwpm_element_type *element_type)
{
	bool found = false;

	tegra_hwpm_fn(hwpm, " ");

	if ((ip_idx == NULL) || (inst_idx == NULL) ||
		(element_idx == NULL) || (element_type == NULL)) {
		tegra_hwpm_err(hwpm, "NULL index pointer");
		return false;
	}

	if (iia_func == TEGRA_HWPM_FIND_GIVEN_ADDRESS) {
		/* IP index is not known, search in all IPs */
		found = tegra_hwpm_addr_in_all_ip(hwpm, iia_func, find_addr,
			ip_idx, inst_idx, element_idx, element_type);
		if (!found) {
			tegra_hwpm_err(hwpm,
				"Address 0x%llx not in any IP", find_addr);
			return found;
		}
	}

	if (iia_func == TEGRA_HWPM_MATCH_BASE_ADDRESS) {
		found = tegra_hwpm_addr_in_single_ip(hwpm, iia_func, find_addr,
			ip_idx, inst_idx, element_idx, element_type);
		if (!found) {
			tegra_hwpm_err(hwpm, "Address 0x%llx not in IP %d",
				find_addr, *ip_idx);
			return found;
		}
	}

	return found;
}
