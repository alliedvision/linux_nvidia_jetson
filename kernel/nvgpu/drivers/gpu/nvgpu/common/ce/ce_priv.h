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
#ifndef NVGPU_CE_PRIV_H
#define NVGPU_CE_PRIV_H

#include <nvgpu/types.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/list.h>
#include <nvgpu/lock.h>

struct gk20a;

/* ce context db */
struct nvgpu_ce_gpu_ctx {
	struct gk20a *g;
	u32 ctx_id;
	struct nvgpu_mutex gpu_ctx_mutex;
	int gpu_ctx_state;

	/* tsg related data */
	struct nvgpu_tsg *tsg;

	/* channel related data */
	struct nvgpu_channel *ch;
	struct vm_gk20a *vm;

	/* cmd buf mem_desc */
	struct nvgpu_mem cmd_buf_mem;
	struct nvgpu_fence_type *postfences[NVGPU_CE_MAX_INFLIGHT_JOBS];

	struct nvgpu_list_node list;

	u32 cmd_buf_read_queue_offset;
};

/* global ce app db */
struct nvgpu_ce_app {
	bool initialised;
	struct nvgpu_mutex app_mutex;
	int app_state;

	struct nvgpu_list_node allocated_contexts;
	u32 ctx_count;
	u32 next_ctx_id;
};

static inline struct nvgpu_ce_gpu_ctx *
nvgpu_ce_gpu_ctx_from_list(struct nvgpu_list_node *node)
{
	return (struct nvgpu_ce_gpu_ctx *)
		((uintptr_t)node - offsetof(struct nvgpu_ce_gpu_ctx, list));
};

u32 nvgpu_ce_prepare_submit(u64 src_paddr,
		u64 dst_paddr,
		u64 size,
		u32 *cmd_buf_cpu_va,
		u32 payload,
		u32 launch_flags,
		u32 request_operation,
		u32 dma_copy_class);

#endif /*NVGPU_CE_PRIV_H*/
