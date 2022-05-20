/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/types.h>
#include <nvgpu/bug.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_init.h>

static void nvgpu_warn_on_no_regs(struct gk20a *g, u32 r)
{
	nvgpu_warn(g, "Attempted access to GPU regs after unmapping! r=0x%08x", r);
	WARN_ON(1);
}

void nvgpu_writel(struct gk20a *g, u32 r, u32 v)
{
	if (unlikely(!g->regs)) {
		nvgpu_warn_on_no_regs(g, r);
		nvgpu_log(g, gpu_dbg_reg, "r=0x%x v=0x%x (failed)", r, v);
	} else {
		nvgpu_os_writel(v, g->regs + r);
		nvgpu_wmb();
		nvgpu_log(g, gpu_dbg_reg, "r=0x%x v=0x%x", r, v);
	}
}

#ifdef CONFIG_NVGPU_DGPU
void nvgpu_writel_relaxed(struct gk20a *g, u32 r, u32 v)
{
	if (unlikely(!g->regs)) {
		nvgpu_warn_on_no_regs(g, r);
		nvgpu_log(g, gpu_dbg_reg, "r=0x%x v=0x%x (failed)", r, v);
	} else {
		nvgpu_os_writel_relaxed(v, g->regs + r);
	}
}
#endif

u32 nvgpu_readl(struct gk20a *g, u32 r)
{
	u32 v = nvgpu_readl_impl(g, r);

	if (v == 0xffffffff)
		nvgpu_check_gpu_state(g);

	return v;
}

u32 nvgpu_readl_impl(struct gk20a *g, u32 r)
{
	u32 v = 0xffffffff;

	if (unlikely(!g->regs)) {
		nvgpu_warn_on_no_regs(g, r);
		nvgpu_log(g, gpu_dbg_reg, "r=0x%x v=0x%x (failed)", r, v);
	} else {
		v = nvgpu_os_readl(g->regs + r);
		nvgpu_log(g, gpu_dbg_reg, "r=0x%x v=0x%x", r, v);
	}

	return v;
}

void nvgpu_writel_loop(struct gk20a *g, u32 r, u32 v)
{
	if (unlikely(!g->regs)) {
		nvgpu_warn_on_no_regs(g, r);
		nvgpu_log(g, gpu_dbg_reg, "r=0x%x v=0x%x (failed)", r, v);
	} else {
		nvgpu_wmb();
		do {
			nvgpu_os_writel(v, g->regs + r);
		} while (nvgpu_os_readl(g->regs + r) != v);
		nvgpu_log(g, gpu_dbg_reg, "r=0x%x v=0x%x", r, v);
	}
}

void nvgpu_bar1_writel(struct gk20a *g, u32 b, u32 v)
{
	if (unlikely(!g->bar1)) {
		nvgpu_warn_on_no_regs(g, b);
		nvgpu_log(g, gpu_dbg_reg, "b=0x%x v=0x%x (failed)", b, v);
	} else {
		nvgpu_wmb();
		nvgpu_os_writel(v, g->bar1 + b);
		nvgpu_log(g, gpu_dbg_reg, "b=0x%x v=0x%x", b, v);
	}
}

u32 nvgpu_bar1_readl(struct gk20a *g, u32 b)
{
	u32 v = 0xffffffff;

	if (unlikely(!g->bar1)) {
		nvgpu_warn_on_no_regs(g, b);
		nvgpu_log(g, gpu_dbg_reg, "b=0x%x v=0x%x (failed)", b, v);
	} else {
		v = nvgpu_os_readl(g->bar1 + b);
		nvgpu_log(g, gpu_dbg_reg, "b=0x%x v=0x%x", b, v);
	}

	return v;
}

bool nvgpu_io_exists(struct gk20a *g)
{
	return g->regs != 0U;
}

bool nvgpu_io_valid_reg(struct gk20a *g, u32 r)
{
	return r < g->regs_size;
}

void nvgpu_writel_check(struct gk20a *g, u32 r, u32 v)
{
	u32 read_val = 0U;

	nvgpu_writel(g, r, v);
	read_val = nvgpu_readl(g, r);
	if (v != read_val) {
		nvgpu_err(g, "r=0x%x rd=0x%x wr=0x%x (mismatch)",
					r, read_val, v);
		BUG_ON(1);
	}
}

void nvgpu_func_writel(struct gk20a *g, u32 r, u32 v)
{
	if (g->ops.func.get_full_phys_offset == NULL) {
		BUG_ON(1);
	}
	nvgpu_writel(g,
		nvgpu_safe_add_u32(r, g->ops.func.get_full_phys_offset(g)), v);
}

u32 nvgpu_func_readl(struct gk20a *g, u32 r)
{
	if (g->ops.func.get_full_phys_offset == NULL) {
		BUG_ON(1);
	}
	return nvgpu_readl(g,
		nvgpu_safe_add_u32(r, g->ops.func.get_full_phys_offset(g)));
}
