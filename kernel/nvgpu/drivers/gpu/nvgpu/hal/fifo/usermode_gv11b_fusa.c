/*
 * Copyright (c) 2015-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/io_usermode.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/channel.h>
#include <nvgpu/fifo.h>
#include <nvgpu/static_analysis.h>

#include "usermode_gv11b.h"

#include <nvgpu/hw/gv11b/hw_usermode_gv11b.h>

u64 gv11b_usermode_base(struct gk20a *g)
{
	(void)g;
	return usermode_cfg0_r();
}

u64 gv11b_usermode_bus_base(struct gk20a *g)
{
	(void)g;
	return usermode_cfg0_r();
}

u32 gv11b_usermode_doorbell_token(struct nvgpu_channel *ch)
{
	struct gk20a *g = ch->g;
	struct nvgpu_fifo *f = &g->fifo;
	u32 hw_chid = nvgpu_safe_add_u32(f->channel_base, ch->chid);

	return usermode_notify_channel_pending_id_f(hw_chid);
}

void gv11b_usermode_ring_doorbell(struct nvgpu_channel *ch)
{
	nvgpu_log_info(ch->g, "channel ring door bell %d", ch->chid);

	nvgpu_usermode_writel(ch->g, usermode_notify_channel_pending_r(),
		gv11b_usermode_doorbell_token(ch));
}
