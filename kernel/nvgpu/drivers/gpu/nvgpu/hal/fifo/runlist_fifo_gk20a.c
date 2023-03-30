/*
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

#include <nvgpu/io.h>
#include <nvgpu/channel.h>
#include <nvgpu/runlist.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/fifo.h>
#include <nvgpu/engine_status.h>
#include <nvgpu/engines.h>
#include <nvgpu/device.h>
#include <nvgpu/gr/gr_falcon.h>

#include "runlist_fifo_gk20a.h"

#include <nvgpu/hw/gk20a/hw_fifo_gk20a.h>

#define FECS_MAILBOX_0_ACK_RESTORE 0x4U

u32 gk20a_runlist_count_max(struct gk20a *g)
{
	(void)g;
	return fifo_eng_runlist_base__size_1_v();
}

#ifdef CONFIG_NVGPU_CHANNEL_TSG_SCHEDULING
int gk20a_runlist_reschedule(struct nvgpu_channel *ch, bool preempt_next)
{
	return nvgpu_runlist_reschedule(ch, preempt_next, true);
}

/* trigger host preempt of GR pending load ctx if that ctx is not for ch */
int gk20a_fifo_reschedule_preempt_next(struct nvgpu_channel *ch,
		bool wait_preempt)
{
	struct gk20a *g = ch->g;
	struct nvgpu_runlist *runlist = ch->runlist;
	int ret = 0;
	u32 fecsstat0 = 0, fecsstat1 = 0;
	u32 preempt_id;
	u32 preempt_type = 0;
	const struct nvgpu_device *dev;
	struct nvgpu_engine_status_info engine_status;

	dev = nvgpu_device_get(g, NVGPU_DEVTYPE_GRAPHICS, 0);
	if (dev == NULL) {
		nvgpu_warn(g, "GPU has no GR engine?!");
		return -EINVAL;
	}

	if ((runlist->eng_bitmask & BIT32(dev->engine_id)) == 0U) {
		return ret;
	}

	if (wait_preempt) {
		u32 val = nvgpu_readl(g, fifo_preempt_r());

		if ((val & fifo_preempt_pending_true_f()) != 0U) {
			return ret;
		}
	}

	fecsstat0 = g->ops.gr.falcon.read_fecs_ctxsw_mailbox(g,
			NVGPU_GR_FALCON_FECS_CTXSW_MAILBOX0);
	g->ops.engine_status.read_engine_status_info(g, dev->engine_id,
						     &engine_status);
	if (nvgpu_engine_status_is_ctxsw_switch(&engine_status)) {
		nvgpu_engine_status_get_next_ctx_id_type(&engine_status,
			&preempt_id, &preempt_type);
	} else {
		return ret;
	}
	if ((preempt_id == ch->tsgid) && (preempt_type != 0U)) {
		return ret;
	}
	fecsstat1 = g->ops.gr.falcon.read_fecs_ctxsw_mailbox(g,
			NVGPU_GR_FALCON_FECS_CTXSW_MAILBOX0);
	if (fecsstat0 != FECS_MAILBOX_0_ACK_RESTORE ||
		fecsstat1 != FECS_MAILBOX_0_ACK_RESTORE) {
		/* preempt useless if FECS acked save and started restore */
		return ret;
	}

	g->ops.fifo.preempt_trigger(g, preempt_id, preempt_type != 0U);
#ifdef TRACEPOINTS_ENABLED
	trace_gk20a_reschedule_preempt_next(ch->chid, fecsstat0,
		engine_status.reg_data, fecsstat1,
		g->ops.gr.falcon.read_fecs_ctxsw_mailbox(g,
			NVGPU_GR_FALCON_FECS_CTXSW_MAILBOX0),
		nvgpu_readl(g, fifo_preempt_r()));
#endif
	if (wait_preempt) {
		if (g->ops.fifo.is_preempt_pending(g, preempt_id,
			preempt_type, false) != 0) {
			nvgpu_err(g, "fifo preempt timed out");
			/*
			 * This function does not care if preempt
			 * times out since it is here only to improve
			 * latency. If a timeout happens, it will be
			 * handled by other fifo handling code.
			 */
		}
	}
#ifdef TRACEPOINTS_ENABLED
	trace_gk20a_reschedule_preempted_next(ch->chid);
#endif
	return ret;
}
#endif
