/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <unit/unit.h>
#include <unit/io.h>
#include <nvgpu/posix/io.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/worker.h>
#include <nvgpu/thread.h>
#include <nvgpu/timers.h>
#include <nvgpu/atomic.h>
#include <nvgpu/posix/posix-fault-injection.h>

#include "worker.h"

/*
 * nvgpu_worker_ops functions
 */
static nvgpu_atomic_t pre_process_count;
static void pre_process(struct nvgpu_worker *worker)
{
	nvgpu_atomic_inc(&pre_process_count);
}

static bool force_early_exit = false;
static nvgpu_atomic_t wakeup_early_exit_count;
static bool wakeup_early_exit(struct nvgpu_worker *worker)
{
	nvgpu_atomic_inc(&wakeup_early_exit_count);
	if (force_early_exit) {
		return true;
	}

	return nvgpu_worker_should_stop(worker);
}

static nvgpu_atomic_t wakeup_post_process_val;
static void wakeup_post_process(struct nvgpu_worker *worker)
{
	nvgpu_atomic_inc(&wakeup_post_process_val);
}

static void wakeup_post_process_stop_thread(struct nvgpu_worker *worker)
{
	struct nvgpu_posix_fault_inj *thread_fi =
			nvgpu_thread_get_fault_injection();

	nvgpu_posix_enable_fault_injection(thread_fi, true, 0);
	nvgpu_atomic_inc(&wakeup_post_process_val);
}

static bool stall_processing = false;
static nvgpu_atomic_t item_count;
static void wakeup_process_item(struct nvgpu_list_node *work_item)
{
	bool stall = stall_processing;

	nvgpu_atomic_inc(&item_count);
	while (stall) {
		nvgpu_udelay(5);
		stall = stall_processing;
	}
}

static bool wakeup_condition_val = false;
static bool wakeup_condition(struct nvgpu_worker *worker)
{
	return wakeup_condition_val;
}

static u32 wakeup_timeout_val = 0U;
static u32 wakeup_timeout(struct nvgpu_worker *worker)
{
	return wakeup_timeout_val;
}

_Thread_local struct nvgpu_worker worker;
_Thread_local struct nvgpu_worker worker_branch;
_Thread_local struct nvgpu_worker_ops worker_ops = {
	/* pre_process is NULL for branch testing for NULL when thread starts. */
	.pre_process = NULL,
	.wakeup_early_exit = wakeup_early_exit,
	.wakeup_post_process = wakeup_post_process,
	.wakeup_process_item = wakeup_process_item,
	.wakeup_condition = wakeup_condition,
	.wakeup_timeout = wakeup_timeout,
};

int test_init(struct unit_module *m, struct gk20a *g, void *args)
{
	int err;
	struct nvgpu_posix_fault_inj *thread_fi =
			nvgpu_thread_get_fault_injection();
	char tmp[sizeof(worker.thread_name)+10];

	memset(tmp, 'g', sizeof(tmp) - 1);
	tmp[sizeof(tmp) - 1] = '\0';

	/* init with a long name to get branch coverage */
	nvgpu_worker_init_name(&worker, tmp,
		"A long-named simulated unit test gpu");

	/* init with a reasonable name */
	nvgpu_worker_init_name(&worker, "testworker", "gpu");

	/* enable fault injection to create error starting thread for worker */
	nvgpu_posix_enable_fault_injection(thread_fi, true, 0);
	err = nvgpu_worker_init(g, &worker, &worker_ops);
	unit_assert(err != 0, return UNIT_FAIL);
	nvgpu_posix_enable_fault_injection(thread_fi, false, 0);

	/* normal init */
	err = nvgpu_worker_init(g, &worker, &worker_ops);
	unit_assert(err == 0, return UNIT_FAIL);

	/* init when already running */
	while (!nvgpu_thread_is_running(&worker.poll_task)) {
		nvgpu_udelay(5);
	}

	nvgpu_atomic_set(&worker_branch.poll_task.running, 1);
	err = nvgpu_worker_init(g, &worker_branch, &worker_ops);
	unit_assert(err == 0, return UNIT_FAIL);

	return UNIT_SUCCESS;
}

int test_enqueue(struct unit_module *m, struct gk20a *g, void *args)
{
	int err;
	const unsigned int num_work_items = 3U;
	struct nvgpu_list_node work_items[num_work_items];
	unsigned int i;

	for (i = 0U; i < num_work_items; i++) {
		nvgpu_init_list_node(&work_items[i]);
	}
	nvgpu_atomic_set(&item_count, 0);

	for (i = 0U; i < num_work_items; i++) {
		err = nvgpu_worker_enqueue(&worker, &work_items[i]);
		unit_assert(err == 0, return UNIT_FAIL);
	}
	/* wait until all items are processed */
	while ((u32)nvgpu_atomic_read(&item_count) < num_work_items) {
		nvgpu_udelay(5);
	}

	/*
	 * Test requeueing same item. To do this, we have to stall the worker
	 * in the processing loop so we can make sure the item isn't removed.
	 */
	stall_processing = true;
	nvgpu_init_list_node(&work_items[0]);
	err = nvgpu_worker_enqueue(&worker, &work_items[0]);
	unit_assert(err == 0, return UNIT_FAIL);
	while ((u32)nvgpu_atomic_read(&item_count) < (num_work_items + 1)) {
		nvgpu_udelay(5);
	}
	err = nvgpu_worker_enqueue(&worker, &work_items[0]);
	unit_assert(err == 0, return UNIT_FAIL);
	err = nvgpu_worker_enqueue(&worker, &work_items[0]);
	unit_assert(err != 0, return UNIT_FAIL);
	stall_processing = false;
	while ((u32)nvgpu_atomic_read(&item_count) < (num_work_items + 2)) {
		nvgpu_udelay(5);
	}

	return UNIT_SUCCESS;
}

int test_branches(struct unit_module *m, struct gk20a *g, void *args)
{
	int err;
	struct nvgpu_list_node work_item;
	int last_item_count;
	struct nvgpu_posix_fault_inj *thread_fi =
			nvgpu_thread_get_fault_injection();
	struct nvgpu_posix_fault_inj *thread_running_fi =
			nvgpu_thread_running_true_get_fault_injection();
	unsigned int i;

	/*
	 * make timeout value short to get those branches, but have to
	 * call enqueue to make it trigger
	 */
	wakeup_timeout_val = 1U;
	nvgpu_atomic_set(&wakeup_post_process_val, 0);
	nvgpu_init_list_node(&work_item);
	err = nvgpu_worker_enqueue(&worker, &work_item);
	unit_assert(err == 0, return UNIT_FAIL);
	while (nvgpu_atomic_read(&wakeup_post_process_val) < 10) {
		nvgpu_udelay(5);
	}
	wakeup_timeout_val = 0U;

	/* cover branches where these ops are NULL */
	worker_ops.wakeup_condition = NULL;
	worker_ops.wakeup_timeout = NULL;
	worker_ops.wakeup_early_exit = NULL;
	worker_ops.wakeup_post_process = NULL;
	/* do this twice to make sure each is given a chance */
	for (i = 0U; i < 2; i++) {
		last_item_count = nvgpu_atomic_read(&item_count);
		err = nvgpu_worker_enqueue(&worker, &work_item);
		unit_assert(err == 0, return UNIT_FAIL);
		while (last_item_count == nvgpu_atomic_read(&item_count)) {
			nvgpu_udelay(5);
		}
	}
	worker_ops.wakeup_condition = wakeup_condition;
	worker_ops.wakeup_timeout = wakeup_timeout;
	worker_ops.wakeup_early_exit = wakeup_early_exit;
	worker_ops.wakeup_post_process = wakeup_post_process;

	/* cover branch for the wakeup_condition op */
	nvgpu_atomic_set(&wakeup_post_process_val, 0);
	wakeup_condition_val = true;
	last_item_count = nvgpu_atomic_read(&item_count);
	err = nvgpu_worker_enqueue(&worker, &work_item);
	unit_assert(err == 0, return UNIT_FAIL);
	while (nvgpu_atomic_read(&wakeup_post_process_val) < 1) {
		nvgpu_udelay(5);
	}
	wakeup_condition_val = false;

	/*
	 * Cover branches for failsafe checks for empty work. This shouldn't
	 * really happen, but there's logic to catch them just in case. So, we
	 * can't make it happen directly, so we send the cond directly.
	 */
	nvgpu_atomic_set(&wakeup_post_process_val, 0);
	nvgpu_atomic_inc(&worker.put);
	nvgpu_cond_signal_interruptible(&worker.wq);
	while (nvgpu_atomic_read(&wakeup_post_process_val) < 1) {
		nvgpu_udelay(5);
	}

	/* Cover branch for early exit. This will exit the thread. */
	nvgpu_atomic_set(&wakeup_early_exit_count, 0);
	force_early_exit = true;
	nvgpu_init_list_node(&work_item);
	err = nvgpu_worker_enqueue(&worker, &work_item);
	unit_assert(err == 0, return UNIT_FAIL);
	while (nvgpu_atomic_read(&wakeup_early_exit_count) < 1) {
		nvgpu_udelay(5);
	}
	force_early_exit = false;
	/* when the thread exists, we need sync some state */
	nvgpu_thread_stop(&worker.poll_task);

	/*
	 * While the thread is stopped, we can hit a branch in enqueue where
	 * starting the thread fails.
	 */
	nvgpu_init_list_node(&work_item);
	nvgpu_posix_enable_fault_injection(thread_fi, true, 0);
	if (!EXPECT_BUG(nvgpu_worker_enqueue(&worker, &work_item))) {
		unit_return_fail(m, "should have failed to enqueue\n");
	}
	nvgpu_posix_enable_fault_injection(thread_fi, false, 0);

	/*
	 * While the thread is stopped, we can hit a branch in the worker start
	 * function where the first check for thread running is false, then
	 * second check is true.
	 */
	nvgpu_init_list_node(&work_item);
	nvgpu_posix_enable_fault_injection(thread_running_fi, true, 1);
	err = nvgpu_worker_enqueue(&worker, &work_item);
	unit_assert(err == 0, return UNIT_FAIL);
	nvgpu_posix_enable_fault_injection(thread_running_fi, false, 0);

	/* Re-init the worker to start the thread for next test. */
	worker_ops.pre_process = pre_process;
	nvgpu_atomic_set(&pre_process_count, 0);
	nvgpu_worker_init(g, &worker, &worker_ops);
	unit_assert(err == 0, return UNIT_FAIL);
	/* make sure thread has started */
	while (nvgpu_atomic_read(&pre_process_count) < 1) {
		nvgpu_udelay(5);
	}

	/*
	 * Test for loop checking for thread_should_stop. The
	 * wakeup_post_process callback will enable the thread fault inject
	 * so nvgpu_thread_should_stop will return true.
	 * This will exit the thread.
	 */
	worker_ops.wakeup_post_process = wakeup_post_process_stop_thread;
	nvgpu_atomic_set(&wakeup_post_process_val, 0);
	nvgpu_init_list_node(&work_item);
	err = nvgpu_worker_enqueue(&worker, &work_item);
	unit_assert(err == 0, return UNIT_FAIL);
	while (nvgpu_atomic_read(&wakeup_post_process_val) < 1) {
		nvgpu_udelay(5);
	}
	/* there's no way to know the thread has exited, so wait a little */
	nvgpu_udelay(1000);
	worker_ops.wakeup_post_process = wakeup_post_process;
	nvgpu_posix_enable_fault_injection(thread_fi, false, 0);
	/* when the thread exists, we need sync some state */
	nvgpu_thread_stop(&worker.poll_task);

	/* Re-init the worker to start the thread for de-init testing. */
	worker_ops.pre_process = pre_process;
	nvgpu_atomic_set(&pre_process_count, 0);
	nvgpu_worker_init(g, &worker, &worker_ops);
	unit_assert(err == 0, return UNIT_FAIL);
	/* make sure thread has started */
	while (nvgpu_atomic_read(&pre_process_count) < 1) {
		nvgpu_udelay(5);
	}

	return UNIT_SUCCESS;
}

int test_deinit(struct unit_module *m, struct gk20a *g, void *args)
{
	nvgpu_worker_deinit(&worker);
	nvgpu_udelay(10);

	return UNIT_SUCCESS;
}


struct unit_module_test worker_tests[] = {
	UNIT_TEST(init,		test_init,				NULL, 0),
	UNIT_TEST(enqueue,	test_enqueue,				NULL, 1),
	UNIT_TEST(branches,	test_branches,				NULL, 0),
	UNIT_TEST(deinit,	test_deinit,				NULL, 0),
};

UNIT_MODULE(worker, worker_tests, UNIT_PRIO_NVGPU_TEST);
