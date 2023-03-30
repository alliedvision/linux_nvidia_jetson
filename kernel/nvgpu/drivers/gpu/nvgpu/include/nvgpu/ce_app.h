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
#ifndef NVGPU_CE_APP_H
#define NVGPU_CE_APP_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_fence_type;

#define NVGPU_CE_INVAL_CTX_ID	~U32(0U)

/* CE command utility macros */
#define NVGPU_CE_LOWER_ADDRESS_OFFSET_MASK U32_MAX
#define NVGPU_CE_UPPER_ADDRESS_OFFSET_MASK 0xffU
#define NVGPU_CE_MAX_ADDRESS \
	((U64(NVGPU_CE_UPPER_ADDRESS_OFFSET_MASK) << 32U) | \
	 U64(NVGPU_CE_LOWER_ADDRESS_OFFSET_MASK))

#define NVGPU_CE_MAX_INFLIGHT_JOBS 32U

/*
 * A copyengine job for any buffer size needs at most:
 *
 * - two u32 words for class header
 * - two operations, both either 16 words (transfer) or 15 words (memset)
 *
 * The size does not need to be exact, so this uses the upper bound:
 * 2 + 2 * 16 = 34 words, or 136 bytes.
 */
#define NVGPU_CE_MAX_COMMAND_BUFF_BYTES_PER_SUBMIT \
	((2U + 2U * 16U) * sizeof(u32))

/* dma launch_flags */
	/* location */
#define NVGPU_CE_SRC_LOCATION_COHERENT_SYSMEM			BIT32(0)
#define NVGPU_CE_SRC_LOCATION_NONCOHERENT_SYSMEM		BIT32(1)
#define NVGPU_CE_SRC_LOCATION_LOCAL_FB				BIT32(2)
#define NVGPU_CE_DST_LOCATION_COHERENT_SYSMEM			BIT32(3)
#define NVGPU_CE_DST_LOCATION_NONCOHERENT_SYSMEM		BIT32(4)
#define NVGPU_CE_DST_LOCATION_LOCAL_FB				BIT32(5)
	/* memory layout */
#define NVGPU_CE_SRC_MEMORY_LAYOUT_PITCH			BIT32(6)
#define NVGPU_CE_SRC_MEMORY_LAYOUT_BLOCKLINEAR			BIT32(7)
#define NVGPU_CE_DST_MEMORY_LAYOUT_PITCH			BIT32(8)
#define NVGPU_CE_DST_MEMORY_LAYOUT_BLOCKLINEAR			BIT32(9)
	/* transfer type */
#define NVGPU_CE_DATA_TRANSFER_TYPE_PIPELINED			BIT32(10)
#define NVGPU_CE_DATA_TRANSFER_TYPE_NON_PIPELINED		BIT32(11)

/* CE operation mode */
#define NVGPU_CE_PHYS_MODE_TRANSFER	BIT32(0)
#define NVGPU_CE_MEMSET			BIT32(1)

/* CE app state machine flags */
enum {
	NVGPU_CE_ACTIVE                    = (1 << 0),
	NVGPU_CE_SUSPEND                   = (1 << 1),
};

/* gpu context state machine flags */
enum {
	NVGPU_CE_GPU_CTX_ALLOCATED         = (1 << 0),
	NVGPU_CE_GPU_CTX_DELETED           = (1 << 1),
};

/* global CE app related apis */
int nvgpu_ce_app_init_support(struct gk20a *g);
void nvgpu_ce_app_suspend(struct gk20a *g);
void nvgpu_ce_app_destroy(struct gk20a *g);

/* CE app utility functions */
u32 nvgpu_ce_app_create_context(struct gk20a *g,
		u32 runlist_id,
		int timeslice,
		int runlist_level);
void nvgpu_ce_app_delete_context(struct gk20a *g,
		u32 ce_ctx_id);
int nvgpu_ce_execute_ops(struct gk20a *g,
		u32 ce_ctx_id,
		u64 src_paddr,
		u64 dst_paddr,
		u64 size,
		u32 payload,
		u32 launch_flags,
		u32 request_operation,
		u32 submit_flags,
		struct nvgpu_fence_type **fence_out);
#endif /*NVGPU_CE_APP_H*/
