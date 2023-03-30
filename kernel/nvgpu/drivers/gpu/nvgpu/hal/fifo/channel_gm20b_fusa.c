/*
 * Copyright (c) 2014-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/channel.h>
#include <nvgpu/log.h>
#include <nvgpu/atomic.h>
#include <nvgpu/io.h>
#include <nvgpu/barrier.h>
#include <nvgpu/mm.h>
#include <nvgpu/gk20a.h>

#include "channel_gm20b.h"

#include <nvgpu/hw/gm20b/hw_ccsr_gm20b.h>
#include <nvgpu/hw/gm20b/hw_ram_gm20b.h>

void gm20b_channel_bind(struct nvgpu_channel *c)
{
	struct gk20a *g = c->g;

	u32 inst_ptr = nvgpu_inst_block_ptr(g, &c->inst_block);

	nvgpu_log_info(g, "bind channel %d inst ptr 0x%08x",
		c->chid, inst_ptr);

	nvgpu_writel(g, ccsr_channel_inst_r(c->chid),
		     ccsr_channel_inst_ptr_f(inst_ptr) |
		     nvgpu_aperture_mask(g, &c->inst_block,
				ccsr_channel_inst_target_sys_mem_ncoh_f(),
				ccsr_channel_inst_target_sys_mem_coh_f(),
				ccsr_channel_inst_target_vid_mem_f()) |
		     ccsr_channel_inst_bind_true_f());

	nvgpu_writel(g, ccsr_channel_r(c->chid),
		(nvgpu_readl(g, ccsr_channel_r(c->chid)) &
		 ~ccsr_channel_enable_set_f(~U32(0U))) |
		 ccsr_channel_enable_set_true_f());

	nvgpu_atomic_set(&c->bound, 1);
}

void gm20b_channel_force_ctx_reload(struct nvgpu_channel *ch)
{
	struct gk20a *g = ch->g;
	u32 reg = nvgpu_readl(g, ccsr_channel_r(ch->chid));

	nvgpu_writel(g, ccsr_channel_r(ch->chid),
		reg | ccsr_channel_force_ctx_reload_true_f());
}
