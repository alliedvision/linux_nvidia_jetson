/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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
		int err = nvgpu_nvs_add_domain(g, "(default)", 1000*1000, 10*1000, &domain);
		if (err != 0) {
			return err;
		}
	}

	return 0;
}

int nvgpu_nvs_open(struct gk20a *g)
{
	int err = 0;

	nvs_dbg(g, "Opening NVS node.");

	nvgpu_mutex_acquire(&g->sched_mutex);

	/*
	 * If there's already a scheduler present, we are done; no need for
	 * further action.
	 */
	if (g->scheduler != NULL) {
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

	nvs_dbg(g, "  Creating scheduler.");
	err = nvs_sched_create(g->scheduler->sched, &nvgpu_nvs_ops, g);

unlock:
	nvgpu_mutex_release(&g->sched_mutex);

	if (err) {
		nvs_dbg(g, "  Failed! Error code: %d", err);
		if (g->scheduler) {
			nvgpu_kfree(g, g->scheduler->sched);
			nvgpu_kfree(g, g->scheduler);
			g->scheduler = NULL;
		}
	}

	return err;
}

/*
 * A trivial allocator for now.
 */
static u64 nvgpu_nvs_new_id(struct gk20a *g)
{
	return nvgpu_atomic64_inc_return(&g->scheduler->id_counter);
}

int nvgpu_nvs_add_domain(struct gk20a *g, const char *name, u32 timeslice,
			 u32 preempt_grace, struct nvgpu_nvs_domain **pdomain)
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
		nvgpu_kfree(g, nvgpu_dom);
		err = -ENOMEM;
		goto unlock;
	}

	err = nvgpu_rl_domain_alloc(g, name);
	if (err != 0) {
		nvs_domain_destroy(g->scheduler->sched, nvs_dom);
		nvgpu_kfree(g, nvgpu_dom);
		return err;
	}


	nvgpu_dom->parent = nvs_dom;

	*pdomain = nvgpu_dom;
unlock:
	nvgpu_mutex_release(&g->sched_mutex);
	return err;
}

struct nvgpu_nvs_domain *
nvgpu_nvs_get_dom_by_id(struct gk20a *g, struct nvs_sched *sched, u64 dom_id)
{
	struct nvs_domain *nvs_dom;

	nvs_domain_for_each(sched, nvs_dom) {
		struct nvgpu_nvs_domain *nvgpu_dom = nvs_dom->priv;

		if (nvgpu_dom->id == dom_id) {
			return nvgpu_dom;
		}
	}

	return NULL;
}

struct nvgpu_nvs_domain *
nvgpu_nvs_domain_get(struct gk20a *g, const char *name)
{
	struct nvs_domain *nvs_dom;
	struct nvgpu_nvs_domain *dom = NULL;
	struct nvgpu_nvs_scheduler *sched = g->scheduler;

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

void nvgpu_nvs_domain_put(struct gk20a *g, struct nvgpu_nvs_domain *dom)
{
	nvgpu_mutex_acquire(&g->sched_mutex);
	dom->ref--;
	WARN_ON(dom->ref == 0U);
	nvgpu_mutex_release(&g->sched_mutex);
}

int nvgpu_nvs_del_domain(struct gk20a *g, u64 dom_id)
{
	struct nvgpu_nvs_domain *nvgpu_dom;
	struct nvs_domain *nvs_dom;
	int err = 0;

	nvgpu_mutex_acquire(&g->sched_mutex);

	nvs_dbg(g, "Attempting to remove domain: %llu", dom_id);

	nvgpu_dom = nvgpu_nvs_get_dom_by_id(g, g->scheduler->sched, dom_id);
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

	nvs_domain_destroy(g->scheduler->sched, nvs_dom);
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
	nvs_dbg(g, "  timeslice:     %u us", nvs_dom->timeslice_us);
	nvs_dbg(g, "  preempt grace: %u us", nvs_dom->preempt_grace_us);
	nvs_dbg(g, "  domain ID:     %llu", domain->id);
}
