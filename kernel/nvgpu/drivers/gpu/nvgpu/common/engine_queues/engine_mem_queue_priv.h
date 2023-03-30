/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_ENGINE_MEM_QUEUE_PRIV_H
#define NVGPU_ENGINE_MEM_QUEUE_PRIV_H

#include <nvgpu/lock.h>
#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_falcon;

struct nvgpu_engine_mem_queue {
	struct gk20a *g;
	u32 flcn_id;

	/* Queue Type (queue_type) */
	u8 queue_type;

	/* used by nvgpu, for command LPQ/HPQ */
	struct nvgpu_mutex mutex;

	/* current write position */
	u32 position;
	/* physical dmem offset where this queue begins */
	u32 offset;
	/* logical queue identifier */
	u32 id;
	/* physical queue index */
	u32 index;
	/* in bytes */
	u32 size;
	/* open-flag */
	u32 oflag;

	/* queue type(DMEM-Q/EMEM-Q) specific ops */
	int (*push)(struct nvgpu_falcon *flcn,
		    struct nvgpu_engine_mem_queue *queue,
		    u32 dst, void *data, u32 size);
	int (*pop)(struct nvgpu_falcon *flcn,
		   struct nvgpu_engine_mem_queue *queue,
		   u32 src, void *data, u32 size);

	/* engine specific ops */
	int (*head)(struct gk20a *g, u32 queue_id, u32 queue_index,
		u32 *head, bool set);
	int (*tail)(struct gk20a *g, u32 queue_id, u32 queue_index,
		u32 *tail, bool set);
};

#endif /* NVGPU_ENGINE_MEM_QUEUE_PRIV_H */
