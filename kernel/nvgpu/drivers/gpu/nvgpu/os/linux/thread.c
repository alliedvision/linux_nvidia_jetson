/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/kthread.h>

#include <nvgpu/thread.h>
#include <nvgpu/timers.h>

int nvgpu_thread_proxy(void *threaddata)
{
	struct nvgpu_thread *thread = threaddata;
	int ret = thread->fn(thread->data);
	bool was_running;

	was_running = nvgpu_atomic_xchg(&thread->running, false);

	/* if the thread was not running, then nvgpu_thread_stop() was
	 * called, so just wait until we get the notification that we should
	 * stop.
	 */
	if (!was_running) {
		while (!nvgpu_thread_should_stop(thread)) {
			nvgpu_usleep_range(5000, 5100);
		}
	}
	return ret;
}

int nvgpu_thread_create(struct nvgpu_thread *thread,
		void *data,
		int (*threadfn)(void *data), const char *name)
{
	struct task_struct *task = kthread_create(nvgpu_thread_proxy,
			thread, name);
	if (IS_ERR(task))
		return PTR_ERR(task);

	thread->task = task;
	thread->fn = threadfn;
	thread->data = data;
	nvgpu_atomic_set(&thread->running, true);
	wake_up_process(task);
	return 0;
};

void nvgpu_thread_stop(struct nvgpu_thread *thread)
{
	bool was_running;

	if (thread->task) {
		was_running = nvgpu_atomic_xchg(&thread->running, false);

		if (was_running) {
			kthread_stop(thread->task);
		}
		thread->task = NULL;
	}
};

void nvgpu_thread_stop_graceful(struct nvgpu_thread *thread,
		void (*thread_stop_fn)(void *data), void *data)
{
	/*
	 * Threads waiting on wq's should have nvgpu_thread_should_stop()
	 * as one of its wakeup condition. This allows the thread to be woken
	 * up when kthread_stop() is invoked and does not require an additional
	 * callback to wakeup the sleeping thread.
	 */
	nvgpu_thread_stop(thread);
};

bool nvgpu_thread_should_stop(struct nvgpu_thread *thread)
{
	return kthread_should_stop();
};

bool nvgpu_thread_is_running(struct nvgpu_thread *thread)
{
	return nvgpu_atomic_read(&thread->running);
};

void nvgpu_thread_join(struct nvgpu_thread *thread)
{
	while (nvgpu_atomic_read(&thread->running)) {
		nvgpu_msleep(10);
	}
};
