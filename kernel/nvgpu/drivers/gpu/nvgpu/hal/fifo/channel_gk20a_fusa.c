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

#include <nvgpu/channel.h>
#include <nvgpu/log.h>
#include <nvgpu/atomic.h>
#include <nvgpu/io.h>
#include <nvgpu/barrier.h>
#include <nvgpu/bug.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/string.h>

#include "hal/fifo/pbdma_gm20b.h"

#include "channel_gk20a.h"

#include <nvgpu/hw/gk20a/hw_ccsr_gk20a.h>

void gk20a_channel_enable(struct nvgpu_channel *ch)
{
	nvgpu_writel(ch->g, ccsr_channel_r(ch->chid),
		gk20a_readl(ch->g, ccsr_channel_r(ch->chid)) |
		ccsr_channel_enable_set_true_f());
}

void gk20a_channel_disable(struct nvgpu_channel *ch)
{
	nvgpu_writel(ch->g, ccsr_channel_r(ch->chid),
		gk20a_readl(ch->g,
			ccsr_channel_r(ch->chid)) |
			ccsr_channel_enable_clr_true_f());
}

/* ccsr_channel_status_v is four bits long */
static const char * const ccsr_chan_status_str[] = {
	"idle",
	"pending",
	"pending_ctx_reload",
	"pending_acquire",
	"pending_acq_ctx_reload",
	"on_pbdma",
	"on_pbdma_and_eng",
	"on_eng",
	"on_eng_pending_acquire",
	"on_eng_pending",
	"on_pbdma_ctx_reload",
	"on_pbdma_and_eng_ctx_reload",
	"on_eng_ctx_reload",
	"on_eng_pending_ctx_reload",
	"on_eng_pending_acq_ctx_reload",
	"N/A",
};

void gk20a_channel_read_state(struct gk20a *g, struct nvgpu_channel *ch,
		struct nvgpu_channel_hw_state *state)
{
	u32 reg = nvgpu_readl(g, ccsr_channel_r(ch->chid));
	u32 status_v = ccsr_channel_status_v(reg);

	state->next = ccsr_channel_next_v(reg) == ccsr_channel_next_true_v();
	state->enabled = ccsr_channel_enable_v(reg) ==
			    ccsr_channel_enable_in_use_v();
	state->ctx_reload =
		(status_v ==
			ccsr_channel_status_pending_ctx_reload_v()) ||
		(status_v ==
			ccsr_channel_status_pending_acq_ctx_reload_v()) ||
		(status_v ==
			ccsr_channel_status_on_pbdma_ctx_reload_v()) ||
		(status_v ==
			ccsr_channel_status_on_pbdma_and_eng_ctx_reload_v()) ||
		(status_v ==
			ccsr_channel_status_on_eng_ctx_reload_v()) ||
		(status_v ==
			ccsr_channel_status_on_eng_pending_ctx_reload_v()) ||
		(status_v ==
			ccsr_channel_status_on_eng_pending_acq_ctx_reload_v());
	state->busy = ccsr_channel_busy_v(reg) == ccsr_channel_busy_true_v();
	state->pending_acquire =
		(status_v == ccsr_channel_status_pending_acquire_v()) ||
		(status_v == ccsr_channel_status_on_eng_pending_acquire_v());

	/* Copy at the most NVGPU_CHANNEL_STATUS_STRING_LENGTH characters */
	(void) strncpy(state->status_string, ccsr_chan_status_str[status_v],
		NVGPU_CHANNEL_STATUS_STRING_LENGTH - 1U);

	state->status_string[NVGPU_CHANNEL_STATUS_STRING_LENGTH - 1U] = '\0';
}
