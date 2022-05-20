/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/io.h>
#include <nvgpu/mm.h>
#include <nvgpu/bug.h>
#include <nvgpu/gk20a.h>

#include "perf_gm20b.h"

#include <nvgpu/hw/gm20b/hw_perf_gm20b.h>

bool gm20b_perf_get_membuf_overflow_status(struct gk20a *g)
{
	const u32 st = perf_pmasys_control_membuf_status_overflowed_f();
	return st == (nvgpu_readl(g, perf_pmasys_control_r()) & st);
}

u32 gm20b_perf_get_membuf_pending_bytes(struct gk20a *g)
{
	return nvgpu_readl(g, perf_pmasys_mem_bytes_r());
}

void gm20b_perf_set_membuf_handled_bytes(struct gk20a *g,
	u32 entries, u32 entry_size)
{
	if (entries > 0U) {
		nvgpu_writel(g, perf_pmasys_mem_bump_r(), entries * entry_size);
	}
}

void gm20b_perf_membuf_reset_streaming(struct gk20a *g)
{
	u32 engine_status;
	u32 num_unread_bytes;

	engine_status = nvgpu_readl(g, perf_pmasys_enginestatus_r());
	WARN_ON(0U ==
		(engine_status & perf_pmasys_enginestatus_rbufempty_empty_f()));

	nvgpu_writel(g, perf_pmasys_control_r(),
		perf_pmasys_control_membuf_clear_status_doit_f());

	num_unread_bytes = nvgpu_readl(g, perf_pmasys_mem_bytes_r());
	if (num_unread_bytes != 0U) {
		nvgpu_writel(g, perf_pmasys_mem_bump_r(), num_unread_bytes);
	}
}

void gm20b_perf_enable_membuf(struct gk20a *g, u32 size, u64 buf_addr)
{
	u32 addr_lo;
	u32 addr_hi;

	addr_lo = u64_lo32(buf_addr);
	addr_hi = u64_hi32(buf_addr);

	nvgpu_writel(g, perf_pmasys_outbase_r(), addr_lo);
	nvgpu_writel(g, perf_pmasys_outbaseupper_r(),
		perf_pmasys_outbaseupper_ptr_f(addr_hi));
	nvgpu_writel(g, perf_pmasys_outsize_r(), size);

}

void gm20b_perf_disable_membuf(struct gk20a *g)
{
	nvgpu_writel(g, perf_pmasys_outbase_r(), 0);
	nvgpu_writel(g, perf_pmasys_outbaseupper_r(),
			perf_pmasys_outbaseupper_ptr_f(0));
	nvgpu_writel(g, perf_pmasys_outsize_r(), 0);
}

void gm20b_perf_init_inst_block(struct gk20a *g, struct nvgpu_mem *inst_block)
{
	u32 inst_block_ptr = nvgpu_inst_block_ptr(g, inst_block);

	nvgpu_writel(g, perf_pmasys_mem_block_r(),
		     perf_pmasys_mem_block_base_f(inst_block_ptr) |
		     perf_pmasys_mem_block_valid_true_f() |
		     nvgpu_aperture_mask(g, inst_block,
				perf_pmasys_mem_block_target_sys_ncoh_f(),
				perf_pmasys_mem_block_target_sys_coh_f(),
				perf_pmasys_mem_block_target_lfb_f()));
}

void gm20b_perf_deinit_inst_block(struct gk20a *g)
{
	nvgpu_writel(g, perf_pmasys_mem_block_r(),
			perf_pmasys_mem_block_base_f(0) |
			perf_pmasys_mem_block_valid_false_f() |
			perf_pmasys_mem_block_target_f(0));
}

u32 gm20b_perf_get_pmmsys_per_chiplet_offset(void)
{
	return (perf_pmmsys_extent_v() - perf_pmmsys_base_v() + 1U);
}

u32 gm20b_perf_get_pmmgpc_per_chiplet_offset(void)
{
	return (perf_pmmgpc_extent_v() - perf_pmmgpc_base_v() + 1U);
}

u32 gm20b_perf_get_pmmfbp_per_chiplet_offset(void)
{
	return (perf_pmmfbp_extent_v() - perf_pmmfbp_base_v() + 1U);
}
