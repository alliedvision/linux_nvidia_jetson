/*
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/lock.h>
#include <nvgpu/list.h>
#include <nvgpu/posix/bug.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#ifndef _QNX_SOURCE
#include <execinfo.h>
#endif
#ifdef __NVGPU_UNIT_TEST__
#include <setjmp.h>
#include <signal.h>
#endif

#ifndef _QNX_SOURCE
#define BACKTRACE_MAXSIZE 1024
#endif

struct nvgpu_bug_desc {
#ifdef __NVGPU_UNIT_TEST__
	bool in_use;
	pthread_once_t once;
	struct nvgpu_spinlock lock;
	struct nvgpu_list_node head;
#endif
	void (*quiesce_cb)(void *arg);
	void *quiesce_arg;
};

static struct nvgpu_bug_desc bug_desc = {
#ifdef __NVGPU_UNIT_TEST__
	.once = PTHREAD_ONCE_INIT
#endif
};

#ifdef __NVGPU_UNIT_TEST__
void nvgpu_bug_cb_longjmp(void *arg)
{
	nvgpu_info(NULL, "Expected BUG detected!");

	jmp_buf *jmp_handler = arg;
	longjmp(*jmp_handler, 1);
}
#endif

static void nvgpu_posix_dump_stack(int skip_frames)
{
#ifndef _QNX_SOURCE
	void *trace[BACKTRACE_MAXSIZE];
	int i;
	int trace_size = backtrace(trace, BACKTRACE_MAXSIZE);
	char **trace_syms = backtrace_symbols(trace, trace_size);

	for (i = skip_frames; i < trace_size; i++) {
		nvgpu_err(NULL, "[%d] %s", i - skip_frames, trace_syms[i]);
	}

	free(trace_syms);
#else
	(void)skip_frames;
#endif
	return;
}

void dump_stack(void)
{
	const int frames = 2;
	/* Skip this function and nvgpu_posix_dump_stack() */
	nvgpu_posix_dump_stack(frames);
}

#ifdef __NVGPU_UNIT_TEST__
static void nvgpu_bug_init(void)
{
	nvgpu_spinlock_init(&bug_desc.lock);
	nvgpu_init_list_node(&bug_desc.head);
	bug_desc.in_use = true;
}
#endif

void nvgpu_bug_exit(void)
{
#ifndef __NVGPU_UNIT_TEST__
	int err;
	nvgpu_err(NULL, "SW quiesce done. Exiting.");
	while ((err = raise(SIGSEGV)) != 0) {
		/*
		 * Make sure that SIGSEGV signal is raised.
		 */
	}

	pthread_exit(NULL);
#endif
}

void nvgpu_bug_register_cb(struct nvgpu_bug_cb *cb)
{
#ifdef __NVGPU_UNIT_TEST__
	int err;
#endif

	if (cb->sw_quiesce_data) {
		bug_desc.quiesce_cb = cb->cb;
		bug_desc.quiesce_arg = cb->arg;
	} else {
#ifdef __NVGPU_UNIT_TEST__
		err = pthread_once(&bug_desc.once, nvgpu_bug_init);
		nvgpu_assert(err == 0);

		nvgpu_spinlock_acquire(&bug_desc.lock);
		nvgpu_list_add_tail(&cb->node, &bug_desc.head);
		nvgpu_spinlock_release(&bug_desc.lock);
#endif
	}
}

void nvgpu_bug_unregister_cb(struct nvgpu_bug_cb *cb)
{
	if (cb->sw_quiesce_data) {
		bug_desc.quiesce_cb = NULL;
		bug_desc.quiesce_arg = NULL;
	} else {
#ifdef __NVGPU_UNIT_TEST__
		if (bug_desc.in_use) {
			nvgpu_spinlock_acquire(&bug_desc.lock);
			nvgpu_list_del(&cb->node);
			nvgpu_spinlock_release(&bug_desc.lock);
		}
#endif
	}
}

/*
 * Ahhh! A bug!
 */
void nvgpu_posix_bug(const char *msg, int line_no)
{
	int err;
#ifdef __NVGPU_UNIT_TEST__
	struct nvgpu_bug_cb *cb;
#endif

	nvgpu_err(NULL, "%s:%d BUG detected!", msg, line_no);

#ifndef __NVGPU_UNIT_TEST__
	dump_stack();
#endif

	if (bug_desc.quiesce_cb != NULL) {
		bug_desc.quiesce_cb(bug_desc.quiesce_arg);
	}

#ifdef __NVGPU_UNIT_TEST__
	if (!bug_desc.in_use) {
		goto done;
	}

	nvgpu_spinlock_acquire(&bug_desc.lock);
	while (!nvgpu_list_empty(&bug_desc.head)) {
		/*
		 * Always process first entry, in -unlikely- where a
		 * callback would unregister another one.
		 */
		cb = nvgpu_list_first_entry(&bug_desc.head,
			nvgpu_bug_cb, node);
		/* Remove callback from list */
		nvgpu_list_del(&cb->node);
		/*
		 * Release spinlock before invoking callback.
		 * This allows callback to register/unregister other
		 * callbacks (unlikely).
		 * This allows using a longjmp in a callback
		 * for unit testing.
		 */
		nvgpu_spinlock_release(&bug_desc.lock);
		cb->cb(cb->arg);
		nvgpu_spinlock_acquire(&bug_desc.lock);
	}
	nvgpu_spinlock_release(&bug_desc.lock);

done:
#endif

#ifdef __NVGPU_UNIT_TEST__
	dump_stack();
#endif
	while ((err = raise(SIGSEGV)) != 0) {
		/*
		 * Make sure that SIGSEGV signal is raised.
		 */
	}

	pthread_exit(NULL);
}

bool nvgpu_posix_warn(const char *func, int line_no, bool cond, const char *fmt, ...)
{
	(void)fmt;
	if (!cond) {
		goto done;
	}

	/*
	 * MISRA-C rule 17.1 forbids stdarg.h (va_list etc) and this is shared
	 * with the safety build. Rule 21.6 forbids stdio.h so the warning
	 * cannot be formatted to a local buffer either.
	 */
	nvgpu_warn(NULL, "%s:%d WARNING detected %s", func, line_no, fmt);

	dump_stack();

done:
	return cond;
}
