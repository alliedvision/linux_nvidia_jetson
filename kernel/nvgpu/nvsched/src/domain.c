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
#include <nvs/domain.h>

/*
 * Create and add a new domain to the end of the domain list.
 */
struct nvs_domain *nvs_domain_create(struct nvs_sched *sched,
		  const char *name, u64 timeslice, u64 preempt_grace,
		  void *priv)
{
	struct nvs_domain_list *dlist = sched->domain_list;
	struct nvs_domain *dom = nvs_malloc(sched, sizeof(*dom));

	nvs_log(sched, "Creating domain - %s", name);

	if (dom == NULL) {
		return NULL;
	}

	nvs_memset(dom, 0, sizeof(*dom));

	strncpy(dom->name, name, sizeof(dom->name) - 1);
	dom->timeslice_ns     = timeslice;
	dom->preempt_grace_ns = preempt_grace;
	dom->priv             = priv;

	nvs_log_event(sched, NVS_EV_CREATE_DOMAIN, 0U);

	/*
	 * Now add the domain to the list of domains. If this is the first
	 * domain we are done. Otherwise use the last pointer to quickly
	 * append the domain.
	 */
	dlist->nr++;
	if (dlist->domains == NULL) {
		dlist->domains = dom;
		dlist->last    = dom;
		return dom;
	}

	dlist->last->next = dom;
	dlist->last       = dom;

	nvs_log(sched, "%s: Domain added", name);
	return dom;
}

/*
 * Unlink the domain from our list and clear the last pointer if this was the
 * only one remaining.
 */
static void nvs_domain_unlink(struct nvs_sched *sched,
			      struct nvs_domain *dom)
{
	struct nvs_domain_list *dlist = sched->domain_list;
	struct nvs_domain *tmp;


	if (dlist->domains == dom) {
		dlist->domains = dom->next;

		/*
		 * If dom == last and dom is the first entry, then we have a
		 * single entry in the list and we need to clear the last
		 * pointer as well.
		 */
		if (dom == dlist->last) {
			dlist->last = NULL;
		}
		return;
	}

	nvs_domain_for_each(sched, tmp) {
		/*
		 * If tmp's next pointer is dom, then we take tmp and have it
		 * skip over dom. But also don't forget to handle last; if dom
		 * is last, then last becomes the one before it.
		 */
		if (dom == tmp->next) {
			tmp->next = dom->next;
			if (dom == dlist->last) {
				dlist->last = tmp;
			}
			return;
		}
	}
}

void nvs_domain_destroy(struct nvs_sched *sched,
			struct nvs_domain *dom)
{
	nvs_log_event(sched, NVS_EV_REMOVE_DOMAIN, 0);

	nvs_domain_unlink(sched, dom);

	nvs_memset(dom, 0, sizeof(*dom));
	nvs_free(sched, dom);

	sched->domain_list->nr--;
}

void nvs_domain_clear_all(struct nvs_sched *sched)
{
	struct nvs_domain_list *dlist = sched->domain_list;

	while (dlist->domains != NULL) {
		nvs_domain_destroy(sched, dlist->domains);
	}
}

u32 nvs_domain_count(struct nvs_sched *sched)
{
	return sched->domain_list->nr;
}

struct nvs_domain *nvs_domain_by_name(struct nvs_sched *sched, const char *name)
{
	struct nvs_domain *domain;

	nvs_domain_for_each(sched, domain) {
		if (strcmp(domain->name, name) == 0) {
			return domain;
		}
	}

	return NULL;
}
