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

#include <nvgpu/types.h>
#include <nvgpu/io.h>
#include <nvgpu/log.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/channel.h>
#include <nvgpu/io_usermode.h>
#include <nvgpu/runlist.h>

#include "usermode_tu104.h"

#include <nvgpu/hw/tu104/hw_usermode_tu104.h>
#include <nvgpu/hw/tu104/hw_ctrl_tu104.h>
#include <nvgpu/hw/tu104/hw_func_tu104.h>

u64 tu104_usermode_base(struct gk20a *g)
{
	return func_cfg0_r();
}

u64 tu104_usermode_bus_base(struct gk20a *g)
{
	return U64(func_full_phys_offset_v() + func_cfg0_r());
}

void tu104_usermode_setup_hw(struct gk20a *g)
{
	u32 val;

	val = nvgpu_readl(g, ctrl_virtual_channel_cfg_r(0));
	val |= ctrl_virtual_channel_cfg_pending_enable_true_f();
	nvgpu_writel(g, ctrl_virtual_channel_cfg_r(0), val);
}

u32 tu104_usermode_doorbell_token(struct nvgpu_channel *ch)
{
	struct gk20a *g = ch->g;
	struct nvgpu_fifo *f = &g->fifo;
	u32 hw_chid = f->channel_base + ch->chid;

	return ctrl_doorbell_vector_f(hw_chid) |
			ctrl_doorbell_runlist_id_f(ch->runlist->id);
}

void tu104_usermode_ring_doorbell(struct nvgpu_channel *ch)
{
	nvgpu_log_info(ch->g, "channel ring door bell %d, runlist %d",
			ch->chid, ch->runlist->id);

	nvgpu_usermode_writel(ch->g, func_doorbell_r(),
				ch->g->ops.usermode.doorbell_token(ch));
}
