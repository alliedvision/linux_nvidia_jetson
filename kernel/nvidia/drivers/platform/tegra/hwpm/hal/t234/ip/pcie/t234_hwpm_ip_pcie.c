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

#include "t234_hwpm_ip_pcie.h"

#include <tegra_hwpm.h>
#include <hal/t234/t234_hwpm_regops_allowlist.h>
#include <hal/t234/hw/t234_addr_map_soc_hwpm.h>

static struct hwpm_ip_aperture t234_pcie_inst0_perfmon_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST] = {
	{
		.element_type = HWPM_ELEMENT_PERFMON,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = "perfmon_pcie0",
		.start_abs_pa = addr_map_rpg_pm_pcie_c0_base_r(),
		.end_abs_pa = addr_map_rpg_pm_pcie_c0_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = addr_map_rpg_pm_base_r(),
		.alist = t234_perfmon_alist,
		.alist_size = ARRAY_SIZE(t234_perfmon_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst1_perfmon_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST] = {
	{
		.element_type = HWPM_ELEMENT_PERFMON,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = "perfmon_pcie1",
		.start_abs_pa = addr_map_rpg_pm_pcie_c1_base_r(),
		.end_abs_pa = addr_map_rpg_pm_pcie_c1_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = addr_map_rpg_pm_base_r(),
		.alist = t234_perfmon_alist,
		.alist_size = ARRAY_SIZE(t234_perfmon_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst2_perfmon_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST] = {
	{
		.element_type = HWPM_ELEMENT_PERFMON,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = "perfmon_pcie2",
		.start_abs_pa = addr_map_rpg_pm_pcie_c2_base_r(),
		.end_abs_pa = addr_map_rpg_pm_pcie_c2_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = addr_map_rpg_pm_base_r(),
		.alist = t234_perfmon_alist,
		.alist_size = ARRAY_SIZE(t234_perfmon_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst3_perfmon_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST] = {
	{
		.element_type = HWPM_ELEMENT_PERFMON,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = "perfmon_pcie3",
		.start_abs_pa = addr_map_rpg_pm_pcie_c3_base_r(),
		.end_abs_pa = addr_map_rpg_pm_pcie_c3_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = addr_map_rpg_pm_base_r(),
		.alist = t234_perfmon_alist,
		.alist_size = ARRAY_SIZE(t234_perfmon_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst4_perfmon_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST] = {
	{
		.element_type = HWPM_ELEMENT_PERFMON,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = "perfmon_pcie4",
		.start_abs_pa = addr_map_rpg_pm_pcie_c4_base_r(),
		.end_abs_pa = addr_map_rpg_pm_pcie_c4_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = addr_map_rpg_pm_base_r(),
		.alist = t234_perfmon_alist,
		.alist_size = ARRAY_SIZE(t234_perfmon_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst5_perfmon_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST] = {
	{
		.element_type = HWPM_ELEMENT_PERFMON,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = "perfmon_pcie5",
		.start_abs_pa = addr_map_rpg_pm_pcie_c5_base_r(),
		.end_abs_pa = addr_map_rpg_pm_pcie_c5_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = addr_map_rpg_pm_base_r(),
		.alist = t234_perfmon_alist,
		.alist_size = ARRAY_SIZE(t234_perfmon_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst6_perfmon_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST] = {
	{
		.element_type = HWPM_ELEMENT_PERFMON,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = "perfmon_pcie6",
		.start_abs_pa = addr_map_rpg_pm_pcie_c6_base_r(),
		.end_abs_pa = addr_map_rpg_pm_pcie_c6_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = addr_map_rpg_pm_base_r(),
		.alist = t234_perfmon_alist,
		.alist_size = ARRAY_SIZE(t234_perfmon_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst7_perfmon_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST] = {
	{
		.element_type = HWPM_ELEMENT_PERFMON,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = "perfmon_pcie7",
		.start_abs_pa = addr_map_rpg_pm_pcie_c7_base_r(),
		.end_abs_pa = addr_map_rpg_pm_pcie_c7_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = addr_map_rpg_pm_base_r(),
		.alist = t234_perfmon_alist,
		.alist_size = ARRAY_SIZE(t234_perfmon_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst8_perfmon_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST] = {
	{
		.element_type = HWPM_ELEMENT_PERFMON,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = "perfmon_pcie8",
		.start_abs_pa = addr_map_rpg_pm_pcie_c8_base_r(),
		.end_abs_pa = addr_map_rpg_pm_pcie_c8_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = addr_map_rpg_pm_base_r(),
		.alist = t234_perfmon_alist,
		.alist_size = ARRAY_SIZE(t234_perfmon_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst9_perfmon_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST] = {
	{
		.element_type = HWPM_ELEMENT_PERFMON,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = "perfmon_pcie9",
		.start_abs_pa = addr_map_rpg_pm_pcie_c9_base_r(),
		.end_abs_pa = addr_map_rpg_pm_pcie_c9_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = addr_map_rpg_pm_base_r(),
		.alist = t234_perfmon_alist,
		.alist_size = ARRAY_SIZE(t234_perfmon_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst10_perfmon_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST] = {
	{
		.element_type = HWPM_ELEMENT_PERFMON,
		.element_index_mask = BIT(10),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = "perfmon_pcie10",
		.start_abs_pa = addr_map_rpg_pm_pcie_c10_base_r(),
		.end_abs_pa = addr_map_rpg_pm_pcie_c10_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = addr_map_rpg_pm_base_r(),
		.alist = t234_perfmon_alist,
		.alist_size = ARRAY_SIZE(t234_perfmon_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst0_perfmux_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST] = {
	{
		.element_type = IP_ELEMENT_PERFMUX,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = {'\0'},
		.start_abs_pa = addr_map_pcie_c0_ctl_base_r(),
		.end_abs_pa = addr_map_pcie_c0_ctl_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = 0ULL,
		.alist = t234_pcie_ctl_alist,
		.alist_size = ARRAY_SIZE(t234_pcie_ctl_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst1_perfmux_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST] = {
	{
		.element_type = IP_ELEMENT_PERFMUX,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = {'\0'},
		.start_abs_pa = addr_map_pcie_c1_ctl_base_r(),
		.end_abs_pa = addr_map_pcie_c1_ctl_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = 0ULL,
		.alist = t234_pcie_ctl_alist,
		.alist_size = ARRAY_SIZE(t234_pcie_ctl_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst2_perfmux_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST] = {
	{
		.element_type = IP_ELEMENT_PERFMUX,
		.element_index_mask = BIT(10),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = {'\0'},
		.start_abs_pa = addr_map_pcie_c2_ctl_base_r(),
		.end_abs_pa = addr_map_pcie_c2_ctl_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = 0ULL,
		.alist = t234_pcie_ctl_alist,
		.alist_size = ARRAY_SIZE(t234_pcie_ctl_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst3_perfmux_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST] = {
	{
		.element_type = IP_ELEMENT_PERFMUX,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = {'\0'},
		.start_abs_pa = addr_map_pcie_c3_ctl_base_r(),
		.end_abs_pa = addr_map_pcie_c3_ctl_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = 0ULL,
		.alist = t234_pcie_ctl_alist,
		.alist_size = ARRAY_SIZE(t234_pcie_ctl_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst4_perfmux_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST] = {
	{
		.element_type = IP_ELEMENT_PERFMUX,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = {'\0'},
		.start_abs_pa = addr_map_pcie_c4_ctl_base_r(),
		.end_abs_pa = addr_map_pcie_c4_ctl_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = 0ULL,
		.alist = t234_pcie_ctl_alist,
		.alist_size = ARRAY_SIZE(t234_pcie_ctl_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst5_perfmux_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST] = {
	{
		.element_type = IP_ELEMENT_PERFMUX,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = {'\0'},
		.start_abs_pa = addr_map_pcie_c5_ctl_base_r(),
		.end_abs_pa = addr_map_pcie_c5_ctl_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = 0ULL,
		.alist = t234_pcie_ctl_alist,
		.alist_size = ARRAY_SIZE(t234_pcie_ctl_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst6_perfmux_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST] = {
	{
		.element_type = IP_ELEMENT_PERFMUX,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = {'\0'},
		.start_abs_pa = addr_map_pcie_c6_ctl_base_r(),
		.end_abs_pa = addr_map_pcie_c6_ctl_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = 0ULL,
		.alist = t234_pcie_ctl_alist,
		.alist_size = ARRAY_SIZE(t234_pcie_ctl_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst7_perfmux_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST] = {
	{
		.element_type = IP_ELEMENT_PERFMUX,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = {'\0'},
		.start_abs_pa = addr_map_pcie_c7_ctl_base_r(),
		.end_abs_pa = addr_map_pcie_c7_ctl_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = 0ULL,
		.alist = t234_pcie_ctl_alist,
		.alist_size = ARRAY_SIZE(t234_pcie_ctl_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst8_perfmux_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST] = {
	{
		.element_type = IP_ELEMENT_PERFMUX,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = {'\0'},
		.start_abs_pa = addr_map_pcie_c8_ctl_base_r(),
		.end_abs_pa = addr_map_pcie_c8_ctl_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = 0ULL,
		.alist = t234_pcie_ctl_alist,
		.alist_size = ARRAY_SIZE(t234_pcie_ctl_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst9_perfmux_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST] = {
	{
		.element_type = IP_ELEMENT_PERFMUX,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = {'\0'},
		.start_abs_pa = addr_map_pcie_c9_ctl_base_r(),
		.end_abs_pa = addr_map_pcie_c9_ctl_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = 0ULL,
		.alist = t234_pcie_ctl_alist,
		.alist_size = ARRAY_SIZE(t234_pcie_ctl_alist),
		.fake_registers = NULL,
	},
};

static struct hwpm_ip_aperture t234_pcie_inst10_perfmux_element_static_array[
	T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST] = {
	{
		.element_type = IP_ELEMENT_PERFMUX,
		.element_index_mask = BIT(0),
		.dt_index = 0U,
		.dt_mmio = NULL,
		.name = {'\0'},
		.start_abs_pa = addr_map_pcie_c10_ctl_base_r(),
		.end_abs_pa = addr_map_pcie_c10_ctl_limit_r(),
		.start_pa = 0ULL,
		.end_pa = 0ULL,
		.base_pa = 0ULL,
		.alist = t234_pcie_ctl_alist,
		.alist_size = ARRAY_SIZE(t234_pcie_ctl_alist),
		.fake_registers = NULL,
	},
};

/* IP instance array */
static struct hwpm_ip_inst t234_pcie_inst_static_array[
	T234_HWPM_IP_PCIE_NUM_INSTANCES] = {
	{
		.hw_inst_mask = BIT(0),
		.num_core_elements_per_inst =
			T234_HWPM_IP_PCIE_NUM_CORE_ELEMENT_PER_INST,
		.element_info = {
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMUX
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST,
				.element_static_array =
					t234_pcie_inst0_perfmux_element_static_array,
				.range_start = addr_map_pcie_c0_ctl_base_r(),
				.range_end = addr_map_pcie_c0_ctl_limit_r(),
				.element_stride = addr_map_pcie_c0_ctl_limit_r() -
					addr_map_pcie_c0_ctl_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_BROADCAST
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_BROADCAST_PER_INST,
				.element_static_array = NULL,
				.range_start = 0ULL,
				.range_end = 0ULL,
				.element_stride = 0ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMON
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST,
				.element_static_array =
					t234_pcie_inst0_perfmon_element_static_array,
				.range_start = addr_map_rpg_pm_pcie_c0_base_r(),
				.range_end = addr_map_rpg_pm_pcie_c0_limit_r(),
				.element_stride = addr_map_rpg_pm_pcie_c0_limit_r() -
					addr_map_rpg_pm_pcie_c0_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
		},

		.ip_ops = {
			.ip_dev = NULL,
			.hwpm_ip_pm = NULL,
			.hwpm_ip_reg_op = NULL,
		},

		.element_fs_mask = 0U,
	},
	{
		.hw_inst_mask = BIT(1),
		.num_core_elements_per_inst =
			T234_HWPM_IP_PCIE_NUM_CORE_ELEMENT_PER_INST,
		.element_info = {
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMUX
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST,
				.element_static_array =
					t234_pcie_inst1_perfmux_element_static_array,
				.range_start = addr_map_pcie_c1_ctl_base_r(),
				.range_end = addr_map_pcie_c1_ctl_limit_r(),
				.element_stride = addr_map_pcie_c1_ctl_limit_r() -
					addr_map_pcie_c1_ctl_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_BROADCAST
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_BROADCAST_PER_INST,
				.element_static_array = NULL,
				.range_start = 0ULL,
				.range_end = 0ULL,
				.element_stride = 0ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMON
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST,
				.element_static_array =
					t234_pcie_inst1_perfmon_element_static_array,
				.range_start = addr_map_rpg_pm_pcie_c1_base_r(),
				.range_end = addr_map_rpg_pm_pcie_c1_limit_r(),
				.element_stride = addr_map_rpg_pm_pcie_c1_limit_r() -
					addr_map_rpg_pm_pcie_c1_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
		},

		.ip_ops = {
			.ip_dev = NULL,
			.hwpm_ip_pm = NULL,
			.hwpm_ip_reg_op = NULL,
		},

		.element_fs_mask = 0U,
	},
	{
		.hw_inst_mask = BIT(2),
		.num_core_elements_per_inst =
			T234_HWPM_IP_PCIE_NUM_CORE_ELEMENT_PER_INST,
		.element_info = {
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMUX
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST,
				.element_static_array =
					t234_pcie_inst2_perfmux_element_static_array,
				.range_start = addr_map_pcie_c2_ctl_base_r(),
				.range_end = addr_map_pcie_c2_ctl_limit_r(),
				.element_stride = addr_map_pcie_c2_ctl_limit_r() -
					addr_map_pcie_c2_ctl_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_BROADCAST
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_BROADCAST_PER_INST,
				.element_static_array = NULL,
				.range_start = 0ULL,
				.range_end = 0ULL,
				.element_stride = 0ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMON
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST,
				.element_static_array =
					t234_pcie_inst2_perfmon_element_static_array,
				.range_start = addr_map_rpg_pm_pcie_c2_base_r(),
				.range_end = addr_map_rpg_pm_pcie_c2_limit_r(),
				.element_stride = addr_map_rpg_pm_pcie_c2_limit_r() -
					addr_map_rpg_pm_pcie_c2_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
		},

		.ip_ops = {
			.ip_dev = NULL,
			.hwpm_ip_pm = NULL,
			.hwpm_ip_reg_op = NULL,
		},

		.element_fs_mask = 0U,
	},
	{
		.hw_inst_mask = BIT(3),
		.num_core_elements_per_inst =
			T234_HWPM_IP_PCIE_NUM_CORE_ELEMENT_PER_INST,
		.element_info = {
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMUX
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST,
				.element_static_array =
					t234_pcie_inst3_perfmux_element_static_array,
				.range_start = addr_map_pcie_c3_ctl_base_r(),
				.range_end = addr_map_pcie_c3_ctl_limit_r(),
				.element_stride = addr_map_pcie_c3_ctl_limit_r() -
					addr_map_pcie_c3_ctl_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_BROADCAST
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_BROADCAST_PER_INST,
				.element_static_array = NULL,
				.range_start = 0ULL,
				.range_end = 0ULL,
				.element_stride = 0ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMON
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST,
				.element_static_array =
					t234_pcie_inst3_perfmon_element_static_array,
				.range_start = addr_map_rpg_pm_pcie_c3_base_r(),
				.range_end = addr_map_rpg_pm_pcie_c3_limit_r(),
				.element_stride = addr_map_rpg_pm_pcie_c3_limit_r() -
					addr_map_rpg_pm_pcie_c3_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
		},

		.ip_ops = {
			.ip_dev = NULL,
			.hwpm_ip_pm = NULL,
			.hwpm_ip_reg_op = NULL,
		},

		.element_fs_mask = 0U,
	},
	{
		.hw_inst_mask = BIT(4),
		.num_core_elements_per_inst =
			T234_HWPM_IP_PCIE_NUM_CORE_ELEMENT_PER_INST,
		.element_info = {
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMUX
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST,
				.element_static_array =
					t234_pcie_inst4_perfmux_element_static_array,
				.range_start = addr_map_pcie_c4_ctl_base_r(),
				.range_end = addr_map_pcie_c4_ctl_limit_r(),
				.element_stride = addr_map_pcie_c4_ctl_limit_r() -
					addr_map_pcie_c4_ctl_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_BROADCAST
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_BROADCAST_PER_INST,
				.element_static_array = NULL,
				.range_start = 0ULL,
				.range_end = 0ULL,
				.element_stride = 0ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMON
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST,
				.element_static_array =
					t234_pcie_inst4_perfmon_element_static_array,
				.range_start = addr_map_rpg_pm_pcie_c4_base_r(),
				.range_end = addr_map_rpg_pm_pcie_c4_limit_r(),
				.element_stride = addr_map_rpg_pm_pcie_c4_limit_r() -
					addr_map_rpg_pm_pcie_c4_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
		},

		.ip_ops = {
			.ip_dev = NULL,
			.hwpm_ip_pm = NULL,
			.hwpm_ip_reg_op = NULL,
		},

		.element_fs_mask = 0U,
	},
	{
		.hw_inst_mask = BIT(5),
		.num_core_elements_per_inst =
			T234_HWPM_IP_PCIE_NUM_CORE_ELEMENT_PER_INST,
		.element_info = {
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMUX
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST,
				.element_static_array =
					t234_pcie_inst5_perfmux_element_static_array,
				.range_start = addr_map_pcie_c5_ctl_base_r(),
				.range_end = addr_map_pcie_c5_ctl_limit_r(),
				.element_stride = addr_map_pcie_c5_ctl_limit_r() -
					addr_map_pcie_c5_ctl_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_BROADCAST
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_BROADCAST_PER_INST,
				.element_static_array = NULL,
				.range_start = 0ULL,
				.range_end = 0ULL,
				.element_stride = 0ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMON
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST,
				.element_static_array =
					t234_pcie_inst5_perfmon_element_static_array,
				.range_start = addr_map_rpg_pm_pcie_c5_base_r(),
				.range_end = addr_map_rpg_pm_pcie_c5_limit_r(),
				.element_stride = addr_map_rpg_pm_pcie_c5_limit_r() -
					addr_map_rpg_pm_pcie_c5_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
		},

		.ip_ops = {
			.ip_dev = NULL,
			.hwpm_ip_pm = NULL,
			.hwpm_ip_reg_op = NULL,
		},

		.element_fs_mask = 0U,
	},
	{
		.hw_inst_mask = BIT(6),
		.num_core_elements_per_inst =
			T234_HWPM_IP_PCIE_NUM_CORE_ELEMENT_PER_INST,
		.element_info = {
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMUX
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST,
				.element_static_array =
					t234_pcie_inst6_perfmux_element_static_array,
				.range_start = addr_map_pcie_c6_ctl_base_r(),
				.range_end = addr_map_pcie_c6_ctl_limit_r(),
				.element_stride = addr_map_pcie_c6_ctl_limit_r() -
					addr_map_pcie_c6_ctl_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_BROADCAST
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_BROADCAST_PER_INST,
				.element_static_array = NULL,
				.range_start = 0ULL,
				.range_end = 0ULL,
				.element_stride = 0ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMON
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST,
				.element_static_array =
					t234_pcie_inst6_perfmon_element_static_array,
				.range_start = addr_map_rpg_pm_pcie_c6_base_r(),
				.range_end = addr_map_rpg_pm_pcie_c6_limit_r(),
				.element_stride = addr_map_rpg_pm_pcie_c6_limit_r() -
					addr_map_rpg_pm_pcie_c6_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
		},

		.ip_ops = {
			.ip_dev = NULL,
			.hwpm_ip_pm = NULL,
			.hwpm_ip_reg_op = NULL,
		},

		.element_fs_mask = 0U,
	},
	{
		.hw_inst_mask = BIT(7),
		.num_core_elements_per_inst =
			T234_HWPM_IP_PCIE_NUM_CORE_ELEMENT_PER_INST,
		.element_info = {
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMUX
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST,
				.element_static_array =
					t234_pcie_inst7_perfmux_element_static_array,
				.range_start = addr_map_pcie_c7_ctl_base_r(),
				.range_end = addr_map_pcie_c7_ctl_limit_r(),
				.element_stride = addr_map_pcie_c7_ctl_limit_r() -
					addr_map_pcie_c7_ctl_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_BROADCAST
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_BROADCAST_PER_INST,
				.element_static_array = NULL,
				.range_start = 0ULL,
				.range_end = 0ULL,
				.element_stride = 0ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMON
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST,
				.element_static_array =
					t234_pcie_inst7_perfmon_element_static_array,
				.range_start = addr_map_rpg_pm_pcie_c7_base_r(),
				.range_end = addr_map_rpg_pm_pcie_c7_limit_r(),
				.element_stride = addr_map_rpg_pm_pcie_c7_limit_r() -
					addr_map_rpg_pm_pcie_c7_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
		},

		.ip_ops = {
			.ip_dev = NULL,
			.hwpm_ip_pm = NULL,
			.hwpm_ip_reg_op = NULL,
		},

		.element_fs_mask = 0U,
	},
	{
		.hw_inst_mask = BIT(8),
		.num_core_elements_per_inst =
			T234_HWPM_IP_PCIE_NUM_CORE_ELEMENT_PER_INST,
		.element_info = {
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMUX
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST,
				.element_static_array =
					t234_pcie_inst8_perfmux_element_static_array,
				.range_start = addr_map_pcie_c8_ctl_base_r(),
				.range_end = addr_map_pcie_c8_ctl_limit_r(),
				.element_stride = addr_map_pcie_c8_ctl_limit_r() -
					addr_map_pcie_c8_ctl_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_BROADCAST
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_BROADCAST_PER_INST,
				.element_static_array = NULL,
				.range_start = 0ULL,
				.range_end = 0ULL,
				.element_stride = 0ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMON
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST,
				.element_static_array =
					t234_pcie_inst8_perfmon_element_static_array,
				.range_start = addr_map_rpg_pm_pcie_c8_base_r(),
				.range_end = addr_map_rpg_pm_pcie_c8_limit_r(),
				.element_stride = addr_map_rpg_pm_pcie_c8_limit_r() -
					addr_map_rpg_pm_pcie_c8_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
		},

		.ip_ops = {
			.ip_dev = NULL,
			.hwpm_ip_pm = NULL,
			.hwpm_ip_reg_op = NULL,
		},

		.element_fs_mask = 0U,
	},
	{
		.hw_inst_mask = BIT(9),
		.num_core_elements_per_inst =
			T234_HWPM_IP_PCIE_NUM_CORE_ELEMENT_PER_INST,
		.element_info = {
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMUX
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST,
				.element_static_array =
					t234_pcie_inst9_perfmux_element_static_array,
				.range_start = addr_map_pcie_c9_ctl_base_r(),
				.range_end = addr_map_pcie_c9_ctl_limit_r(),
				.element_stride = addr_map_pcie_c9_ctl_limit_r() -
					addr_map_pcie_c9_ctl_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_BROADCAST
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_BROADCAST_PER_INST,
				.element_static_array = NULL,
				.range_start = 0ULL,
				.range_end = 0ULL,
				.element_stride = 0ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMON
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST,
				.element_static_array =
					t234_pcie_inst9_perfmon_element_static_array,
				.range_start = addr_map_rpg_pm_pcie_c9_base_r(),
				.range_end = addr_map_rpg_pm_pcie_c9_limit_r(),
				.element_stride = addr_map_rpg_pm_pcie_c9_limit_r() -
					addr_map_rpg_pm_pcie_c9_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
		},

		.ip_ops = {
			.ip_dev = NULL,
			.hwpm_ip_pm = NULL,
			.hwpm_ip_reg_op = NULL,
		},

		.element_fs_mask = 0U,
	},
	{
		.hw_inst_mask = BIT(10),
		.num_core_elements_per_inst =
			T234_HWPM_IP_PCIE_NUM_CORE_ELEMENT_PER_INST,
		.element_info = {
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMUX
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMUX_PER_INST,
				.element_static_array =
					t234_pcie_inst10_perfmux_element_static_array,
				.range_start = addr_map_pcie_c10_ctl_base_r(),
				.range_end = addr_map_pcie_c10_ctl_limit_r(),
				.element_stride = addr_map_pcie_c10_ctl_limit_r() -
					addr_map_pcie_c10_ctl_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_BROADCAST
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_BROADCAST_PER_INST,
				.element_static_array = NULL,
				.range_start = 0ULL,
				.range_end = 0ULL,
				.element_stride = 0ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
			/*
			 * Instance info corresponding to
			 * TEGRA_HWPM_APERTURE_TYPE_PERFMON
			 */
			{
				.num_element_per_inst =
					T234_HWPM_IP_PCIE_NUM_PERFMON_PER_INST,
				.element_static_array =
					t234_pcie_inst10_perfmon_element_static_array,
				.range_start = addr_map_rpg_pm_pcie_c10_base_r(),
				.range_end = addr_map_rpg_pm_pcie_c10_limit_r(),
				.element_stride = addr_map_rpg_pm_pcie_c10_limit_r() -
					addr_map_rpg_pm_pcie_c10_base_r() + 1ULL,
				.element_slots = 0U,
				.element_arr = NULL,
			},
		},

		.ip_ops = {
			.ip_dev = NULL,
			.hwpm_ip_pm = NULL,
			.hwpm_ip_reg_op = NULL,
		},

		.element_fs_mask = 0U,
	},
};

/* IP structure */
struct hwpm_ip t234_hwpm_ip_pcie = {
	.num_instances = T234_HWPM_IP_PCIE_NUM_INSTANCES,
	.ip_inst_static_array = t234_pcie_inst_static_array,

	.inst_aperture_info = {
		/*
		 * Instance info corresponding to
		 * TEGRA_HWPM_APERTURE_TYPE_PERFMUX
		 */
		{
			/* NOTE: range should be in ascending order */
			.range_start = addr_map_pcie_c8_ctl_base_r(),
			.range_end = addr_map_pcie_c7_ctl_limit_r(),
			.inst_stride = addr_map_pcie_c8_ctl_limit_r() -
				addr_map_pcie_c8_ctl_base_r() + 1ULL,
			.inst_slots = 0U,
			.inst_arr = NULL,
		},
		/*
		 * Instance info corresponding to
		 * TEGRA_HWPM_APERTURE_TYPE_BROADCAST
		 */
		{
			.range_start = 0ULL,
			.range_end = 0ULL,
			.inst_stride = 0ULL,
			.inst_slots = 0U,
			.inst_arr = NULL,
		},
		/*
		 * Instance info corresponding to
		 * TEGRA_HWPM_APERTURE_TYPE_PERFMON
		 */
		{
			.range_start = addr_map_rpg_pm_pcie_c0_base_r(),
			.range_end = addr_map_rpg_pm_pcie_c10_limit_r(),
			.inst_stride = addr_map_rpg_pm_pcie_c0_limit_r() -
				addr_map_rpg_pm_pcie_c0_base_r() + 1ULL,
			.inst_slots = 0U,
			.inst_arr = NULL,
		},
	},

	.dependent_fuse_mask = TEGRA_HWPM_FUSE_SECURITY_MODE_MASK |
		TEGRA_HWPM_FUSE_HWPM_GLOBAL_DISABLE_MASK,
	.override_enable = false,
	.inst_fs_mask = 0U,
	.resource_status = TEGRA_HWPM_RESOURCE_STATUS_INVALID,
	.reserved = false,
};
