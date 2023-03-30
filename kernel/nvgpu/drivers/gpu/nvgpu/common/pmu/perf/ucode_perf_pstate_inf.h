/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_PMUIF_PERFPSTATE_H_
#define NVGPU_PMUIF_PERFPSTATE_H_

#define PMU_PERF_CLK_DOMAINS_IDX_MAX	(16U)

struct nv_pmu_perf_pstate_boardobjgrp_set_header {
	struct nv_pmu_boardobjgrp_e32 super;
	u8 numClkDomains;
	u8 boot_pstate_idx;
};

struct nv_pmu_perf_pstate {
	struct nv_pmu_boardobj super;
	u8 lpwrEntryIdx;
	u32 flags;
};

struct nv_pmu_perf_pstate_3x {
	struct nv_pmu_perf_pstate super;
};

struct nv_ctrl_perf_pstate_clk_freq_35 {
	u32 freqKz;
	u32 freqVfMaxKhz;
	u32 baseFreqKhz;
	u32 origFreqKhz;
	u32 porFreqKhz;
};

struct ctrl_perf_pstate_clk_entry_35 {
	struct nv_ctrl_perf_pstate_clk_freq_35 min;
	struct nv_ctrl_perf_pstate_clk_freq_35 max;
	struct nv_ctrl_perf_pstate_clk_freq_35 nom;
};

struct ctrl_perf_pstate_clk_entry_30 {
	u32 targetFreqKhz;
	u32 freqRangeMinKhz;
	u32 freqRangeMaxKhz;
};

struct nv_pmu_perf_pstate_30 {
	struct nv_pmu_perf_pstate_3x super;
	struct ctrl_perf_pstate_clk_entry_30
		clkEntries[PMU_PERF_CLK_DOMAINS_IDX_MAX];
};

struct nv_pmu_perf_pstate_35 {
	struct nv_pmu_perf_pstate_3x super;
	u8 pcieIdx;
	u8 nvlinkIdx;
	struct ctrl_perf_pstate_clk_entry_35
	clkEntries[PMU_PERF_CLK_DOMAINS_IDX_MAX];
};

union nv_pmu_perf_pstate_boardobj_set_union {
	struct nv_pmu_boardobj obj;
	struct nv_pmu_perf_pstate super;
	struct nv_pmu_perf_pstate_3x v3x;
	struct nv_pmu_perf_pstate_30 v30;
	struct nv_pmu_perf_pstate_35 v35;
};

NV_PMU_BOARDOBJ_GRP_SET_MAKE_E32(perf, pstate);

#endif /* NVGPU_PMUIF_PERFPSTATE_H_ */
