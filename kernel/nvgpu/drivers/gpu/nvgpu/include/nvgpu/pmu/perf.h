/*
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
#ifndef NVGPU_PMU_PERF_H
#define NVGPU_PMU_PERF_H

#include <nvgpu/boardobjgrp_e32.h>
#include <nvgpu/boardobjgrp_e255.h>
#include <nvgpu/boardobjgrpmask.h>
#include <nvgpu/pmu/clk/clk.h>

struct nvgpu_clk_slave_freq;

#define CTRL_PERF_PSTATE_P0		0U
#define CTRL_PERF_PSTATE_P5		5U
#define CTRL_PERF_PSTATE_P8		8U
#define CLK_SET_INFO_MAX_SIZE		(32U)

#define NV_PMU_PERF_CMD_ID_RPC                                   (0x00000002U)
#define NV_PMU_PERF_CMD_ID_BOARDOBJ_GRP_SET                      (0x00000003U)
#define NV_PMU_PERF_CMD_ID_BOARDOBJ_GRP_GET_STATUS               (0x00000004U)

/*!
 * RPC calls serviced by PERF unit.
 */
#define NV_PMU_RPC_ID_PERF_BOARD_OBJ_GRP_CMD                     0x00U
#define NV_PMU_RPC_ID_PERF_LOAD                                  0x01U
#define NV_PMU_RPC_ID_PERF_CHANGE_SEQ_INFO_GET                   0x02U
#define NV_PMU_RPC_ID_PERF_CHANGE_SEQ_INFO_SET                   0x03U
#define NV_PMU_RPC_ID_PERF_CHANGE_SEQ_SET_CONTROL                0x04U
#define NV_PMU_RPC_ID_PERF_CHANGE_SEQ_QUEUE_CHANGE               0x05U
#define NV_PMU_RPC_ID_PERF_CHANGE_SEQ_LOCK                       0x06U
#define NV_PMU_RPC_ID_PERF_CHANGE_SEQ_QUERY                      0x07U
#define NV_PMU_RPC_ID_PERF_PERF_LIMITS_INVALIDATE                0x08U
#define NV_PMU_RPC_ID_PERF_PERF_PSTATE_STATUS_UPDATE             0x09U
#define NV_PMU_RPC_ID_PERF_VFE_EQU_EVAL                          0x0AU
#define NV_PMU_RPC_ID_PERF_VFE_INVALIDATE                        0x0BU
#define NV_PMU_RPC_ID_PERF_VFE_EQU_MONITOR_SET                   0x0CU
#define NV_PMU_RPC_ID_PERF_VFE_EQU_MONITOR_GET                   0x0DU
#define NV_PMU_RPC_ID_PERF__COUNT                                0x0EU

/* PERF Message-type Definitions */
#define NV_PMU_PERF_MSG_ID_RPC                                   (0x00000003U)
#define NV_PMU_PERF_MSG_ID_BOARDOBJ_GRP_SET                      (0x00000004U)
#define NV_PMU_PERF_MSG_ID_BOARDOBJ_GRP_GET_STATUS               (0x00000006U)

struct nvgpu_pmu_perf_pstate_clk_info {
	u32 clkwhich;
	u32 nominal_mhz;
	u16 min_mhz;
	u16 max_mhz;
};

struct perf_chage_seq_input_clk {
	u32 clk_freq_khz;
};

struct nvgpu_pmu_perf_change_input_clk_info {
	struct ctrl_boardobjgrp_mask_e32 clk_domains_mask;
	struct perf_chage_seq_input_clk
		clk[CTRL_CLK_CLK_DOMAIN_CLIENT_MAX_DOMAINS];
};

int nvgpu_pmu_perf_init(struct gk20a *g);
void nvgpu_pmu_perf_deinit(struct gk20a *g);
int nvgpu_pmu_perf_sw_setup(struct gk20a *g);
int nvgpu_pmu_perf_pmu_setup(struct gk20a *g);

int nvgpu_pmu_perf_load(struct gk20a *g);

int nvgpu_pmu_perf_vfe_get_s_param(struct gk20a *g, u64 *s_param);

int nvgpu_pmu_perf_vfe_get_volt_margin(struct gk20a *g, u32 *vmargin_uv);
int nvgpu_pmu_perf_vfe_get_freq_margin(struct gk20a *g, u32 *fmargin_mhz);

int nvgpu_pmu_perf_changeseq_set_clks(struct gk20a *g,
	struct nvgpu_clk_slave_freq *vf_point);

struct nvgpu_pmu_perf_pstate_clk_info *nvgpu_pmu_perf_pstate_get_clk_set_info(
		struct gk20a *g, u32 pstate_num, u32 clkwhich);

void nvgpu_perf_change_seq_execute_time(struct gk20a *g, s64 *change_time);

#endif /* NVGPU_PMU_PERF_H */
