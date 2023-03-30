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

#include <nvgpu/gk20a.h>
#include <nvgpu/channel.h>
#include <nvgpu/watchdog.h>
#include <nvgpu/error_notifier.h>
#include <nvgpu/watchdog.h>
#include <nvgpu/string.h>

struct nvgpu_channel_wdt {
	struct gk20a *g;

	/* lock protects the running timer state */
	struct nvgpu_spinlock lock;
	struct nvgpu_timeout timer;
	bool running;
	struct nvgpu_channel_wdt_state ch_state;

	/* lock not needed */
	u32 limit_ms;
	bool enabled;
};

struct nvgpu_channel_wdt *nvgpu_channel_wdt_alloc(struct gk20a *g)
{
	struct nvgpu_channel_wdt *wdt = nvgpu_kzalloc(g, sizeof(*wdt));

	if (wdt == NULL) {
		return NULL;
	}

	wdt->g = g;
	nvgpu_spinlock_init(&wdt->lock);
	wdt->enabled = true;
	wdt->limit_ms = g->ch_wdt_init_limit_ms;

	return wdt;
}

void nvgpu_channel_wdt_destroy(struct nvgpu_channel_wdt *wdt)
{
	nvgpu_kfree(wdt->g, wdt);
}

void nvgpu_channel_wdt_enable(struct nvgpu_channel_wdt *wdt)
{
	wdt->enabled = true;
}

void nvgpu_channel_wdt_disable(struct nvgpu_channel_wdt *wdt)
{
	wdt->enabled = false;
}

bool nvgpu_channel_wdt_enabled(struct nvgpu_channel_wdt *wdt)
{
	return wdt->enabled;
}

void nvgpu_channel_wdt_set_limit(struct nvgpu_channel_wdt *wdt, u32 limit_ms)
{
	wdt->limit_ms = limit_ms;
}

u32 nvgpu_channel_wdt_limit(struct nvgpu_channel_wdt *wdt)
{
	return wdt->limit_ms;
}

static void nvgpu_channel_wdt_init(struct nvgpu_channel_wdt *wdt,
		struct nvgpu_channel_wdt_state *state)
{
	struct gk20a *g = wdt->g;

	/*
	 * Note: this is intentionally not the sw kind of timer to avoid false
	 * triggers in pre-si environments that tend to run slow.
	 */
	nvgpu_timeout_init_cpu_timer(g, &wdt->timer, wdt->limit_ms);

	wdt->ch_state = *state;
	wdt->running = true;
}

/**
 * Start a timeout counter (watchdog) on this channel.
 *
 * Trigger a watchdog to recover the channel after the per-platform timeout
 * duration (but strictly no earlier) if the channel hasn't advanced within
 * that time.
 *
 * If the timeout is already running, do nothing. This should be called when
 * new jobs are submitted. The timeout will stop when the last tracked job
 * finishes, making the channel idle.
 */
void nvgpu_channel_wdt_start(struct nvgpu_channel_wdt *wdt,
		struct nvgpu_channel_wdt_state *state)
{
	if (!nvgpu_is_timeouts_enabled(wdt->g)) {
		return;
	}

	if (!wdt->enabled) {
		return;
	}

	nvgpu_spinlock_acquire(&wdt->lock);

	if (wdt->running) {
		nvgpu_spinlock_release(&wdt->lock);
		return;
	}
	nvgpu_channel_wdt_init(wdt, state);
	nvgpu_spinlock_release(&wdt->lock);
}

/**
 * Stop a running timeout counter (watchdog) on this channel.
 *
 * Make the watchdog consider the channel not running, so that it won't get
 * recovered even if no progress is detected. Progress is not tracked if the
 * watchdog is turned off.
 *
 * No guarantees are made about concurrent execution of the timeout handler.
 * (This should be called from an update handler running in the same thread
 * with the watchdog.)
 */
bool nvgpu_channel_wdt_stop(struct nvgpu_channel_wdt *wdt)
{
	bool was_running;

	nvgpu_spinlock_acquire(&wdt->lock);
	was_running = wdt->running;
	wdt->running = false;
	nvgpu_spinlock_release(&wdt->lock);
	return was_running;
}

/**
 * Continue a previously stopped timeout
 *
 * Enable the timeout again but don't reinitialize its timer.
 *
 * No guarantees are made about concurrent execution of the timeout handler.
 * (This should be called from an update handler running in the same thread
 * with the watchdog.)
 */
void nvgpu_channel_wdt_continue(struct nvgpu_channel_wdt *wdt)
{
	nvgpu_spinlock_acquire(&wdt->lock);
	wdt->running = true;
	nvgpu_spinlock_release(&wdt->lock);
}

/**
 * Reset the counter of a timeout that is in effect.
 *
 * If this channel has an active timeout, act as if something happened on the
 * channel right now.
 *
 * Rewinding a stopped counter is irrelevant; this is a no-op for non-running
 * timeouts. Stopped timeouts can only be started (which is technically a
 * rewind too) or continued (where the stop is actually pause).
 */
void nvgpu_channel_wdt_rewind(struct nvgpu_channel_wdt *wdt,
		struct nvgpu_channel_wdt_state *state)
{
	nvgpu_spinlock_acquire(&wdt->lock);
	if (wdt->running) {
		nvgpu_channel_wdt_init(wdt, state);
	}
	nvgpu_spinlock_release(&wdt->lock);
}

/**
 * Check if the watchdog is running.
 *
 * A running watchdog means one that is requested to run and expire in the
 * future. The state of a running watchdog has to be checked periodically to
 * see if it's expired.
 */
bool nvgpu_channel_wdt_running(struct nvgpu_channel_wdt *wdt)
{
	bool running;

	nvgpu_spinlock_acquire(&wdt->lock);
	running = wdt->running;
	nvgpu_spinlock_release(&wdt->lock);

	return running;
}

/**
 * Check if a channel has been stuck for the watchdog limit.
 *
 * Test if this channel has really got stuck at this point by checking if its
 * {gp,pb}_get have advanced or not. If progress was detected, start the timer
 * from zero again. If no {gp,pb}_get action happened in the watchdog time
 * limit, return true. Else return false.
 */
static bool nvgpu_channel_wdt_handler(struct nvgpu_channel_wdt *wdt,
		struct nvgpu_channel_wdt_state *state)
{
	struct gk20a *g = wdt->g;
	struct nvgpu_channel_wdt_state previous_state;

	nvgpu_log_fn(g, " ");

	/* Get status but keep timer running */
	nvgpu_spinlock_acquire(&wdt->lock);
	previous_state = wdt->ch_state;
	nvgpu_spinlock_release(&wdt->lock);

	if (nvgpu_memcmp((const u8 *)state,
			(const u8 *)&previous_state,
			sizeof(*state)) != 0) {
		/* Channel has advanced, timer keeps going but resets */
		nvgpu_channel_wdt_rewind(wdt, state);
		return false;
	}

	if (!nvgpu_timeout_peek_expired(&wdt->timer)) {
		/* Seems stuck but waiting to time out */
		return false;
	}

	return true;
}

/**
 * Test if the per-channel watchdog is on; check the timeout in that case.
 *
 * Each channel has an expiration time based watchdog. The timer is
 * (re)initialized in two situations: when a new job is submitted on an idle
 * channel and when the timeout is checked but progress is detected. The
 * watchdog timeout limit is a coarse sliding window.
 *
 * The timeout is stopped (disabled) after the last job in a row finishes
 * and marks the channel idle.
 */
bool nvgpu_channel_wdt_check(struct nvgpu_channel_wdt *wdt,
		struct nvgpu_channel_wdt_state *state)
{
	bool running;

	nvgpu_spinlock_acquire(&wdt->lock);
	running = wdt->running;
	nvgpu_spinlock_release(&wdt->lock);

	if (running) {
		return nvgpu_channel_wdt_handler(wdt, state);
	} else {
		return false;
	}
}
