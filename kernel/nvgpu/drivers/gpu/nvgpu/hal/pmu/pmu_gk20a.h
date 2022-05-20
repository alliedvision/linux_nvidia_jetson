/*
 * Copyright (c) 2011-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef PMU_GK20A_H
#define PMU_GK20A_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_pmu;
struct pmu_mutexes;

#define PMU_MODE_MISMATCH_STATUS_MAILBOX_R  6U
#define PMU_MODE_MISMATCH_STATUS_VAL        0xDEADDEADU

void gk20a_pmu_isr(struct gk20a *g);
u32 gk20a_pmu_get_irqmask(struct gk20a *g);

#ifdef CONFIG_NVGPU_LS_PMU
void gk20a_pmu_dump_falcon_stats(struct nvgpu_pmu *pmu);
void gk20a_pmu_init_perfmon_counter(struct gk20a *g);
void gk20a_pmu_pg_idle_counter_config(struct gk20a *g, u32 pg_engine_id);
u32 gk20a_pmu_read_idle_counter(struct gk20a *g, u32 counter_id);
void gk20a_pmu_reset_idle_counter(struct gk20a *g, u32 counter_id);
u32 gk20a_pmu_read_idle_intr_status(struct gk20a *g);
void gk20a_pmu_clear_idle_intr_status(struct gk20a *g);
void gk20a_pmu_dump_elpg_stats(struct nvgpu_pmu *pmu);
u32 gk20a_pmu_mutex_owner(struct gk20a *g, struct pmu_mutexes *mutexes,
	u32 id);
int gk20a_pmu_mutex_acquire(struct gk20a *g, struct pmu_mutexes *mutexes,
	u32 id, u32 *token);
void gk20a_pmu_mutex_release(struct gk20a *g, struct pmu_mutexes *mutexes,
	u32 id, u32 *token);
int gk20a_pmu_queue_head(struct gk20a *g, u32 queue_id, u32 queue_index,
	u32 *head, bool set);
int gk20a_pmu_queue_tail(struct gk20a *g, u32 queue_id, u32 queue_index,
	u32 *tail, bool set);
void gk20a_pmu_msgq_tail(struct nvgpu_pmu *pmu, u32 *tail, bool set);
u32 gk20a_pmu_get_irqdest(struct gk20a *g);
void gk20a_pmu_enable_irq(struct nvgpu_pmu *pmu, bool enable);
void gk20a_pmu_handle_interrupts(struct gk20a *g, u32 intr);
bool gk20a_pmu_is_interrupted(struct nvgpu_pmu *pmu);
int gk20a_pmu_bar0_error_status(struct gk20a *g, u32 *bar0_status,
	u32 *etype);
int gk20a_pmu_ns_bootstrap(struct gk20a *g, struct nvgpu_pmu *pmu,
	u32 args_offset);
bool gk20a_pmu_is_engine_in_reset(struct gk20a *g);
void gk20a_pmu_engine_reset(struct gk20a *g, bool do_reset);
void gk20a_write_dmatrfbase(struct gk20a *g, u32 addr);
u32 gk20a_pmu_falcon_base_addr(void);
bool gk20a_is_pmu_supported(struct gk20a *g);
#endif

#endif /* PMU_GK20A_H */
