/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_PMU_QUEUE_H
#define NVGPU_PMU_QUEUE_H

#include <nvgpu/pmu/pmuif/cmn.h>
#include <nvgpu/types.h>

union pmu_init_msg_pmu;
struct nvgpu_falcon;
struct nvgpu_mem;
struct pmu_cmd;
struct gk20a;

struct pmu_queues {
	struct nvgpu_engine_fb_queue *fb_queue[PMU_QUEUE_COUNT];
	struct nvgpu_engine_mem_queue *queue[PMU_QUEUE_COUNT];
	u32 queue_type;
};

int nvgpu_pmu_queues_init(struct gk20a *g,
			  union pmu_init_msg_pmu *init,
			  struct pmu_queues *queues,
			  struct nvgpu_mem *super_surface_buf);

void nvgpu_pmu_queues_free(struct gk20a *g, struct pmu_queues *queues);

bool nvgpu_pmu_queue_is_empty(struct pmu_queues *queues, u32 queue_id);
u32 nvgpu_pmu_queue_get_size(struct pmu_queues *queues, u32 queue_id);
int nvgpu_pmu_queue_push(struct pmu_queues *queues, struct nvgpu_falcon *flcn,
			 u32 queue_id, struct pmu_cmd *cmd);
int nvgpu_pmu_queue_pop(struct pmu_queues *queues, struct nvgpu_falcon *flcn,
			u32 queue_id, void *data, u32 bytes_to_read,
			u32 *bytes_read);

bool nvgpu_pmu_fb_queue_enabled(struct pmu_queues *queues);
struct nvgpu_engine_fb_queue *nvgpu_pmu_fb_queue(struct pmu_queues *queues,
						 u32 queue_id);
int nvgpu_pmu_queue_rewind(struct pmu_queues *queues, u32 queue_id,
			   struct nvgpu_falcon *flcn);

#endif /* NVGPU_PMU_QUEUE_H */
