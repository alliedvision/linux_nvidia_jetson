/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/bug.h>
#include <nvgpu/channel.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/io.h>
#include <nvgpu/nvgpu_mem.h>

#include "userd_ga10b.h"

#include <nvgpu/hw/ga10b/hw_ram_ga10b.h>

#ifdef CONFIG_NVGPU_USERD
void ga10b_userd_init_mem(struct gk20a *g, struct nvgpu_channel *c)
{
	struct nvgpu_mem *mem = c->userd_mem;
	u32 offset = c->userd_offset / U32(sizeof(u32));

	nvgpu_log_fn(g, " ");

	nvgpu_mem_wr32(g, mem, offset + ram_userd_put_w(), 0);
	nvgpu_mem_wr32(g, mem, offset + ram_userd_get_w(), 0);
	nvgpu_mem_wr32(g, mem, offset + ram_userd_ref_w(), 0);
	nvgpu_mem_wr32(g, mem, offset + ram_userd_put_hi_w(), 0);
	nvgpu_mem_wr32(g, mem, offset + ram_userd_top_level_get_w(), 0);
	nvgpu_mem_wr32(g, mem, offset + ram_userd_top_level_get_hi_w(), 0);
	nvgpu_mem_wr32(g, mem, offset + ram_userd_get_hi_w(), 0);
	nvgpu_mem_wr32(g, mem, offset + ram_userd_gp_get_w(), 0);
	nvgpu_mem_wr32(g, mem, offset + ram_userd_gp_put_w(), 0);
}
#endif /* CONFIG_NVGPU_USERD */
