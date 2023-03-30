/*
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <nvgpu/bug.h>
#include <nvgpu/periodic_timer.h>
#include <linux/async.h>

static void async_func(void *data, async_cookie_t cookie)
{
	struct nvgpu_periodic_timer *timer = data;

	timer->fn(timer->arg);
	raw_spin_lock(&timer->lock);
	if (timer->enabled) {
		hrtimer_start(&timer->timer, timer->interval, HRTIMER_MODE_REL);
	}
	raw_spin_unlock(&timer->lock);
}

static enum hrtimer_restart timer_callback(struct hrtimer *os_timer)
{
	struct nvgpu_periodic_timer *timer =
			container_of(os_timer, struct nvgpu_periodic_timer, timer);

	timer->async_cookie = async_schedule(async_func, timer);
	return HRTIMER_NORESTART;
}

int nvgpu_periodic_timer_init(struct nvgpu_periodic_timer *timer,
			void (*fn)(void *arg), void *arg)
{
	hrtimer_init(&timer->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timer->timer.function = timer_callback;
	timer->fn = fn;
	timer->arg = arg;
	timer->interval = ktime_set(0, 0);
	timer->async_cookie = 0;
	timer->enabled = false;
	raw_spin_lock_init(&timer->lock);
	return 0;
}

int nvgpu_periodic_timer_start(struct nvgpu_periodic_timer *timer,
				u64 interval_ns)
{
	raw_spin_lock(&timer->lock);
	timer->interval = ns_to_ktime(interval_ns);
	timer->enabled = true;
	timer->async_cookie = 0;
	hrtimer_start(&timer->timer, timer->interval, HRTIMER_MODE_REL);
	raw_spin_unlock(&timer->lock);

	return 0;
}

int nvgpu_periodic_timer_stop(struct nvgpu_periodic_timer *timer)
{
	raw_spin_lock(&timer->lock);
	if (!timer->enabled) {
		raw_spin_unlock(&timer->lock);
		return 0;
	}
	timer->enabled = false;
	raw_spin_unlock(&timer->lock);
	hrtimer_cancel(&timer->timer);
	if (timer->async_cookie != 0) {
		async_synchronize_cookie(timer->async_cookie + 1);
	}
	return 0;
}

int nvgpu_periodic_timer_destroy(struct nvgpu_periodic_timer *timer)
{
	return nvgpu_periodic_timer_stop(timer);
}
