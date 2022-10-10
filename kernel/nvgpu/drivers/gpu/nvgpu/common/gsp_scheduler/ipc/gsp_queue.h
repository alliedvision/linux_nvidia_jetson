/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GSP_QUEUE_H
#define NVGPU_GSP_QUEUE_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_falcon;
struct nv_flcn_cmd_gsp;
struct nvgpu_engine_mem_queue;
struct gsp_init_msg_gsp_init;

int nvgpu_gsp_queues_init(struct gk20a *g,
			   struct nvgpu_engine_mem_queue **queues,
			   struct gsp_init_msg_gsp_init *init);
void nvgpu_gsp_queues_free(struct gk20a *g,
			    struct nvgpu_engine_mem_queue **queues);
u32 nvgpu_gsp_queue_get_size(struct nvgpu_engine_mem_queue **queues,
			      u32 queue_id);
int nvgpu_gsp_queue_push(struct nvgpu_engine_mem_queue **queues,
			  u32 queue_id, struct nvgpu_falcon *flcn,
			  struct nv_flcn_cmd_gsp *cmd, u32 size);
bool nvgpu_gsp_queue_is_empty(struct nvgpu_engine_mem_queue **queues,
			       u32 queue_id);
bool nvgpu_gsp_queue_read(struct gk20a *g,
			   struct nvgpu_engine_mem_queue **queues,
			   u32 queue_id, struct nvgpu_falcon *flcn, void *data,
			   u32 bytes_to_read, int *status);
int nvgpu_gsp_queue_rewind(struct nvgpu_falcon *flcn,
			    struct nvgpu_engine_mem_queue **queues,
			    u32 queue_id);

#endif /* NVGPU_GSP_QUEUE_H */
