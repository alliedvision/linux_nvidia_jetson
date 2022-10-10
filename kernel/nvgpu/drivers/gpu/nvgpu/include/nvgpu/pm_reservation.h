/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_PM_RESERVATION_H
#define NVGPU_PM_RESERVATION_H

#ifdef CONFIG_NVGPU_PROFILER

#include <nvgpu/list.h>
#include <nvgpu/lock.h>

struct gk20a;

enum nvgpu_profiler_pm_reservation_scope {
	NVGPU_PROFILER_PM_RESERVATION_SCOPE_DEVICE,
	NVGPU_PROFILER_PM_RESERVATION_SCOPE_CONTEXT,
};

enum nvgpu_profiler_pm_resource_type {
	NVGPU_PROFILER_PM_RESOURCE_TYPE_HWPM_LEGACY,
	NVGPU_PROFILER_PM_RESOURCE_TYPE_SMPC,
	NVGPU_PROFILER_PM_RESOURCE_TYPE_PMA_STREAM,
	NVGPU_PROFILER_PM_RESOURCE_TYPE_PC_SAMPLER,
	NVGPU_PROFILER_PM_RESOURCE_TYPE_COUNT,
};

struct nvgpu_pm_resource_reservation_entry {
	struct nvgpu_list_node entry;

	u32 reservation_id;
	u32 vmid;
	enum nvgpu_profiler_pm_reservation_scope scope;
};

static inline struct nvgpu_pm_resource_reservation_entry *
nvgpu_pm_resource_reservation_entry_from_entry(struct nvgpu_list_node *node)
{
	return (struct nvgpu_pm_resource_reservation_entry *)
		((uintptr_t)node - offsetof(struct nvgpu_pm_resource_reservation_entry, entry));
}

struct nvgpu_pm_resource_reservations {
	struct nvgpu_list_node head;
	u32 count;
	struct nvgpu_mutex lock;
};

int nvgpu_pm_reservation_init(struct gk20a *g);
void nvgpu_pm_reservation_deinit(struct gk20a *g);

int nvgpu_pm_reservation_acquire(struct gk20a *g, u32 reservation_id,
	enum nvgpu_profiler_pm_resource_type pm_resource,
	enum nvgpu_profiler_pm_reservation_scope scope,
	u32 vmid);
int nvgpu_pm_reservation_release(struct gk20a *g, u32 reservation_id,
	enum nvgpu_profiler_pm_resource_type pm_resource,
	u32 vmid);
void nvgpu_pm_reservation_release_all_per_vmid(struct gk20a *g, u32 vmid);

#endif /* CONFIG_NVGPU_PROFILER */
#endif /* NVGPU_PM_RESERVATION_H */
