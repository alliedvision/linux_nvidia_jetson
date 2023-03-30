/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_SEC2_QUEUE_H
#define NVGPU_SEC2_QUEUE_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_falcon;
struct nv_flcn_cmd_sec2;
struct nvgpu_engine_mem_queue;
struct sec2_init_msg_sec2_init;

int nvgpu_sec2_queues_init(struct gk20a *g,
			   struct nvgpu_engine_mem_queue **queues,
			   struct sec2_init_msg_sec2_init *init);
void nvgpu_sec2_queues_free(struct gk20a *g,
			    struct nvgpu_engine_mem_queue **queues);
u32 nvgpu_sec2_queue_get_size(struct nvgpu_engine_mem_queue **queues,
			      u32 queue_id);
int nvgpu_sec2_queue_push(struct nvgpu_engine_mem_queue **queues,
			  u32 queue_id, struct nvgpu_falcon *flcn,
			  struct nv_flcn_cmd_sec2 *cmd, u32 size);
bool nvgpu_sec2_queue_is_empty(struct nvgpu_engine_mem_queue **queues,
			       u32 queue_id);
bool nvgpu_sec2_queue_read(struct gk20a *g,
			   struct nvgpu_engine_mem_queue **queues,
			   u32 queue_id, struct nvgpu_falcon *flcn, void *data,
			   u32 bytes_to_read, int *status);
int nvgpu_sec2_queue_rewind(struct nvgpu_falcon *flcn,
			    struct nvgpu_engine_mem_queue **queues,
			    u32 queue_id);

#endif /* NVGPU_SEC2_QUEUE_H */
