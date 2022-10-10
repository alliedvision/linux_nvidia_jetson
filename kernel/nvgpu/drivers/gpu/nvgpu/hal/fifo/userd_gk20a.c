/*
 * GK20A USERD
 *
 * Copyright (c) 2011-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include "userd_gk20a.h"

#include <nvgpu/hw/gk20a/hw_ram_gk20a.h>

#ifdef CONFIG_NVGPU_USERD
void gk20a_userd_init_mem(struct gk20a *g, struct nvgpu_channel *c)
{
	struct nvgpu_mem *mem = c->userd_mem;
	u32 offset = c->userd_offset / U32(sizeof(u32));

	nvgpu_log_fn(g, " ");

	nvgpu_mem_wr32(g, mem, offset + ram_userd_put_w(), 0);
	nvgpu_mem_wr32(g, mem, offset + ram_userd_get_w(), 0);
	nvgpu_mem_wr32(g, mem, offset + ram_userd_ref_w(), 0);
	nvgpu_mem_wr32(g, mem, offset + ram_userd_put_hi_w(), 0);
	nvgpu_mem_wr32(g, mem, offset + ram_userd_gp_top_level_get_w(), 0);
	nvgpu_mem_wr32(g, mem, offset + ram_userd_gp_top_level_get_hi_w(), 0);
	nvgpu_mem_wr32(g, mem, offset + ram_userd_get_hi_w(), 0);
	nvgpu_mem_wr32(g, mem, offset + ram_userd_gp_get_w(), 0);
	nvgpu_mem_wr32(g, mem, offset + ram_userd_gp_put_w(), 0);
}

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
u32 gk20a_userd_gp_get(struct gk20a *g, struct nvgpu_channel *c)
{
	u64 userd_gpu_va = nvgpu_channel_userd_gpu_va(c);
	u64 addr = userd_gpu_va + sizeof(u32) * ram_userd_gp_get_w();

	BUG_ON(u64_hi32(addr) != 0U);

	return nvgpu_bar1_readl(g, (u32)addr);
}

u64 gk20a_userd_pb_get(struct gk20a *g, struct nvgpu_channel *c)
{
	u64 userd_gpu_va = nvgpu_channel_userd_gpu_va(c);
	u64 lo_addr = userd_gpu_va + sizeof(u32) * ram_userd_get_w();
	u64 hi_addr = userd_gpu_va + sizeof(u32) * ram_userd_get_hi_w();
	u32 lo, hi;

	BUG_ON((u64_hi32(lo_addr) != 0U) || (u64_hi32(hi_addr) != 0U));
	lo = nvgpu_bar1_readl(g, (u32)lo_addr);
	hi = nvgpu_bar1_readl(g, (u32)hi_addr);

	return ((u64)hi << 32) | lo;
}

void gk20a_userd_gp_put(struct gk20a *g, struct nvgpu_channel *c)
{
	u64 userd_gpu_va = nvgpu_channel_userd_gpu_va(c);
	u64 addr = userd_gpu_va + sizeof(u32) * ram_userd_gp_put_w();

	BUG_ON(u64_hi32(addr) != 0U);
	nvgpu_bar1_writel(g, (u32)addr, c->gpfifo.put);
}
#endif
#endif /* CONFIG_NVGPU_USERD */

u32 gk20a_userd_entry_size(struct gk20a *g)
{
	(void)g;
	return BIT32(ram_userd_base_shift_v());
}
