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
#ifndef NVGPU_GOPS_USERD_H
#define NVGPU_GOPS_USERD_H

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_channel;

struct gops_userd {
	int (*setup_sw)(struct gk20a *g);
	void (*cleanup_sw)(struct gk20a *g);
	void (*init_mem)(struct gk20a *g, struct nvgpu_channel *c);
	u32 (*entry_size)(struct gk20a *g);

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	u32 (*gp_get)(struct gk20a *g, struct nvgpu_channel *c);
	void (*gp_put)(struct gk20a *g, struct nvgpu_channel *c);
	u64 (*pb_get)(struct gk20a *g, struct nvgpu_channel *c);
#endif

};
/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

#endif
