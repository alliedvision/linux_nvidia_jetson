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

#include "channel_wdt.h"
#include "channel_worker.h"

#include <nvgpu/watchdog.h>
#include <nvgpu/channel.h>
#include <nvgpu/error_notifier.h>
#include <nvgpu/gk20a.h>

void nvgpu_channel_set_wdt_debug_dump(struct nvgpu_channel *ch, bool dump)
{
	ch->wdt_debug_dump = dump;
}

static struct nvgpu_channel_wdt_state nvgpu_channel_collect_wdt_state(
		struct nvgpu_channel *ch)
{
	struct gk20a *g = ch->g;
	struct nvgpu_channel_wdt_state state = { 0, 0 };

	/*
	 * Note: just checking for nvgpu_channel_wdt_enabled() is not enough at
	 * the moment because system suspend puts g->regs away but doesn't stop
	 * the worker thread that runs the watchdog. This might need to be
	 * cleared up in the future.
	 */
	if (nvgpu_channel_wdt_running(ch->wdt)) {
		/*
		 * Read the state only if the wdt is on to avoid unnecessary
		 * accesses. The kernel mem for userd may not even exist; this
		 * channel could be in usermode submit mode.
		 */
		state.gp_get = g->ops.userd.gp_get(g, ch);
		state.pb_get = g->ops.userd.pb_get(g, ch);
	}

	return state;
}

void nvgpu_channel_launch_wdt(struct nvgpu_channel *ch)
{
	struct nvgpu_channel_wdt_state state = nvgpu_channel_collect_wdt_state(ch);

	/*
	 * FIXME: channel recovery can race the submit path and can start even
	 * after this, but this check is the best we can do for now.
	 */
	if (!nvgpu_channel_check_unserviceable(ch)) {
		nvgpu_channel_wdt_start(ch->wdt, &state);
	}
}

void nvgpu_channel_restart_all_wdts(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;
	u32 chid;

	for (chid = 0; chid < f->num_channels; chid++) {
		struct nvgpu_channel *ch = nvgpu_channel_from_id(g, chid);

		if (ch != NULL) {
			if ((ch->wdt != NULL) &&
			    !nvgpu_channel_check_unserviceable(ch)) {
				struct nvgpu_channel_wdt_state state =
					nvgpu_channel_collect_wdt_state(ch);

				nvgpu_channel_wdt_rewind(ch->wdt, &state);
			}
			nvgpu_channel_put(ch);
		}
	}
}

static void nvgpu_channel_recover_from_wdt(struct nvgpu_channel *ch)
{
	struct gk20a *g = ch->g;

	nvgpu_log_fn(g, " ");

	if (nvgpu_channel_check_unserviceable(ch)) {
		/* channel is already recovered */
		nvgpu_info(g, "chid: %d unserviceable but wdt was ON", ch->chid);
		return;
	}

	nvgpu_err(g, "Job on channel %d timed out", ch->chid);

	/* force reset calls gk20a_debug_dump but not this */
	if (ch->wdt_debug_dump) {
		gk20a_gr_debug_dump(g);
	}

#ifdef CONFIG_NVGPU_CHANNEL_TSG_CONTROL
	if (g->ops.tsg.force_reset(ch,
	    NVGPU_ERR_NOTIFIER_FIFO_ERROR_IDLE_TIMEOUT,
	    ch->wdt_debug_dump) != 0) {
		nvgpu_err(g, "failed tsg force reset for chid: %d", ch->chid);
	}
#endif
}

/*
 * Test the watchdog progress. If the channel is stuck, reset it.
 *
 * The gpu is implicitly on at this point because the watchdog can only run on
 * channels that have submitted jobs pending for cleanup.
 */
static void nvgpu_channel_check_wdt(struct nvgpu_channel *ch)
{
	struct nvgpu_channel_wdt_state state = nvgpu_channel_collect_wdt_state(ch);

	if (nvgpu_channel_wdt_check(ch->wdt, &state)) {
		nvgpu_channel_recover_from_wdt(ch);
	}
}

void nvgpu_channel_worker_poll_init(struct nvgpu_worker *worker)
{
	struct nvgpu_channel_worker *ch_worker =
		nvgpu_channel_worker_from_worker(worker);

	ch_worker->watchdog_interval = 100U;

	nvgpu_timeout_init_cpu_timer_sw(worker->g, &ch_worker->timeout,
			ch_worker->watchdog_interval);
}

/**
 * Loop every living channel, check timeouts and handle stuck channels.
 */
static void nvgpu_channel_poll_wdt(struct gk20a *g)
{
	unsigned int chid;

	for (chid = 0; chid < g->fifo.num_channels; chid++) {
		struct nvgpu_channel *ch = nvgpu_channel_from_id(g, chid);

		if (ch != NULL) {
			if (!nvgpu_channel_check_unserviceable(ch)) {
				nvgpu_channel_check_wdt(ch);
			}
			nvgpu_channel_put(ch);
		}
	}
}

void nvgpu_channel_worker_poll_wakeup_post_process_item(
		struct nvgpu_worker *worker)
{
	struct gk20a *g = worker->g;

	struct nvgpu_channel_worker *ch_worker =
		nvgpu_channel_worker_from_worker(worker);

	if (nvgpu_timeout_peek_expired(&ch_worker->timeout)) {
		nvgpu_channel_poll_wdt(g);
		nvgpu_timeout_init_cpu_timer_sw(g, &ch_worker->timeout,
				ch_worker->watchdog_interval);
	}
}

u32 nvgpu_channel_worker_poll_wakeup_condition_get_timeout(
		struct nvgpu_worker *worker)
{
	struct nvgpu_channel_worker *ch_worker =
		nvgpu_channel_worker_from_worker(worker);

	return ch_worker->watchdog_interval;
}
