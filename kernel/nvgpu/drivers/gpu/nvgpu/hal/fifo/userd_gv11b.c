/*
 * GV11B USERD
 *
 * Copyright (c) 2015-2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/io.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/channel.h>

#include <nvgpu/hw/gv11b/hw_ram_gv11b.h>

#include "userd_gv11b.h"

u32 gv11b_userd_gp_get(struct gk20a *g, struct nvgpu_channel *ch)
{
	struct nvgpu_mem *mem = ch->userd_mem;
	u32 offset = ch->userd_offset / U32(sizeof(u32));

	return nvgpu_mem_rd32(g, mem, offset + ram_userd_gp_get_w());
}

u64 gv11b_userd_pb_get(struct gk20a *g, struct nvgpu_channel *ch)
{
	struct nvgpu_mem *mem = ch->userd_mem;
	u32 offset = ch->userd_offset / U32(sizeof(u32));
	u32 lo, hi;

	lo = nvgpu_mem_rd32(g, mem, offset + ram_userd_get_w());
	hi = nvgpu_mem_rd32(g, mem, offset + ram_userd_get_hi_w());

	return ((u64)hi << 32) | lo;
}

void gv11b_userd_gp_put(struct gk20a *g, struct nvgpu_channel *ch)
{
	struct nvgpu_mem *mem = ch->userd_mem;
	u32 offset = ch->userd_offset / U32(sizeof(u32));

	nvgpu_mem_wr32(g, mem, offset + ram_userd_gp_put_w(), ch->gpfifo.put);
	/* Commit everything to GPU. */
	nvgpu_mb();

	g->ops.usermode.ring_doorbell(ch);
}
