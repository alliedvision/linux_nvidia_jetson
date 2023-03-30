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
#include <linux/of_address.h>
#include <linux/dma-buf.h>
#include <uapi/linux/tegra-soc-hwpm-uapi.h>

#include <tegra_hwpm_log.h>
#include <tegra_hwpm_io.h>
#include <tegra_hwpm.h>
#include <hal/t234/t234_hwpm_internal.h>

#include <hal/t234/hw/t234_pmasys_soc_hwpm.h>
#include <hal/t234/hw/t234_pmmsys_soc_hwpm.h>

int t234_hwpm_disable_triggers(struct tegra_soc_hwpm *hwpm)
{
	int err = 0;
	u32 reg_val = 0U;
	u32 field_mask = 0U;
	u32 field_val = 0U;
	s32 timeout_msecs = 1000;
	u32 sleep_msecs = 100;
	struct tegra_soc_hwpm_chip *active_chip = hwpm->active_chip;
	struct hwpm_ip *chip_ip = active_chip->chip_ips[
		active_chip->get_rtr_int_idx(hwpm)];
	struct hwpm_ip_inst *ip_inst_rtr = &chip_ip->ip_inst_static_array[
		T234_HWPM_IP_RTR_STATIC_RTR_INST];
	struct hwpm_ip_inst *ip_inst_pma = &chip_ip->ip_inst_static_array[
		T234_HWPM_IP_RTR_STATIC_PMA_INST];
	struct hwpm_ip_aperture *rtr_perfmux = &ip_inst_rtr->element_info[
		TEGRA_HWPM_APERTURE_TYPE_PERFMUX].element_static_array[
			T234_HWPM_IP_RTR_PERMUX_INDEX];
	struct hwpm_ip_aperture *pma_perfmux = &ip_inst_pma->element_info[
		TEGRA_HWPM_APERTURE_TYPE_PERFMUX].element_static_array[
			T234_HWPM_IP_RTR_PERMUX_INDEX];

	tegra_hwpm_fn(hwpm, " ");

	/* Disable PMA triggers */
	err = tegra_hwpm_readl(hwpm, pma_perfmux,
		pmasys_trigger_config_user_r(0), &reg_val);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "hwpm read failed");
		return err;
	}
	reg_val = set_field(reg_val, pmasys_trigger_config_user_pma_pulse_m(),
		pmasys_trigger_config_user_pma_pulse_disable_f());
	err = tegra_hwpm_writel(hwpm, pma_perfmux,
		pmasys_trigger_config_user_r(0), reg_val);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "hwpm write failed");
		return err;
	}

	err = tegra_hwpm_writel(hwpm, pma_perfmux,
		pmasys_sys_trigger_start_mask_r(), 0);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "hwpm write failed");
		return err;
	}
	err = tegra_hwpm_writel(hwpm, pma_perfmux,
		pmasys_sys_trigger_start_maskb_r(), 0);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "hwpm write failed");
		return err;
	}
	err = tegra_hwpm_writel(hwpm, pma_perfmux,
		pmasys_sys_trigger_stop_mask_r(), 0);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "hwpm write failed");
		return err;
	}
	err = tegra_hwpm_writel(hwpm, pma_perfmux,
		pmasys_sys_trigger_stop_maskb_r(), 0);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "hwpm write failed");
		return err;
	}

	/* Wait for PERFMONs, ROUTER, and PMA to idle */
	do {
		err = tegra_hwpm_readl(hwpm, rtr_perfmux,
			pmmsys_sys0router_perfmonstatus_r(), &reg_val);
		if (err != 0) {
			tegra_hwpm_err(hwpm, "hwpm read failed");
			return err;
		}
		msleep(sleep_msecs);
		timeout_msecs -= sleep_msecs;
	} while ((pmmsys_sys0router_perfmonstatus_merged_v(reg_val) != 0U) &&
		(timeout_msecs > 0));

	if (timeout_msecs <= 0) {
		tegra_hwpm_err(hwpm, "Timeout expired for "
			"NV_PERF_PMMSYS_SYS0ROUTER_PERFMONSTATUS_MERGED_EMPTY");
		return -ETIMEDOUT;
	}

	timeout_msecs = 1000;

	do {
		err = tegra_hwpm_readl(hwpm, rtr_perfmux,
			pmmsys_sys0router_enginestatus_r(), &reg_val);
		if (err != 0) {
			tegra_hwpm_err(hwpm, "hwpm read failed");
			return err;
		}
		msleep(sleep_msecs);
		timeout_msecs -= sleep_msecs;
	} while ((pmmsys_sys0router_enginestatus_status_v(reg_val) !=
			pmmsys_sys0router_enginestatus_status_empty_v()) &&
		(timeout_msecs > 0));
	if (timeout_msecs <= 0) {
		tegra_hwpm_err(hwpm, "Timeout expired for "
			"NV_PERF_PMMSYS_SYS0ROUTER_ENGINESTATUS_STATUS_EMPTY");
		return -ETIMEDOUT;
	}

	timeout_msecs = 1000;

	field_mask = pmasys_enginestatus_status_m() |
		     pmasys_enginestatus_rbufempty_m();
	field_val = pmasys_enginestatus_status_empty_f() |
		pmasys_enginestatus_rbufempty_empty_f();
	do {
		err = tegra_hwpm_readl(hwpm, pma_perfmux,
			pmasys_enginestatus_r(), &reg_val);
		if (err != 0) {
			tegra_hwpm_err(hwpm, "hwpm read failed");
			return err;
		}
		msleep(sleep_msecs);
		timeout_msecs -= sleep_msecs;
	} while (((reg_val & field_mask) != field_val) && (timeout_msecs > 0));

	if (timeout_msecs <= 0) {
		tegra_hwpm_err(hwpm, "Timeout expired for "
			"NV_PERF_PMASYS_ENGINESTATUS");
		return -ETIMEDOUT;
	}

	return err;
}

int t234_hwpm_init_prod_values(struct tegra_soc_hwpm *hwpm)
{
	int err = 0;
	u32 val = 0U;
	struct tegra_soc_hwpm_chip *active_chip = hwpm->active_chip;
	struct hwpm_ip *chip_ip = active_chip->chip_ips[
		active_chip->get_rtr_int_idx(hwpm)];
	struct hwpm_ip_inst *ip_inst_pma = &chip_ip->ip_inst_static_array[
		T234_HWPM_IP_RTR_STATIC_PMA_INST];
	struct hwpm_ip_aperture *pma_perfmux = &ip_inst_pma->element_info[
		TEGRA_HWPM_APERTURE_TYPE_PERFMUX].element_static_array[
			T234_HWPM_IP_RTR_PERMUX_INDEX];

	tegra_hwpm_fn(hwpm, " ");

	err = tegra_hwpm_readl(hwpm, pma_perfmux, pmasys_controlb_r(), &val);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "hwpm read failed");
		return err;
	}
	val = set_field(val, pmasys_controlb_coalesce_timeout_cycles_m(),
			pmasys_controlb_coalesce_timeout_cycles__prod_f());
	err = tegra_hwpm_writel(hwpm, pma_perfmux, pmasys_controlb_r(), val);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "hwpm write failed");
		return err;
	}

	err = tegra_hwpm_readl(hwpm, pma_perfmux,
		pmasys_channel_config_user_r(0), &val);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "hwpm read failed");
		return err;
	}
	val = set_field(val,
		pmasys_channel_config_user_coalesce_timeout_cycles_m(),
		pmasys_channel_config_user_coalesce_timeout_cycles__prod_f());
	err = tegra_hwpm_writel(hwpm, pma_perfmux,
		pmasys_channel_config_user_r(0), val);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "hwpm write failed");
		return err;
	}

	return 0;
}

int t234_hwpm_disable_slcg(struct tegra_soc_hwpm *hwpm)
{
	int err = 0;
	u32 field_mask = 0U;
	u32 field_val = 0U;
	u32 reg_val = 0U;
	struct tegra_soc_hwpm_chip *active_chip = hwpm->active_chip;
	struct hwpm_ip *chip_ip = active_chip->chip_ips[
		active_chip->get_rtr_int_idx(hwpm)];
	struct hwpm_ip_inst *ip_inst_rtr = &chip_ip->ip_inst_static_array[
		T234_HWPM_IP_RTR_STATIC_RTR_INST];
	struct hwpm_ip_inst *ip_inst_pma = &chip_ip->ip_inst_static_array[
		T234_HWPM_IP_RTR_STATIC_PMA_INST];
	struct hwpm_ip_aperture *rtr_perfmux = &ip_inst_rtr->element_info[
		TEGRA_HWPM_APERTURE_TYPE_PERFMUX].element_static_array[
			T234_HWPM_IP_RTR_PERMUX_INDEX];
	struct hwpm_ip_aperture *pma_perfmux = &ip_inst_pma->element_info[
		TEGRA_HWPM_APERTURE_TYPE_PERFMUX].element_static_array[
			T234_HWPM_IP_RTR_PERMUX_INDEX];

	err = tegra_hwpm_readl(hwpm, pma_perfmux, pmasys_cg2_r(), &reg_val);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "hwpm read failed");
		return err;
	}
	reg_val = set_field(reg_val, pmasys_cg2_slcg_m(),
			pmasys_cg2_slcg_disabled_f());
	err = tegra_hwpm_writel(hwpm, pma_perfmux, pmasys_cg2_r(), reg_val);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "hwpm write failed");
		return err;
	}

	field_mask = pmmsys_sys0router_cg2_slcg_perfmon_m() |
		pmmsys_sys0router_cg2_slcg_router_m() |
		pmmsys_sys0router_cg2_slcg_m();
	field_val = pmmsys_sys0router_cg2_slcg_perfmon_disabled_f() |
		pmmsys_sys0router_cg2_slcg_router_disabled_f() |
		pmmsys_sys0router_cg2_slcg_disabled_f();
	err = tegra_hwpm_readl(hwpm, rtr_perfmux,
		pmmsys_sys0router_cg2_r(), &reg_val);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "hwpm read failed");
		return err;
	}
	reg_val = set_field(reg_val, field_mask, field_val);
	err = tegra_hwpm_writel(hwpm, rtr_perfmux,
		pmmsys_sys0router_cg2_r(), reg_val);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "hwpm write failed");
		return err;
	}

	return 0;
}

int t234_hwpm_enable_slcg(struct tegra_soc_hwpm *hwpm)
{
	int err = 0;
	u32 reg_val = 0U;
	u32 field_mask = 0U;
	u32 field_val = 0U;
	struct tegra_soc_hwpm_chip *active_chip = hwpm->active_chip;
	struct hwpm_ip *chip_ip = active_chip->chip_ips[
		active_chip->get_rtr_int_idx(hwpm)];
	struct hwpm_ip_inst *ip_inst_rtr = &chip_ip->ip_inst_static_array[
		T234_HWPM_IP_RTR_STATIC_RTR_INST];
	struct hwpm_ip_inst *ip_inst_pma = &chip_ip->ip_inst_static_array[
		T234_HWPM_IP_RTR_STATIC_PMA_INST];
	struct hwpm_ip_aperture *rtr_perfmux = &ip_inst_rtr->element_info[
		TEGRA_HWPM_APERTURE_TYPE_PERFMUX].element_static_array[
			T234_HWPM_IP_RTR_PERMUX_INDEX];
	struct hwpm_ip_aperture *pma_perfmux = &ip_inst_pma->element_info[
		TEGRA_HWPM_APERTURE_TYPE_PERFMUX].element_static_array[
			T234_HWPM_IP_RTR_PERMUX_INDEX];

	err = tegra_hwpm_readl(hwpm, pma_perfmux, pmasys_cg2_r(), &reg_val);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "hwpm read failed");
		return err;
	}
	reg_val = set_field(reg_val, pmasys_cg2_slcg_m(),
		pmasys_cg2_slcg_enabled_f());
	err = tegra_hwpm_writel(hwpm, pma_perfmux, pmasys_cg2_r(), reg_val);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "hwpm write failed");
		return err;
	}

	field_mask = pmmsys_sys0router_cg2_slcg_perfmon_m() |
		pmmsys_sys0router_cg2_slcg_router_m() |
		pmmsys_sys0router_cg2_slcg_m();
	field_val = pmmsys_sys0router_cg2_slcg_perfmon__prod_f() |
		pmmsys_sys0router_cg2_slcg_router__prod_f() |
		pmmsys_sys0router_cg2_slcg__prod_f();
	err = tegra_hwpm_readl(hwpm, rtr_perfmux,
		pmmsys_sys0router_cg2_r(), &reg_val);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "hwpm read failed");
		return err;
	}
	reg_val = set_field(reg_val, field_mask, field_val);
	err = tegra_hwpm_writel(hwpm, rtr_perfmux,
		pmmsys_sys0router_cg2_r(), reg_val);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "hwpm write failed");
		return err;
	}

	return 0;
}
