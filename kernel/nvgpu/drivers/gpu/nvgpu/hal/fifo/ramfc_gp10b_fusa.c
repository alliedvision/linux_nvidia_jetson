/*
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

#include <nvgpu/log.h>
#include <nvgpu/log2.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/channel.h>
#include <nvgpu/fifo.h>

#include "hal/fifo/ramfc_gk20a.h"
#include "hal/fifo/ramfc_gp10b.h"

#include <nvgpu/hw/gp10b/hw_ram_gp10b.h>

int gp10b_ramfc_commit_userd(struct nvgpu_channel *ch)
{
	u32 addr_lo;
	u32 addr_hi;
	struct gk20a *g = ch->g;

	nvgpu_log_fn(g, " ");

	addr_lo = u64_lo32(ch->userd_iova >> ram_userd_base_shift_v());
	addr_hi = u64_hi32(ch->userd_iova);

	nvgpu_log_info(g, "channel %d : set ramfc userd 0x%16llx",
		ch->chid, (u64)ch->userd_iova);

	nvgpu_mem_wr32(g, &ch->inst_block,
		ram_in_ramfc_w() + ram_fc_userd_w(),
		g->ops.pbdma.get_userd_aperture_mask(g, ch->userd_mem) |
		g->ops.pbdma.get_userd_addr(addr_lo));

	nvgpu_mem_wr32(g, &ch->inst_block,
		ram_in_ramfc_w() + ram_fc_userd_hi_w(),
		g->ops.pbdma.get_userd_hi_addr(addr_hi));

	return 0;
}
