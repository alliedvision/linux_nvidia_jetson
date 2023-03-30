/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_PMU_GA10B_H
#define NVGPU_PMU_GA10B_H

#include <nvgpu/types.h>

#define DMA_OFFSET_START	0U
#define DMEM_DATA_0             0x0U
#define DMEM_DATA_1             0x1U
#define PMU_IDLE_THRESHOLD_V    0x7FFFFFFF
#define IDLE_COUNTER_0          0
#define IDLE_COUNTER_1          1
#define IDLE_COUNTER_2          2
#define IDLE_COUNTER_3          3
#define IDLE_COUNTER_4          4
#define IDLE_COUNTER_6          6
#define right_shift_8bits(v)    (v >> 8U)
#define left_shift_8bits(v)     (v << 8U)

struct gk20a;
struct nvgpu_pmu;

bool ga10b_is_pmu_supported(struct gk20a *g);
u32 ga10b_pmu_falcon2_base_addr(void);
int ga10b_pmu_ns_bootstrap(struct gk20a *g, struct nvgpu_pmu *pmu,
		u32 args_offset);
u32 ga10b_pmu_get_inst_block_config(struct gk20a *g);
void ga10b_pmu_dump_elpg_stats(struct nvgpu_pmu *pmu);
void ga10b_pmu_init_perfmon_counter(struct gk20a *g);
u32 ga10b_pmu_read_idle_counter(struct gk20a *g, u32 counter_id);
void ga10b_pmu_reset_idle_counter(struct gk20a *g, u32 counter_id);
u32 ga10b_pmu_get_irqmask(struct gk20a *g);
bool ga10b_pmu_is_debug_mode_en(struct gk20a *g);
void ga10b_pmu_handle_swgen1_irq(struct gk20a *g, u32 intr);
#ifdef CONFIG_NVGPU_LS_PMU
bool ga10b_pmu_is_interrupted(struct nvgpu_pmu *pmu);
#endif
void ga10b_pmu_enable_irq(struct nvgpu_pmu *pmu, bool enable);
void ga10b_pmu_handle_ext_irq(struct gk20a *g, u32 intr0);
#endif /* NVGPU_PMU_GA10B_H */
