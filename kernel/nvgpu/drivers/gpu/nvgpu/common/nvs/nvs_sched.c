/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvs/log.h>
#include <nvs/sched.h>

#include <nvgpu/nvs.h>
#include <nvgpu/kmem.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/runlist.h>

static struct nvs_sched_ops nvgpu_nvs_ops = {
	.preempt = NULL,
	.recover = NULL,
};

/*
 * TODO: make use of worker items when
 * 1) the active domain gets modified
 *    - currently updates happen asynchronously elsewhere
 *    - either resubmit the domain or do the updates later
 * 2) recovery gets triggered
 *    - currently it just locks all affected runlists
 *    - consider pausing the scheduler logic and signaling users
 */
struct nvgpu_nvs_worker_item {
	struct nvgpu_list_node list;
};

static inline struct nvgpu_nvs_worker_item *
nvgpu_nvs_worker_item_from_worker_item(struct nvgpu_list_node *node)
{
	return (struct nvgpu_nvs_worker_item *)
	   ((uintptr_t)node - offsetof(struct nvgpu_nvs_worker_item, list));
};

static inline struct nvgpu_nvs_worker *
nvgpu_nvs_worker_from_worker(struct nvgpu_worker *worker)
{
	return (struct nvgpu_nvs_worker *)
	   ((uintptr_t)worker - offsetof(struct nvgpu_nvs_worker, worker));
};

static void nvgpu_nvs_worker_poll_init(struct nvgpu_worker *worker)
{
	struct nvgpu_nvs_worker *nvs_worker =
		nvgpu_nvs_worker_from_worker(worker);

	/* 100 ms is a nice arbitrary timeout for default status */
	nvs_worker->current_timeout = 100;
	nvgpu_timeout_init_cpu_timer_sw(worker->g, &nvs_worker->timeout,
			nvs_worker->current_timeout);
}

static u32 nvgpu_nvs_worker_wakeup_timeout(struct nvgpu_worker *worker)
{
	struct nvgpu_nvs_worker *nvs_worker =
		nvgpu_nvs_worker_from_worker(worker);

	return nvs_worker->current_timeout;
}

static void nvgpu_nvs_worker_wakeup_process_item(
		struct nvgpu_list_node *work_item)
{
	struct nvgpu_nvs_worker_item *item =
		nvgpu_nvs_worker_item_from_worker_item(work_item);
	(void)item;
	/* placeholder; never called yet */
}

static u64 nvgpu_nvs_tick(struct gk20a *g)
{
	struct nvgpu_nvs_scheduler *sched = g->scheduler;
	struct nvgpu_nvs_domain *domain;
	struct nvs_domain *nvs_domain;
	u64 timeslice;

	nvs_dbg(g, "nvs tick");

	nvgpu_mutex_acquire(&g->sched_mutex);

	domain = sched->active_domain;

	if (domain == NULL) {
		/* nothing to schedule, TODO wait for an event instead */
		nvgpu_mutex_release(&g->sched_mutex);
		return 100 * NSEC_PER_MSEC;
	}

	nvs_domain = domain->parent->next;
	if (nvs_domain == NULL) {
		nvs_domain = g->scheduler->sched->domain_list->domains;
	}
	timeslice = nvs_domain->timeslice_ns;

	nvgpu_runlist_tick(g);
	sched->active_domain = nvs_domain->priv;

	nvgpu_mutex_release(&g->sched_mutex);

	return timeslice;
}

static void nvgpu_nvs_worker_wakeup_post_process(struct nvgpu_worker *worker)
{
	struct gk20a *g = worker->g;
	struct nvgpu_nvs_worker *nvs_worker =
		nvgpu_nvs_worker_from_worker(worker);

	if (nvgpu_timeout_peek_expired(&nvs_worker->timeout)) {
		u32 next_timeout_ns = nvgpu_nvs_tick(g);

		if (next_timeout_ns != 0U) {
			nvs_worker->current_timeout =
				(next_timeout_ns + NSEC_PER_MSEC - 1) / NSEC_PER_MSEC;
		}

		nvgpu_timeout_init_cpu_timer_sw(g, &nvs_worker->timeout,
				nvs_worker->current_timeout);
	}
}

static const struct nvgpu_worker_ops nvs_worker_ops = {
	.pre_process = nvgpu_nvs_worker_poll_init,
	.wakeup_timeout = nvgpu_nvs_worker_wakeup_timeout,
	.wakeup_process_item = nvgpu_nvs_worker_wakeup_process_item,
	.wakeup_post_process = nvgpu_nvs_worker_wakeup_post_process,
};

static int nvgpu_nvs_worker_init(struct gk20a *g)
{
	struct nvgpu_worker *worker = &g->scheduler->worker.worker;

	nvgpu_worker_init_name(worker, "nvgpu_nvs", g->name);

	return nvgpu_worker_init(g, worker, &nvs_worker_ops);
}

static void nvgpu_nvs_worker_deinit(struct gk20a *g)
{
	struct nvgpu_worker *worker = &g->scheduler->worker.worker;

	nvgpu_worker_deinit(worker);

	nvs_dbg(g, "NVS worker suspended");
}

int nvgpu_nvs_init(struct gk20a *g)
{
	struct nvgpu_nvs_domain *domain;
	int err;

	nvgpu_mutex_init(&g->sched_mutex);

	err = nvgpu_nvs_open(g);
	if (err != 0) {
		return err;
	}

	if (nvgpu_rl_domain_get(g, 0, "(default)") == NULL) {
		int err = nvgpu_nvs_add_domain(g, "(default)",
				100U * NSEC_PER_MSEC,
				0U,
				&domain);
		if (err != 0) {
			return err;
		}
	}

	return 0;
}

void nvgpu_nvs_remove_support(struct gk20a *g)
{
	struct nvgpu_nvs_scheduler *sched = g->scheduler;
	struct nvs_domain *nvs_dom;

	if (sched == NULL) {
		/* never powered on to init anything */
		return;
	}

	nvgpu_nvs_worker_deinit(g);

	nvs_domain_for_each(sched->sched, nvs_dom) {
		struct nvgpu_nvs_domain *nvgpu_dom = nvs_dom->priv;
		if (nvgpu_dom->ref != 1U) {
			nvgpu_warn(g,
				   "domain %llu is still in use during shutdown! refs: %u",
				   nvgpu_dom->id, nvgpu_dom->ref);
		}

		/* runlist removal will clear the rl domains */
		nvgpu_kfree(g, nvgpu_dom);
	}

	nvs_sched_close(sched->sched);
	nvgpu_kfree(g, sched->sched);
	nvgpu_kfree(g, sched);
	g->scheduler = NULL;
	nvgpu_mutex_destroy(&g->sched_mutex);
}

int nvgpu_nvs_open(struct gk20a *g)
{
	int err = 0;

	nvs_dbg(g, "Opening NVS node.");

	nvgpu_mutex_acquire(&g->sched_mutex);

	if (g->scheduler != NULL) {
		/* resuming from railgate */
		goto unlock;
	}

	g->scheduler = nvgpu_kzalloc(g, sizeof(*g->scheduler));
	if (g->scheduler == NULL) {
		err = -ENOMEM;
		goto unlock;
	}

	/* separately allocated to keep the definition hidden from other files */
	g->scheduler->sched = nvgpu_kzalloc(g, sizeof(*g->scheduler->sched));
	if (g->scheduler->sched == NULL) {
		err = -ENOMEM;
		goto unlock;
	}

	err = nvgpu_nvs_worker_init(g);
	if (err != 0) {
		goto unlock;
	}

	nvs_dbg(g, "  Creating NVS scheduler.");
	err = nvs_sched_create(g->scheduler->sched, &nvgpu_nvs_ops, g);
	if (err != 0) {
		nvgpu_nvs_worker_deinit(g);
		goto unlock;
	}

unlock:
	if (err) {
		nvs_dbg(g, "  Failed! Error code: %d", err);
		if (g->scheduler) {
			nvgpu_kfree(g, g->scheduler->sched);
			nvgpu_kfree(g, g->scheduler);
			g->scheduler = NULL;
		}
	}

	nvgpu_mutex_release(&g->sched_mutex);

	return err;
}

/*
 * A trivial allocator for now.
 */
static u64 nvgpu_nvs_new_id(struct gk20a *g)
{
	return nvgpu_atomic64_inc_return(&g->scheduler->id_counter);
}

int nvgpu_nvs_add_domain(struct gk20a *g, const char *name, u64 timeslice,
			 u64 preempt_grace, struct nvgpu_nvs_domain **pdomain)
{
	int err = 0;
	struct nvs_domain *nvs_dom;
	struct nvgpu_nvs_domain *nvgpu_dom;

	nvs_dbg(g, "Adding new domain: %s", name);

	nvgpu_mutex_acquire(&g->sched_mutex);

	if (nvs_domain_by_name(g->scheduler->sched, name) != NULL) {
		err = -EEXIST;
		goto unlock;
	}

	nvgpu_dom = nvgpu_kzalloc(g, sizeof(*nvgpu_dom));
	if (nvgpu_dom == NULL) {
		err = -ENOMEM;
		goto unlock;
	}

	nvgpu_dom->id = nvgpu_nvs_new_id(g);
	nvgpu_dom->ref = 1U;

	nvs_dom = nvs_domain_create(g->scheduler->sched, name,
				    timeslice, preempt_grace, nvgpu_dom);

	if (nvs_dom == NULL) {
		nvs_dbg(g, "failed to create nvs domain for %s", name);
		nvgpu_kfree(g, nvgpu_dom);
		err = -ENOMEM;
		goto unlock;
	}

	err = nvgpu_rl_domain_alloc(g, name);
	if (err != 0) {
		nvs_dbg(g, "failed to alloc rl domain for %s", name);
		nvs_domain_destroy(g->scheduler->sched, nvs_dom);
		nvgpu_kfree(g, nvgpu_dom);
		goto unlock;
	}


	nvgpu_dom->parent = nvs_dom;

	if (g->scheduler->active_domain == NULL) {
		g->scheduler->active_domain = nvgpu_dom;
	}

	*pdomain = nvgpu_dom;
unlock:
	nvgpu_mutex_release(&g->sched_mutex);
	return err;
}

struct nvgpu_nvs_domain *
nvgpu_nvs_domain_by_id_locked(struct gk20a *g, u64 domain_id)
{
	struct nvgpu_nvs_scheduler *sched = g->scheduler;
	struct nvs_domain *nvs_dom;

	nvgpu_log(g, gpu_dbg_nvs, "lookup %llu", domain_id);

	nvs_domain_for_each(sched->sched, nvs_dom) {
		struct nvgpu_nvs_domain *nvgpu_dom = nvs_dom->priv;

		if (nvgpu_dom->id == domain_id) {
			return nvgpu_dom;
		}
	}

	return NULL;
}

struct nvgpu_nvs_domain *
nvgpu_nvs_domain_by_id(struct gk20a *g, u64 domain_id)
{
	struct nvgpu_nvs_domain *dom = NULL;

	nvgpu_log(g, gpu_dbg_nvs, "lookup %llu", domain_id);

	nvgpu_mutex_acquire(&g->sched_mutex);

	dom = nvgpu_nvs_domain_by_id_locked(g, domain_id);
	if (dom == NULL) {
		goto unlock;
	}

	dom->ref++;

unlock:
	nvgpu_mutex_release(&g->sched_mutex);
	return dom;
}

struct nvgpu_nvs_domain *
nvgpu_nvs_domain_by_name(struct gk20a *g, const char *name)
{
	struct nvs_domain *nvs_dom;
	struct nvgpu_nvs_domain *dom = NULL;
	struct nvgpu_nvs_scheduler *sched = g->scheduler;

	nvgpu_log(g, gpu_dbg_nvs, "lookup %s", name);

	nvgpu_mutex_acquire(&g->sched_mutex);

	nvs_dom = nvs_domain_by_name(sched->sched, name);
	if (nvs_dom == NULL) {
		goto unlock;
	}

	dom = nvs_dom->priv;
	dom->ref++;

unlock:
	nvgpu_mutex_release(&g->sched_mutex);
	return dom;
}

void nvgpu_nvs_domain_get(struct gk20a *g, struct nvgpu_nvs_domain *dom)
{
	nvgpu_mutex_acquire(&g->sched_mutex);
	WARN_ON(dom->ref == 0U);
	dom->ref++;
	nvgpu_log(g, gpu_dbg_nvs, "domain %s: ref++ = %u",
			dom->parent->name, dom->ref);
	nvgpu_mutex_release(&g->sched_mutex);
}

void nvgpu_nvs_domain_put(struct gk20a *g, struct nvgpu_nvs_domain *dom)
{
	nvgpu_mutex_acquire(&g->sched_mutex);
	dom->ref--;
	WARN_ON(dom->ref == 0U);
	nvgpu_log(g, gpu_dbg_nvs, "domain %s: ref-- = %u",
			dom->parent->name, dom->ref);
	nvgpu_mutex_release(&g->sched_mutex);
}

int nvgpu_nvs_del_domain(struct gk20a *g, u64 dom_id)
{
	struct nvgpu_nvs_scheduler *s = g->scheduler;
	struct nvgpu_nvs_domain *nvgpu_dom;
	struct nvs_domain *nvs_dom, *nvs_next;
	int err = 0;

	nvgpu_mutex_acquire(&g->sched_mutex);

	nvs_dbg(g, "Attempting to remove domain: %llu", dom_id);

	nvgpu_dom = nvgpu_nvs_domain_by_id_locked(g, dom_id);
	if (nvgpu_dom == NULL) {
		nvs_dbg(g, "domain %llu does not exist!", dom_id);
		err = -ENOENT;
		goto unlock;
	}

	if (nvgpu_dom->ref != 1U) {
		nvs_dbg(g, "domain %llu is still in use! refs: %u",
				dom_id, nvgpu_dom->ref);
		err = -EBUSY;
		goto unlock;
	}

	nvs_dom = nvgpu_dom->parent;

	err = nvgpu_rl_domain_delete(g, nvs_dom->name);
	if (err != 0) {
		nvs_dbg(g, "failed to delete RL domains on %llu!", dom_id);
		/*
		 * The RL domains require the existence of at least one domain;
		 * this path inherits that logic until it's been made more
		 * flexible.
		 */
		goto unlock;
	}

	nvgpu_dom->ref = 0U;

	/* note: same wraparound logic as in RL domains to keep in sync */
	if (s->active_domain == nvgpu_dom) {
		nvs_next = nvs_dom->next;
		if (nvs_next == NULL) {
			nvs_next = s->sched->domain_list->domains;
		}
		s->active_domain = nvs_next->priv;
	}

	nvs_domain_destroy(s->sched, nvs_dom);
	nvgpu_kfree(g, nvgpu_dom);

unlock:
	nvgpu_mutex_release(&g->sched_mutex);
	return err;
}

u32 nvgpu_nvs_domain_count(struct gk20a *g)
{
	u32 count;

	nvgpu_mutex_acquire(&g->sched_mutex);
	count = nvs_domain_count(g->scheduler->sched);
	nvgpu_mutex_release(&g->sched_mutex);

	return count;
}

const char *nvgpu_nvs_domain_get_name(struct nvgpu_nvs_domain *dom)
{
	struct nvs_domain *nvs_dom = dom->parent;

	return nvs_dom->name;
}

void nvgpu_nvs_get_log(struct gk20a *g, s64 *timestamp, const char **msg)
{
	struct nvs_log_event ev;

	nvs_log_get(g->scheduler->sched, &ev);

	if (ev.event == NVS_EV_NO_EVENT) {
		*timestamp = 0;
		*msg = NULL;
		return;
	}

	*msg       = nvs_log_event_string(ev.event);
	*timestamp = ev.timestamp;
}

void nvgpu_nvs_print_domain(struct gk20a *g, struct nvgpu_nvs_domain *domain)
{
	struct nvs_domain *nvs_dom = domain->parent;

	nvs_dbg(g, "Domain %s", nvs_dom->name);
	nvs_dbg(g, "  timeslice:     %llu ns", nvs_dom->timeslice_ns);
	nvs_dbg(g, "  preempt grace: %llu ns", nvs_dom->preempt_grace_ns);
	nvs_dbg(g, "  domain ID:     %llu", domain->id);
}
