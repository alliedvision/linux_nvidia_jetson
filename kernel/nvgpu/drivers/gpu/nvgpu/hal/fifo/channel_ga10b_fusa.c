/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/runlist.h>
#include <nvgpu/log.h>
#include <nvgpu/atomic.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/bug.h>
#include <nvgpu/string.h>
#include <nvgpu/kmem.h>
#include <nvgpu/log2.h>
#include <nvgpu/static_analysis.h>

#include <hal/fifo/fifo_utils_ga10b.h>
#include "channel_ga10b.h"

#include <nvgpu/hw/ga10b/hw_runlist_ga10b.h>

#define NUM_CHANNELS		512U
#define CHANNEL_BOUND		1
#define CHANNEL_UNBOUND		0

u32 ga10b_channel_count(struct gk20a *g)
{
	/* Limit number of channels, avoids unnecessary memory allocation */
	nvgpu_log(g, gpu_dbg_info, "Number of channels supported by hw = %u",
		((0x1U) << runlist_channel_config_num_channels_log2_2k_v()));

	nvgpu_log(g, gpu_dbg_info, "Number of channels supported by sw = %u",
		NUM_CHANNELS);

	return NUM_CHANNELS;
}

void ga10b_channel_enable(struct nvgpu_channel *ch)
{
	struct gk20a *g = ch->g;
	struct nvgpu_runlist *runlist = NULL;

	runlist = ch->runlist;

	nvgpu_chram_bar0_writel(g, runlist, runlist_chram_channel_r(ch->chid),
		runlist_chram_channel_update_f(
			runlist_chram_channel_update_enable_channel_v()));
}

void ga10b_channel_disable(struct nvgpu_channel *ch)
{
	struct gk20a *g = ch->g;
	struct nvgpu_runlist *runlist = NULL;

	runlist = ch->runlist;

	nvgpu_chram_bar0_writel(g, runlist, runlist_chram_channel_r(ch->chid),
		runlist_chram_channel_update_f(
			runlist_chram_channel_update_disable_channel_v()));
}

void ga10b_channel_bind(struct nvgpu_channel *ch)
{
	struct gk20a *g = ch->g;
	struct nvgpu_runlist *runlist = NULL;

	runlist = ch->runlist;

	/* Enable channel */
	nvgpu_chram_bar0_writel(g, runlist, runlist_chram_channel_r(ch->chid),
		runlist_chram_channel_update_f(
			runlist_chram_channel_update_enable_channel_v()));

	nvgpu_atomic_set(&ch->bound, CHANNEL_BOUND);
}

/*
 * The instance associated with a channel is specified in the channel's
 * runlist entry. Ampere has no notion of binding/unbinding channels
 * to instances. When tearing down a channel or migrating its chid,
 * after ensuring it is unloaded and unrunnable, SW must clear the
 * channel's entry in the channel RAM by writing
 * NV_CHRAM_CHANNEL_UPDATE_CLEAR_CHANNEL to NV_CHRAM_CHANNEL(chid).
 *
 * Note: From GA10x onwards, channel RAM clear is one of the
 * important steps in RC recovery and channel removal.
 * Channel Removal Sequence:
 * SW may also need to remove some channels from a TSG in order to
 * support shutdown of a specific subcontext in that TSG.  In this case
 * it's important for SW to take care to properly clear the channel RAM
 * state of the removed channels and to transfer CTX_RELOAD to some
 * other channel that will not be removed. The procedure is as follows:
 * 1. Disable all the channels in the TSG (or disable scheduling on the
 *    runlist)
 * 2. Preempt the TSG (or runlist)
 * 3. Poll for completion of the preempt (possibly making use of the
 *    appropriate PREEMPT interrupt to avoid the spin loop).
 *    While polling, SW must check for interrupts and hangs.
 *    If a teardown is required, stop following this sequence and
 *    continue with the teardown sequence from step 4.
 * 4. Read the channel RAM for the removed channels to see if CTX_RELOAD
 *    is set on any of them. If so, force CTX_RELOAD on some other
 *    channel that isn't being removed by writing
 *    NV_CHRAM_CHANNEL_UPDATE_FORCE_CTX_RELOAD to chosen channel's chram
 * 5. Write NV_CHRAM_CHANNEL_UPDATE_CLEAR_CHANNEL to removed channels.
 *    This ensures the channels are ready for reuse without confusing
 *    esched's tracking.
 * 6. Submit a new runlist without the removed channels and reenable
 *    scheduling if disabled in step 1.
 * 7. Re-enable all the non-removed channels in the TSG.
 */
void ga10b_channel_unbind(struct nvgpu_channel *ch)
{
	struct gk20a *g = ch->g;
	struct nvgpu_runlist *runlist = NULL;

	runlist = ch->runlist;

	if (nvgpu_atomic_cmpxchg(&ch->bound, CHANNEL_BOUND, CHANNEL_UNBOUND) !=
			0) {
		nvgpu_chram_bar0_writel(g, runlist,
			runlist_chram_channel_r(ch->chid),
			runlist_chram_channel_update_f(
			runlist_chram_channel_update_clear_channel_v()));
	}
}

#define NUM_STATUS_STR		8U

static u32 ga10b_channel_status_mask(void)
{
	u32 mask = (runlist_chram_channel_on_pbdma_m() |
		runlist_chram_channel_on_eng_m() |
		runlist_chram_channel_pending_m() |
		runlist_chram_channel_ctx_reload_m() |
		runlist_chram_channel_pbdma_busy_m() |
		runlist_chram_channel_eng_busy_m() |
		runlist_chram_channel_acquire_fail_m());

	return mask;
}

static const char * const chram_status_str[] = {
	[runlist_chram_channel_on_pbdma_m()] = "on_pbdma",
	[runlist_chram_channel_on_eng_m()] = "on_eng",
	[runlist_chram_channel_pending_m()] = "pending",
	[runlist_chram_channel_ctx_reload_m()] = "ctx_reload",
	[runlist_chram_channel_pbdma_busy_m()] = "pbdma_busy",
	[runlist_chram_channel_eng_busy_m()] = "eng_busy",
	[runlist_chram_channel_acquire_fail_m()] = "acquire_fail",
};

void ga10b_channel_read_state(struct gk20a *g, struct nvgpu_channel *ch,
			struct nvgpu_channel_hw_state *state)
{
	u32 reg = 0U;
	unsigned long bit = 0UL;
	unsigned long status_str_bits = 0UL;
	u32 status_str_count = 0U;
	bool idle = true;
	struct nvgpu_runlist *runlist = NULL;
	const char **chram_status_list = NULL;

	runlist = ch->runlist;

	reg = nvgpu_chram_bar0_readl(g, runlist,
			runlist_chram_channel_r(ch->chid));

	state->next = runlist_chram_channel_next_v(reg) ==
				runlist_chram_channel_next_true_v();
	state->enabled = runlist_chram_channel_enable_v(reg) ==
				runlist_chram_channel_enable_in_use_v();
        state->ctx_reload = runlist_chram_channel_ctx_reload_v(reg) ==
				runlist_chram_channel_ctx_reload_true_v();
        state->busy = runlist_chram_channel_busy_v(reg) ==
				runlist_chram_channel_busy_true_v();
	state->pending_acquire =
		((runlist_chram_channel_pending_v(reg) ==
			runlist_chram_channel_pending_true_v()) &&
		(runlist_chram_channel_acquire_fail_v(reg) ==
			runlist_chram_channel_acquire_fail_true_v()));

	state->eng_faulted = runlist_chram_channel_eng_faulted_v(reg) ==
				runlist_chram_channel_eng_faulted_true_v();

	/* Construct status string for below status fields */
	status_str_bits = (u64)(reg & ga10b_channel_status_mask());

	/* Allocate memory for status string list */
	chram_status_list = nvgpu_kzalloc(g, (sizeof(char *) * NUM_STATUS_STR));
	if (chram_status_list == NULL) {
		nvgpu_err(g, "Status string list pointer allocation failed");
		state->status_string[0] = '\0';
		return;
	}

	/*
	 * Status is true if the corresponding bit is set.
	 * Go through each set bit and copy status string to status string list.
	 */
	for_each_set_bit(bit, &status_str_bits, nvgpu_ilog2(U32_MAX)) {
		chram_status_list[status_str_count] =
					chram_status_str[BIT32(bit)];
		status_str_count = nvgpu_safe_add_u32(status_str_count, 1UL);
		idle = false;
	}

	if (idle) {
		chram_status_list[status_str_count] = "idle";
		status_str_count = nvgpu_safe_add_u32(status_str_count, 1UL);
	}

	/* Combine all status strings */
	(void) nvgpu_str_join(state->status_string,
		NVGPU_CHANNEL_STATUS_STRING_LENGTH, chram_status_list,
		status_str_count, ", ");

	nvgpu_log_info(g, "Channel id:%d state next:%s enabled:%s ctx_reload:%s"
		" busy:%s pending_acquire:%s eng_faulted:%s status_string:%s",
		ch->chid,
		state->next ? "true" : "false",
		state->enabled ? "true" : "false",
		state->ctx_reload ? "true" : "false",
		state->busy ? "true" : "false",
		state->pending_acquire ? "true" : "false",
		state->eng_faulted ? "true" : "false", state->status_string);

	nvgpu_kfree(g, chram_status_list);
}

void ga10b_channel_reset_faulted(struct gk20a *g, struct nvgpu_channel *ch,
		bool eng, bool pbdma)
{
	struct nvgpu_runlist *runlist = NULL;

	runlist = ch->runlist;

	if (eng) {
		nvgpu_chram_bar0_writel(g, runlist,
			runlist_chram_channel_r(ch->chid),
			runlist_chram_channel_update_f(
			runlist_chram_channel_update_reset_eng_faulted_v()));
	}
	if (pbdma) {
		nvgpu_chram_bar0_writel(g, runlist,
			runlist_chram_channel_r(ch->chid),
			runlist_chram_channel_update_f(
			runlist_chram_channel_update_reset_pbdma_faulted_v()));
	}

	/*
	 * At this point the fault is handled and *_FAULTED bit is cleared.
	 * However, if the runlist has gone idle, then the esched unit
	 * will remain idle and will not schedule the runlist unless its
	 * doorbell is written or a new runlist is submitted. Hence, ring the
	 * runlist doorbell once the fault is cleared.
	 */
	g->ops.usermode.ring_doorbell(ch);

}

void ga10b_channel_force_ctx_reload(struct nvgpu_channel *ch)
{
	struct gk20a *g = ch->g;
	struct nvgpu_runlist *runlist = NULL;

	runlist = ch->runlist;

	nvgpu_chram_bar0_writel(g, runlist, runlist_chram_channel_r(ch->chid),
		runlist_chram_channel_update_f(
			runlist_chram_channel_update_force_ctx_reload_v()));
}
