/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GA100_PERF
#define NVGPU_GA100_PERF

#ifdef CONFIG_NVGPU_DEBUGGER

#include <nvgpu/types.h>

struct gk20a;

u32 ga100_perf_get_pmmsys_per_chiplet_offset(void);
u32 ga100_perf_get_pmmgpc_per_chiplet_offset(void);
u32 ga100_perf_get_pmmfbp_per_chiplet_offset(void);

const u32 *ga100_perf_get_hwpm_sys_perfmon_regs(u32 *count);
const u32 *ga100_perf_get_hwpm_gpc_perfmon_regs(u32 *count);
const u32 *ga100_perf_get_hwpm_fbp_perfmon_regs(u32 *count);

void ga100_perf_get_num_hwpm_perfmon(struct gk20a *g, u32 *num_sys_perfmon,
		u32 *num_fbp_perfmon, u32 *num_gpc_perfmon);
#endif /* CONFIG_NVGPU_DEBUGGER */
#endif
