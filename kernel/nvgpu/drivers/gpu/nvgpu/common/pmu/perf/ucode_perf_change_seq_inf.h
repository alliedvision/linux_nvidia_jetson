/*
 * general p state infrastructure
 *
 * Copyright (c) 2016-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef NVGPU_PMUIF_CTRLPERF_H
#define NVGPU_PMUIF_CTRLPERF_H


#define CTRL_PERF_CHANGE_SEQ_VERSION_35				0x04U

/*!
 * Flags to provide information about the input perf change request.
 * This flags will be used to understand the type of perf change req.
 */
#define CTRL_PERF_CHANGE_SEQ_CHANGE_NONE			0x00U
#define CTRL_PERF_CHANGE_SEQ_CHANGE_FORCE			BIT(0)
#define CTRL_PERF_CHANGE_SEQ_CHANGE_FORCE_CLOCKS		BIT(1)
#define CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC			BIT(2)
#define CTRL_PERF_CHANGE_SEQ_CHANGE_SKIP_VBLANK_WAIT		BIT(3)
#define CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_QUEUE_SIZE		0x04U
#define CTRL_PERF_CHANGE_SEQ_SCRIPT_MAX_PROFILING_THREADS	8
#define CTRL_PERF_CHANGE_SEQ_SCRIPT_VF_SWITCH_MAX_STEPS		13U

struct ctrl_clk_domain_clk_mon_item {
	u32 clk_api_domain;
	u32 clk_freq_Mhz;
	u32 low_threshold_percentage;
	u32 high_threshold_percentage;
};

struct ctrl_clk_domain_clk_mon_list {
	u8 num_domain;
	struct ctrl_clk_domain_clk_mon_item
		clk_domain[CTRL_CLK_CLK_DOMAIN_CLIENT_MAX_DOMAINS];
};

struct ctrl_volt_volt_rail_list_item {
	u8 rail_idx;
	u32 voltage_uv;
	u32 voltage_min_noise_unaware_uv;
	u32 voltage_offset_uV[2];
};

struct ctrl_volt_volt_rail_list {
	u8    num_rails;
	struct ctrl_volt_volt_rail_list_item
		rails[CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS];
};

struct ctrl_perf_chage_seq_change_pmu {
	u32 seq_id;
};

struct ctrl_perf_change_seq_change {
	struct ctrl_clk_clk_domain_list clk_list;
	struct ctrl_volt_volt_rail_list volt_list;
	u32 pstate_index;
	u32 flags;
	u32 vf_points_cache_counter;
	u8 version;
	struct ctrl_perf_chage_seq_change_pmu data;
};

struct ctrl_perf_chage_seq_input_clk {
	u32 clk_freq_khz;
};

struct ctrl_perf_chage_seq_input_volt {
	u32 voltage_uv;
	u32 voltage_min_noise_unaware_uv;
};

struct ctrl_perf_change_seq_change_input {
	u32 pstate_index;
	u32 flags;
	u32 vf_points_cache_counter;
	struct nvgpu_pmu_perf_change_input_clk_info clk;
	struct ctrl_boardobjgrp_mask_e32 volt_rails_mask;
	struct ctrl_perf_chage_seq_input_volt
		volt[CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS];
};

struct u64_align32 {
	u32 lo;
	u32 hi;
};
struct ctrl_perf_change_seq_script_profiling_thread {
	u32 step_mask;
	struct u64_align32 timens;
};

struct ctrl_perf_change_seq_script_profiling {
	struct u64_align32 total_timens; /*align 32 */
	struct u64_align32 total_build_timens;
	struct u64_align32 total_execution_timens;
	u8 num_threads; /*number of threads required to process this script*/
	struct ctrl_perf_change_seq_script_profiling_thread
	    nvgpu_threads[CTRL_PERF_CHANGE_SEQ_SCRIPT_MAX_PROFILING_THREADS];
};

struct ctrl_perf_change_seq_pmu_script_header {
	bool b_increase;
	u8 num_steps;
	u8 cur_step_index;
	struct ctrl_perf_change_seq_script_profiling profiling;
};

enum ctrl_perf_change_seq_pmu_step_id {
	CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_NONE,
	CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_PRE_CHANGE_RM,
	CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_PRE_CHANGE_PMU,
	CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_POST_CHANGE_RM,
	CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_POST_CHANGE_PMU,
	CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_PRE_PSTATE_RM,
	CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_PRE_PSTATE_PMU,
	CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_POST_PSTATE_RM,
	CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_POST_PSTATE_PMU,
	CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_VOLT,
	CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_LPWR,
	CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_BIF,
	CTRL_PERF_CHANGE_SEQ_31_STEP_ID_NOISE_UNAWARE_CLKS,
	CTRL_PERF_CHANGE_SEQ_31_STEP_ID_NOISE_AWARE_CLKS,
	CTRL_PERF_CHANGE_SEQ_35_STEP_ID_PRE_VOLT_CLKS,
	CTRL_PERF_CHANGE_SEQ_35_STEP_ID_POST_VOLT_CLKS,
	CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_MAX_STEPS = 26,
};

struct ctrl_perf_change_seq_step_profiling {
	/*all aligned to 32 */
	u64 total_timens;
	u64 nv_thread_timens;
	u64 pmu_thread_timens;
};

struct ctrl_perf_change_seq_pmu_script_step_super {
	enum ctrl_perf_change_seq_pmu_step_id step_id;
	struct ctrl_perf_change_seq_step_profiling profiling;
};

struct ctrl_perf_change_seq_pmu_script_step_change {
	struct ctrl_perf_change_seq_pmu_script_step_super super;
	u32 pstate_index;
};

struct ctrl_perf_change_seq_pmu_script_step_pstate {
	struct ctrl_perf_change_seq_pmu_script_step_super super;
	u32 pstate_index;
};

struct ctrl_perf_change_seq_pmu_script_step_lpwr {
	struct ctrl_perf_change_seq_pmu_script_step_super super;
	u32 pstate_index;
};

struct ctrl_perf_change_seq_pmu_script_step_bif {
	struct ctrl_perf_change_seq_pmu_script_step_super super;
	u32 pstate_index;
	u8 pcie_idx;
	u8 nvlink_idx;
};

struct ctrl_clk_vin_sw_override_list_item {
	u8 override_mode;
	u32 voltage_uV;
};

struct ctrl_clk_vin_sw_override_list {
	struct ctrl_boardobjgrp_mask_e32 volt_rails_mask;
	struct ctrl_clk_vin_sw_override_list_item
		volt[4];
};

struct ctrl_perf_change_seq_pmu_script_step_clks {
	struct ctrl_perf_change_seq_pmu_script_step_super super;
	struct ctrl_clk_clk_domain_list clk_list;
	struct ctrl_clk_vin_sw_override_list vin_sw_override_list;
};

struct ctrl_perf_change_seq_pmu_script_step_volt {
	struct ctrl_perf_change_seq_pmu_script_step_super super;
	struct ctrl_volt_volt_rail_list volt_list;
	struct ctrl_clk_vin_sw_override_list vin_sw_override_list;
};

struct ctrl_perf_change_seq_pmu_script_step_clk_mon {
	struct ctrl_perf_change_seq_pmu_script_step_super super;
	struct ctrl_clk_domain_clk_mon_list clk_mon_list;
};

union ctrl_perf_change_seq_pmu_script_step_data {
	struct ctrl_perf_change_seq_pmu_script_step_super super;
	struct ctrl_perf_change_seq_pmu_script_step_change change;
	struct ctrl_perf_change_seq_pmu_script_step_pstate ctrlperf_pstate;
	struct ctrl_perf_change_seq_pmu_script_step_lpwr lpwr;
	struct ctrl_perf_change_seq_pmu_script_step_bif bif;
	struct ctrl_perf_change_seq_pmu_script_step_clks clk;
	struct ctrl_perf_change_seq_pmu_script_step_volt volt;
	struct ctrl_perf_change_seq_pmu_script_step_clk_mon clk_mon;
};

struct nv_pmu_rpc_perf_change_seq_queue_change {
	/*[IN/OUT] Must be first field in RPC structure */
	struct nv_pmu_rpc_header hdr;
	struct ctrl_perf_change_seq_change_input change;
	u32 seq_id;
	u32 scratch[1];
};

struct nv_pmu_perf_change_seq_super_info_get {
	u8 version;
};

struct nv_pmu_perf_change_seq_pmu_info_get {
	struct nv_pmu_perf_change_seq_super_info_get  super;
	u32 cpu_advertised_step_id_mask;
};

struct nv_pmu_perf_change_seq_super_info_set {
	u8 version;
	struct ctrl_boardobjgrp_mask_e32 clk_domains_exclusion_mask;
	struct ctrl_boardobjgrp_mask_e32 clk_domains_inclusion_mask;
	u32 strp_id_exclusive_mask;
};

struct nv_pmu_perf_change_seq_pmu_info_set {
	struct nv_pmu_perf_change_seq_super_info_set super;
	bool b_lock;
	bool b_vf_point_check_ignore;
	u32 cpu_step_id_mask;
};

struct nv_pmu_rpc_perf_change_seq_info_get {
	/*[IN/OUT] Must be first field in RPC structure */
	struct nv_pmu_rpc_header hdr;
	struct nv_pmu_perf_change_seq_pmu_info_get info_get;
	u32 scratch[1];
};

struct nv_pmu_rpc_perf_change_seq_info_set {
	/*[IN/OUT] Must be first field in RPC structure */
	struct nv_pmu_rpc_header hdr;
	struct nv_pmu_perf_change_seq_pmu_info_set info_set;
	u32 scratch[1];
};

NV_PMU_MAKE_ALIGNED_STRUCT(ctrl_perf_change_seq_change,
		sizeof(struct ctrl_perf_change_seq_change));

NV_PMU_MAKE_ALIGNED_STRUCT(ctrl_perf_change_seq_pmu_script_header,
		sizeof(struct ctrl_perf_change_seq_pmu_script_header));

NV_PMU_MAKE_ALIGNED_UNION(ctrl_perf_change_seq_pmu_script_step_data,
		sizeof(union ctrl_perf_change_seq_pmu_script_step_data));

struct perf_change_seq_pmu_script {
	union ctrl_perf_change_seq_pmu_script_header_aligned hdr;
	union ctrl_perf_change_seq_change_aligned change;
	/* below should be an aligned structure */
	union ctrl_perf_change_seq_pmu_script_step_data_aligned
		steps[CTRL_PERF_CHANGE_SEQ_SCRIPT_VF_SWITCH_MAX_STEPS];
};

#endif /* NVGPU_PMUIF_CTRLPERF_H */
