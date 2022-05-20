/*                                                      |
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
 *
 */

#ifndef NVGPU_PMU_PERFMON_H
#define NVGPU_PMU_PERFMON_H

//#include <nvgpu/enabled.h>
#include <nvgpu/pmu/pmuif/nvgpu_cmdif.h>
#include <nvgpu/pmu/pmuif/perfmon.h>

struct gk20a;
struct nvgpu_pmu;
struct rpc_handler_payload;
struct nv_pmu_rpc_header;
struct pmu_msg;

/* pmu load const defines */
#define PMU_BUSY_CYCLES_NORM_MAX		(1000U)

struct nvgpu_pmu_perfmon {
	struct pmu_perfmon_counter_v2 perfmon_counter_v2;
	u64 perfmon_events_cnt;
	u32 perfmon_query;
	u8 perfmon_state_id[PMU_DOMAIN_GROUP_NUM];
	u32 sample_buffer;
	u32 load_shadow;
	u32 load_avg;
	u32 load;
	bool perfmon_ready;
	bool perfmon_sampling_enabled;
	int (*init_perfmon)(struct nvgpu_pmu *pmu);
	int (*start_sampling)(struct nvgpu_pmu *pmu);
	int (*stop_sampling)(struct nvgpu_pmu *pmu);
	int (*get_samples_rpc)(struct nvgpu_pmu *pmu);
	int (*perfmon_event_handler)(struct gk20a *g,
		struct nvgpu_pmu *pmu, struct pmu_msg *msg);
};

/* perfmon */
void nvgpu_pmu_perfmon_rpc_handler(struct gk20a *g, struct nvgpu_pmu *pmu,
		struct nv_pmu_rpc_header *rpc,
		struct rpc_handler_payload *rpc_payload);
int nvgpu_pmu_initialize_perfmon(struct gk20a *g, struct nvgpu_pmu *pmu,
		struct nvgpu_pmu_perfmon **perfmon_ptr);
void nvgpu_pmu_deinitialize_perfmon(struct gk20a *g, struct nvgpu_pmu *pmu);
int nvgpu_pmu_init_perfmon(struct nvgpu_pmu *pmu);
int nvgpu_pmu_perfmon_start_sampling(struct nvgpu_pmu *pmu);
int nvgpu_pmu_perfmon_stop_sampling(struct nvgpu_pmu *pmu);
int nvgpu_pmu_perfmon_start_sampling_rpc(struct nvgpu_pmu *pmu);
int nvgpu_pmu_perfmon_stop_sampling_rpc(struct nvgpu_pmu *pmu);
int nvgpu_pmu_perfmon_get_samples_rpc(struct nvgpu_pmu *pmu);
int nvgpu_pmu_handle_perfmon_event(struct gk20a *g,
		struct nvgpu_pmu *pmu, struct pmu_msg *msg);
int nvgpu_pmu_handle_perfmon_event_rpc(struct gk20a *g,
		struct nvgpu_pmu *pmu, struct pmu_msg *msg);
int nvgpu_pmu_init_perfmon_rpc(struct nvgpu_pmu *pmu);
int nvgpu_pmu_load_norm(struct gk20a *g, u32 *load);
int nvgpu_pmu_load_update(struct gk20a *g);
int nvgpu_pmu_busy_cycles_norm(struct gk20a *g, u32 *norm);
void nvgpu_pmu_reset_load_counters(struct gk20a *g);
void nvgpu_pmu_get_load_counters(struct gk20a *g, u32 *busy_cycles,
		u32 *total_cycles);
int nvgpu_pmu_perfmon_get_sampling_enable_status(struct nvgpu_pmu *pmu);
void nvgpu_pmu_perfmon_set_sampling_enable_status(struct nvgpu_pmu *pmu,
		bool status);
u64 nvgpu_pmu_perfmon_get_events_count(struct nvgpu_pmu *pmu);
u32 nvgpu_pmu_perfmon_get_load_avg(struct nvgpu_pmu *pmu);

/* perfmon SW Ops */
int nvgpu_pmu_perfmon_initialization(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_perfmon *perfmon);
int nvgpu_pmu_perfmon_start_sample(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_perfmon *perfmon);
int nvgpu_pmu_perfmon_stop_sample(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_perfmon *perfmon);
int nvgpu_pmu_perfmon_get_sample(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_perfmon *perfmon);
int nvgpu_pmu_perfmon_event_handler(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct pmu_msg *msg);

#endif /* NVGPU_PMU_PERFMON_H */
