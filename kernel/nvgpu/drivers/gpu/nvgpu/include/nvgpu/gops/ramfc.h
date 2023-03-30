/*
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
 */
#ifndef NVGPU_GOPS_RAMFC_H
#define NVGPU_GOPS_RAMFC_H

#include <nvgpu/types.h>

/** @cond DOXYGEN_SHOULD_SKIP_THIS */

struct gk20a;
struct nvgpu_channel;
struct nvgpu_channel_dump_info;

struct gops_ramfc {
	int (*setup)(struct nvgpu_channel *ch, u64 gpfifo_base,
			u32 gpfifo_entries, u64 pbdma_acquire_timeout,
			u32 flags);
	void (*capture_ram_dump)(struct gk20a *g,
			struct nvgpu_channel *ch,
			struct nvgpu_channel_dump_info *info);
	int (*commit_userd)(struct nvgpu_channel *ch);
	u32 (*get_syncpt)(struct nvgpu_channel *ch);
	void (*set_syncpt)(struct nvgpu_channel *ch, u32 syncpt);
};

/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

#endif
