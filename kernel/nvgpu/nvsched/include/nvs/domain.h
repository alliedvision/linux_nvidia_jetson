/*
 * Copyright (c) 2021 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
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
	u32			 timeslice_us;
	u32			 preempt_grace_us;

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
		  const char *name, u32 timeslice, u32 preempt_grace,
		  void *priv);
void nvs_domain_destroy(struct nvs_sched *sched, struct nvs_domain *dom);
void nvs_domain_clear_all(struct nvs_sched *sched);
u32 nvs_domain_count(struct nvs_sched *sched);
struct nvs_domain *nvs_domain_by_name(struct nvs_sched *sched, const char *name);

#endif
