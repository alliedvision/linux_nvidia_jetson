/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/bug.h>
#include <nvgpu/worker.h>
#include <nvgpu/string.h>

static void nvgpu_worker_pre_process(struct nvgpu_worker *worker)
{
	if (worker->ops->pre_process != NULL) {
		worker->ops->pre_process(worker);
	}
}

static bool nvgpu_worker_wakeup_condition(struct nvgpu_worker *worker)
{
	bool ret = false;

	if (worker->ops->wakeup_condition != NULL) {
		ret = worker->ops->wakeup_condition(worker);
	}
	return ret;
}

static u32 nvgpu_worker_wakeup_timeout(
		struct nvgpu_worker *worker)
{
	u32 timeout = 0U;

	if (worker->ops->wakeup_timeout != NULL) {
		timeout = worker->ops->wakeup_timeout(
				worker);
	}
	return timeout;
}

static bool nvgpu_worker_wakeup_early_exit(struct nvgpu_worker *worker)
{
	bool ret = false;

	if (worker->ops->wakeup_early_exit != NULL) {
		ret = worker->ops->wakeup_early_exit(worker);
	}
	return ret;
}

static void nvgpu_worker_wakeup_process_item(struct nvgpu_worker *worker,
		struct nvgpu_list_node *work_item)
{
	nvgpu_assert(worker->ops->wakeup_process_item != NULL);

	worker->ops->wakeup_process_item(work_item);
}

static void nvgpu_worker_wakeup_post_process(
		struct nvgpu_worker *worker)
{
	if (worker->ops->wakeup_post_process != NULL) {
		worker->ops->wakeup_post_process(worker);
	}
}

/**
 * Tell the worker that potentially more work needs to be done.
 *
 * Increase the work counter to synchronize the worker with the new work. Wake
 * up the worker. If the worker was already running, it will handle this work
 * before going to sleep.
 */
static int nvgpu_worker_wakeup(struct nvgpu_worker *worker)
{
	int put;
	struct gk20a *g = worker->g;

	nvgpu_log_fn(g, " ");

	put = nvgpu_atomic_inc_return(&worker->put);
	nvgpu_cond_signal_interruptible(&worker->wq);

	return put;
}

static bool nvgpu_worker_pending(struct nvgpu_worker *worker, int get)
{
	bool pending = nvgpu_atomic_read(&worker->put) != get;

	/* We don't need barriers because they are implicit in locking */
	return pending;
}

/**
 * Process the queued works for the worker thread serially.
 *
 * Flush all the work items in the queue one by one. This may block timeout
 * handling for a short while, as these are serialized.
 */
static void nvgpu_worker_process(struct nvgpu_worker *worker, int *get)
{
	struct gk20a *g = worker->g;

	while (nvgpu_worker_pending(worker, *get)) {
		struct nvgpu_list_node *work_item = NULL;

		nvgpu_spinlock_acquire(&worker->items_lock);
		if (!nvgpu_list_empty(&worker->items)) {
			work_item = worker->items.next;
			nvgpu_list_del(work_item);
		}
		nvgpu_spinlock_release(&worker->items_lock);

		if (work_item == NULL) {
			/*
			 * Woke up for some other reason, but there are no
			 * other reasons than a work item added in the items
			 * list currently, so warn and ack the message.
			 */
			nvgpu_info(g, "Spurious worker event!");
			++*get;
			break;
		}

		nvgpu_worker_wakeup_process_item(worker, work_item);
		++*get;
	}
}

/*
 * Process all work items found in the work queue.
 */
static int nvgpu_worker_poll_work(void *arg)
{
	struct nvgpu_worker *worker = (struct nvgpu_worker *)arg;
	int get = 0;

	nvgpu_worker_pre_process(worker);

	while (!nvgpu_worker_should_stop(worker)) {
		int ret;

		ret = NVGPU_COND_WAIT_INTERRUPTIBLE(
				&worker->wq,
				nvgpu_worker_pending(worker, get) ||
				nvgpu_worker_wakeup_condition(worker) ||
				nvgpu_worker_should_stop(worker),
				nvgpu_worker_wakeup_timeout(worker));

		if (nvgpu_worker_wakeup_early_exit(worker)) {
			break;
		}

		if (ret == 0) {
			nvgpu_worker_process(worker, &get);
		}

		nvgpu_worker_wakeup_post_process(worker);
	}
	return 0;
}

static int nvgpu_worker_start(struct nvgpu_worker *worker)
{
	int err = 0;

	if (nvgpu_thread_is_running(&worker->poll_task)) {
		return err;
	}

	nvgpu_mutex_acquire(&worker->start_lock);

	/*
	 * Mutexes have implicit barriers, so there is no risk of a thread
	 * having a stale copy of the poll_task variable as the call to
	 * thread_is_running is volatile
	 */

	if (nvgpu_thread_is_running(&worker->poll_task)) {
		nvgpu_mutex_release(&worker->start_lock);
		return err;
	}

	err = nvgpu_thread_create(&worker->poll_task, worker,
			nvgpu_worker_poll_work, worker->thread_name);
	if (err != 0) {
		nvgpu_err(worker->g,
			  "failed to create worker poller thread %s err %d",
			  worker->thread_name, err);
	}

	nvgpu_mutex_release(&worker->start_lock);
	return err;
}

bool nvgpu_worker_should_stop(struct nvgpu_worker *worker)
{
	return nvgpu_thread_should_stop(&worker->poll_task);
}

int nvgpu_worker_enqueue(struct nvgpu_worker *worker,
		struct nvgpu_list_node *work_item)
{
	int err;
	struct gk20a *g = worker->g;

	/*
	 * Warn if worker thread cannot run
	 */
	err = nvgpu_worker_start(worker);
	if (err != 0) {
		nvgpu_do_assert_print(g, "nvgpu_worker %s cannot run!",
			worker->thread_name);
		return -1;
	}

	nvgpu_spinlock_acquire(&worker->items_lock);
	if (!nvgpu_list_empty(work_item)) {
		/*
		 * Already queued, so will get processed eventually.
		 * The worker is probably awake already.
		 */
		nvgpu_spinlock_release(&worker->items_lock);
		return -1;
	}
	nvgpu_list_add_tail(work_item, &worker->items);
	nvgpu_spinlock_release(&worker->items_lock);

	(void) nvgpu_worker_wakeup(worker);

	return 0;
}

void nvgpu_worker_init_name(struct nvgpu_worker *worker,
		const char* worker_name, const char *gpu_name)
{
	/*
	 * Maximum character size of worker thread name
	 * Note: 1 is subtracted to account for null character
	 */
	size_t worker_name_size = sizeof(worker->thread_name) - 1U;

	/* Number of characters that can be used for thread name */
	size_t num_free_chars = worker_name_size;

	/* Terminate thread name with NULL character */
	worker->thread_name[0] = '\0';

	(void) strncat(worker->thread_name, worker_name, num_free_chars);

	num_free_chars = worker_name_size - strlen(worker->thread_name);

	(void) strncat(worker->thread_name, "_", num_free_chars);

	num_free_chars = worker_name_size - strlen(worker->thread_name);

	(void) strncat(worker->thread_name, gpu_name, num_free_chars);
}

int nvgpu_worker_init(struct gk20a *g, struct nvgpu_worker *worker,
	const struct nvgpu_worker_ops *worker_ops)
{
	int err;

	worker->g = g;
	nvgpu_atomic_set(&worker->put, 0);
	(void) nvgpu_cond_init(&worker->wq);
	nvgpu_init_list_node(&worker->items);
	nvgpu_spinlock_init(&worker->items_lock);
	nvgpu_mutex_init(&worker->start_lock);

	worker->ops = worker_ops;

	err = nvgpu_worker_start(worker);
	if (err != 0) {
		nvgpu_err(g, "failed to start worker poller thread %s",
				worker->thread_name);
		return err;
	}
	return 0;
}

void nvgpu_worker_deinit(struct nvgpu_worker *worker)
{
	nvgpu_mutex_acquire(&worker->start_lock);
	nvgpu_thread_stop(&worker->poll_task);
	nvgpu_mutex_release(&worker->start_lock);
}
