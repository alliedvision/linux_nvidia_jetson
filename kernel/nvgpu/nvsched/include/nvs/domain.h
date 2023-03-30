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

#ifndef NVS_DOMAIN_H
#define NVS_DOMAIN_H

#include <nvs/types-internal.h>

struct nvs_sched;
struct nvs_domain;

/*
 * nvsched provides a simple, singly linked list for keeping track of
 * available domains. If algorithms need something more complex, like a
 * table of priorities and domains therein, then it will need to build
 * these data structures during its init().
 */
struct nvs_domain_list {
	u32			 nr;
	struct nvs_domain	*domains;

	/*
	 * Convenience for adding a domain quickly.
	 */
	struct nvs_domain	*last;
};

struct nvs_domain {
	char			 name[32];

	struct nvs_context_list	*ctx_list;

	/*
	 * Internal, singly linked list pointer.
	 */
	struct nvs_domain	*next;

	/*
	 * Scheduling parameters: specify how long this domain should be scheduled
	 * for and what the grace period the scheduler should give this domain when
	 * preempting. A value of zero is treated as an infinite timeslice or an
	 * infinite grace period.
	 */
	u64			 timeslice_ns;
	u64			 preempt_grace_ns;

	/*
	 * Priv pointer for downstream use.
	 */
	void			*priv;
};

/**
 * @brief Iterate over the list of domains present in the sched.
 */
#define nvs_domain_for_each(sched, domain_ptr)			\
	for ((domain_ptr) = (sched)->domain_list->domains;	\
	     (domain_ptr) != NULL;				\
	     (domain_ptr) = (domain_ptr)->next)

struct nvs_domain *nvs_domain_create(struct nvs_sched *sched,
		  const char *name, u64 timeslice, u64 preempt_grace,
		  void *priv);
void nvs_domain_destroy(struct nvs_sched *sched, struct nvs_domain *dom);
void nvs_domain_clear_all(struct nvs_sched *sched);
u32 nvs_domain_count(struct nvs_sched *sched);
struct nvs_domain *nvs_domain_by_name(struct nvs_sched *sched, const char *name);

#endif
