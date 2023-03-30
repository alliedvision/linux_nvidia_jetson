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

#ifndef NVGPU_NVS_H
#define NVGPU_NVS_H

#ifdef CONFIG_NVS_PRESENT
#include <nvs/domain.h>
#endif

#include <nvgpu/atomic.h>
#include <nvgpu/lock.h>
#include <nvgpu/worker.h>
#include <nvgpu/timers.h>

/*
 * Max size we'll parse from an NVS log entry.
 */
#define NVS_LOG_BUF_SIZE	128

struct gk20a;
struct nvgpu_nvs_domain_ioctl;

/*
 * NvGPU KMD domain implementation details for nvsched.
 */
struct nvgpu_nvs_domain {
	u64 id;

	/*
	 * Subscheduler ID to define the scheduling within a domain. These will
	 * be implemented by the kernel as needed. There'll always be at least
	 * one, which is the host HW built in round-robin scheduler.
	 */
	u32 subscheduler;

	/*
	 * Convenience pointer for linking back to the parent object.
	 */
	struct nvs_domain *parent;

	/*
	 * Domains are dynamically used by their participant TSGs and the
	 * runlist HW. A refcount prevents them from getting prematurely freed.
	 *
	 * This is not the usual refcount. The primary owner is userspace via the
	 * ioctl layer and a TSG putting a ref does not result in domain deletion.
	 */
	u32 ref;

	/*
	 * Userspace API on the device nodes.
	 */
	struct nvgpu_nvs_domain_ioctl *ioctl;
};

struct nvgpu_nvs_worker {
	struct nvgpu_worker worker;
	struct nvgpu_timeout timeout;
	u32 current_timeout;
};

struct nvgpu_nvs_scheduler {
	struct nvs_sched *sched;
	nvgpu_atomic64_t id_counter;
	struct nvgpu_nvs_worker worker;
	struct nvgpu_nvs_domain *active_domain;
};

#ifdef CONFIG_NVS_PRESENT
int nvgpu_nvs_init(struct gk20a *g);
int nvgpu_nvs_open(struct gk20a *g);
void nvgpu_nvs_remove_support(struct gk20a *g);
void nvgpu_nvs_get_log(struct gk20a *g, s64 *timestamp, const char **msg);
u32 nvgpu_nvs_domain_count(struct gk20a *g);
int nvgpu_nvs_del_domain(struct gk20a *g, u64 dom_id);
int nvgpu_nvs_add_domain(struct gk20a *g, const char *name, u64 timeslice,
			 u64 preempt_grace, struct nvgpu_nvs_domain **pdomain);
void nvgpu_nvs_print_domain(struct gk20a *g, struct nvgpu_nvs_domain *domain);

struct nvgpu_nvs_domain *
nvgpu_nvs_domain_by_id(struct gk20a *g, u64 domain_id);
struct nvgpu_nvs_domain *
nvgpu_nvs_domain_by_name(struct gk20a *g, const char *name);
void nvgpu_nvs_domain_get(struct gk20a *g, struct nvgpu_nvs_domain *dom);
void nvgpu_nvs_domain_put(struct gk20a *g, struct nvgpu_nvs_domain *dom);
const char *nvgpu_nvs_domain_get_name(struct nvgpu_nvs_domain *dom);
/*
 * Debug wrapper for NVS code.
 */
#define nvs_dbg(g, fmt, arg...)			\
	nvgpu_log(g, gpu_dbg_nvs, fmt, ##arg)

#else
static inline int nvgpu_nvs_init(struct gk20a *g)
{
	(void)g;
	return 0;
}

static inline void nvgpu_nvs_remove_support(struct gk20a *g)
{
	(void)g;
}

static inline struct nvgpu_nvs_domain *
nvgpu_nvs_domain_by_name(struct gk20a *g, const char *name)
{
	(void)g;
	(void)name;
	return NULL;
}

static inline void nvgpu_nvs_domain_put(struct gk20a *g, struct nvgpu_nvs_domain *dom)
{
	(void)g;
	(void)dom;
}

static inline const char *nvgpu_nvs_domain_get_name(struct nvgpu_nvs_domain *dom)
{
	(void)dom;
	return NULL;
}
#endif

#endif
