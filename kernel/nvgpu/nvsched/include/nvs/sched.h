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

#ifndef NVS_SCHED_H
#define NVS_SCHED_H

/**
 * @page NV Scheduler
 *
 * Overview
 * ========
 *
 * An nvs_sched object defines a _scheduler_, this is an object that contains
 * information about the domains and contexts to manage, and some operations
 * to interact with the underlying HW. The scheduler is split up into three
 * distinct parts:
 *
 * 1. The implementation operations that allow the scheduler to interact
 *    with a given piece of hardware. This serves as a hardware abstraction
 *    since the conceptual framework here is not tied to a specific piece of
 *    HW such as a GPU.
 * 2. Algorithm implementations that pick the next context to actually run.
 * 3. A core component that defines the data structures which define the
 *    domains/contexts. The core component is responsible for linking the
 *    scheduling algorithm outputs to hardware operations.
 *
 * Implementation Operations
 * =========================
 *
 * Each concrete implementation of nvsched must provide, at a minimum some
 * operations that allow the scheduling logic to interact with the managed
 * HW. The two primary operations are preemption and recovery.
 *
 * Algorithms
 * ==========
 *
 * nvsched splits the data structures from the algorithms. This allows
 * multiple algorithms to be supported: for example one implementation could
 * use a round-robin approach for picking next domains, but another may wish
 * to use a priority based approach.
 *
 * Core Scheduler
 * ==============
 *
 * The responsibility for the core scheduler is to provide data structures
 * that model a two level scheduling model: first there's domains and then
 * there's contexts within a domain. An implementation built on top of
 * nvsched will need to instantiate domains and contexts and then execute
 * some top level operations to trigger scheduling work.
 *
 * The data structure nesting looks like this:
 *
 *   struct nvs_sched
 *   +-------------------------+     +---------->+-----------+
 *   |                         |     |           | preempt() |
 *   | struct nvs_sched_ops    +-----+           | recover() |
 *   |                         |                 +-----------+
 *   | // List of:             |
 *   | struct nvs_domain       +---------------->+-----------------+
 *   |                         |                 | Domain 1        |
 *   | struct nvs_domain_algo  +-------+         |   Domain Params |
 *   |                         |       |         |   Context list  +-----+
 *   +-------------------------+       |         +-----------------+     |
 *                                     |         | Domain ...      |     |
 *      +-------------+                |         |   Domain Params |     |
 *      | Context 1   |<---------+     |         |   Context list  +---+ |
 *      +-------------+          |     |         +-----------------+   | |
 *      | Context 2   |<---------+     |         | Domain N        |   | |
 *      +-------------+          |     |         |   Domain Params |   | |
 *      | Context ... |<-----+   |     |         |   Context list  +-+ | |
 *      +-------------+      |   |     |         +-----------------+ | | |
 *      | Context ... |<-----+   |     |                             | | |
 *      +-------------+      |   |     +-------->+-----------------+ | | |
 *      | Context M   |<-+   |   |               | next_domain()   | | | |
 *      +-------------+  |   |   |               | schedule()      | | | |
 *                       |   |   |               | init()          | | | |
 *                       |   |   |               +-----------------+ | | |
 *                       +---|---|-----------------------------------+ | |
 *                           +---|-------------------------------------+ |
 *                               +---------------------------------------+
 */

#include <nvs/impl-internal.h>
#include <nvs/types-internal.h>

struct nvs_sched;
struct nvs_domain;
struct nvs_domain_algo;
struct nvs_domain_list;
struct nvs_log_buffer;

/**
 * @brief Base scheduling operations an implementation will need to provide
 *        to the scheduling core.
 */
struct nvs_sched_ops {
	/**
	 * @brief Preempt the running context on the device \a sched
	 *        is managing.
	 *
	 * @param sched		The scheduler.
	 */
	int	(*preempt)(struct nvs_sched *sched);

	/**
	 * @brief Recover the running context in \a sched.
	 */
	int	(*recover)(struct nvs_sched *sched);
};

/**
 * @brief Define a top level scheduler object.
 */
struct nvs_sched {
	/**
	 * Ops that let the scheduler interface with the underlying
	 * hardware.
	 */
	struct nvs_sched_ops	*ops;

	/**
	 * List of domains. Internally stored as a singly linked
	 * list.
	 *
	 * @sa struct nvs_domain_list
	 */
	struct nvs_domain_list	*domain_list;

	/**
	 * Algorithm instance; invoked after a schedule() call.
	 */
	struct nvs_domain_algo	*algorithm;

	/**
	 * Log buffer with log entries.
	 */
	struct nvs_log_buffer	*log;

	/**
	 * Implementation private data.
	 */
	void			*priv;
};

/**
 * @brief Create a scheduler and assign the \a ops and \a priv pointers.
 *
 * @param sched		Pointer to an uninitialized struct sched.
 * @param ops		Ops defining HW interactions.
 * @param priv		Private pointer for implementation use.
 *
 * Build a sched struct in the passed memory \a sched. This pointer should
 * have at least sizeof(struct nvs_sched) bytes. nvsched cannot do this allocation
 * since the nvs_malloc() function relies on the sched object (some APIs require
 * an API token of some sort which they may choose to embed in sched->priv.
 *
 * The ops struct should be in memory that will not be reclaimed until after the
 * \a sched memory is reclaimed.
 *
 * \a priv may be used by implementations where needed. The priv pointer contents
 * will never be touched by nvsched.
 */
int	nvs_sched_create(struct nvs_sched *sched,
			 struct nvs_sched_ops *ops, void *priv);

void	nvs_sched_close(struct nvs_sched *sched);

#endif
