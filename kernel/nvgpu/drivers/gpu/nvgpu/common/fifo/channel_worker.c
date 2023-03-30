/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include "channel_worker.h"
#include "channel_wdt.h"

#include <nvgpu/worker.h>
#include <nvgpu/channel.h>

static inline struct nvgpu_channel *
nvgpu_channel_from_worker_item(struct nvgpu_list_node *node)
{
	return (struct nvgpu_channel *)
	   ((uintptr_t)node - offsetof(struct nvgpu_channel, worker_item));
};

static void nvgpu_channel_worker_poll_wakeup_process_item(
		struct nvgpu_list_node *work_item)
{
	struct nvgpu_channel *ch = nvgpu_channel_from_worker_item(work_item);

	nvgpu_assert(ch != NULL);

	nvgpu_log_fn(ch->g, " ");

	nvgpu_channel_clean_up_jobs(ch);

	/* ref taken when enqueued */
	nvgpu_channel_put(ch);
}

static const struct nvgpu_worker_ops channel_worker_ops = {
#ifdef CONFIG_NVGPU_CHANNEL_WDT
	.pre_process = nvgpu_channel_worker_poll_init,
	.wakeup_post_process =
		nvgpu_channel_worker_poll_wakeup_post_process_item,
	.wakeup_timeout =
		nvgpu_channel_worker_poll_wakeup_condition_get_timeout,
#endif
	.wakeup_early_exit = NULL,
	.wakeup_process_item =
		nvgpu_channel_worker_poll_wakeup_process_item,
	.wakeup_condition = NULL,
};

/**
 * Initialize the channel worker's metadata and start the background thread.
 */
int nvgpu_channel_worker_init(struct gk20a *g)
{
	struct nvgpu_worker *worker = &g->channel_worker.worker;

	nvgpu_worker_init_name(worker, "nvgpu_channel_poll", g->name);

	return nvgpu_worker_init(g, worker, &channel_worker_ops);
}

void nvgpu_channel_worker_deinit(struct gk20a *g)
{
	struct nvgpu_worker *worker = &g->channel_worker.worker;

	nvgpu_worker_deinit(worker);
}

/**
 * Append a channel to the worker's list, if not there already.
 *
 * The worker thread processes work items (channels in its work list) and polls
 * for other things. This adds @ch to the end of the list and wakes the worker
 * up immediately. If the channel already existed in the list, it's not added,
 * because in that case it has been scheduled already but has not yet been
 * processed.
 */
void nvgpu_channel_worker_enqueue(struct nvgpu_channel *ch)
{
	struct gk20a *g = ch->g;
	int ret;

	nvgpu_log_fn(g, " ");

	/*
	 * Ref released when this item gets processed. The caller should hold
	 * one ref already, so normally shouldn't fail, but the channel could
	 * end up being freed between the time the caller got its reference and
	 * the time we end up here (e.g., if the client got killed); if so, just
	 * return.
	 */
	if (nvgpu_channel_get(ch) == NULL) {
		nvgpu_info(g, "cannot get ch ref for worker!");
		return;
	}

	ret = nvgpu_worker_enqueue(&g->channel_worker.worker,
			&ch->worker_item);
	if (ret != 0) {
		nvgpu_channel_put(ch);
		return;
	}
}
