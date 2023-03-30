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

#ifndef NVGPU_PROFILER_H
#define NVGPU_PROFILER_H

#ifdef CONFIG_NVGPU_PROFILER

#include <nvgpu/list.h>
#include <nvgpu/lock.h>
#include <nvgpu/pm_reservation.h>
#include <nvgpu/regops_allowlist.h>

struct gk20a;
struct nvgpu_channel;
struct nvgpu_tsg;
struct nvgpu_pm_resource_register_range_map;
enum nvgpu_pm_resource_hwpm_register_type;

struct nvgpu_profiler_object {
	struct gk20a *g;

	/*
	 * Debug session id. Only valid for profiler objects
	 * allocated through debug session i.e. in ioctl_dbg.c.
	 */
	int session_id;

	/* Unique profiler object handle. Also used as reservation id. */
	u32 prof_handle;

	/*
	 * Pointer to context being profiled, applicable only for profiler
	 * objects with context scope.
	 */
	struct nvgpu_tsg *tsg;

	/*
	 * If context has been bound by userspace.
	 * For objects with device scope, userspace should still trigger
	 * BIND_CONTEXT IOCTL/DEVCTL with tsg_fd = -1 for consistency.
	 */
	bool context_init;

	/* Lock to serialize IOCTL/DEVCTL calls */
	struct nvgpu_mutex ioctl_lock;

	/* If profiler object has reservation for each resource. */
	bool reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_COUNT];

	/* If context switch is enabled for each resource */
	bool ctxsw[NVGPU_PROFILER_PM_RESOURCE_TYPE_COUNT];

	/* Scope of the profiler object */
	enum nvgpu_profiler_pm_reservation_scope scope;

	/*
	 * Entry of this object into global list of objects
	 * maintained in struct gk20a.
	 */
	struct nvgpu_list_node prof_obj_entry;

	/*
	 * If PM resources are bound to this profiler object.
	 * Profiler object cannot get into runtime i.e. cannot execute RegOps
	 * until this flag is set.
	 */
	bool bound;

	/*
	 * GPU VA of the PMA stream buffer (if PMA stream resource is reserved
	 * successfully) associated with this profiler object.
	 */
	u64 pma_buffer_va;

	/*
	 * Size of the PMA stream buffer (if PMA stream resource is reserved
	 * successfully) associated with this profiler object.
	 */
	u32 pma_buffer_size;

	/*
	 * GPU VA of the buffer that would store available bytes in PMA buffer
	 * (if PMA stream resource is reserved successfully).
	 */
	u64 pma_bytes_available_buffer_va;

	/*
	 * CPU VA of the buffer that would store available bytes in PMA buffer
	 * (if PMA stream resource is reserved successfully).
	 */
	void *pma_bytes_available_buffer_cpuva;

	/*
	 * Dynamic map of HWPM register ranges that can be accessed
	 * through regops.
	 */
	struct nvgpu_pm_resource_register_range_map *map;

	/* Number of range entries in map above */
	u32 map_count;

	/* NVGPU_DBG_REG_OP_TYPE_* for each HWPM resource */
	u32 reg_op_type[NVGPU_HWPM_REGISTER_TYPE_COUNT];

	/** GPU instance Id */
	u32 gpu_instance_id;
};

static inline struct nvgpu_profiler_object *
nvgpu_profiler_object_from_prof_obj_entry(struct nvgpu_list_node *node)
{
	return (struct nvgpu_profiler_object *)
	((uintptr_t)node - offsetof(struct nvgpu_profiler_object, prof_obj_entry));
};

int nvgpu_profiler_alloc(struct gk20a *g,
	struct nvgpu_profiler_object **_prof,
	enum nvgpu_profiler_pm_reservation_scope scope, u32 gpu_instance_id);
void nvgpu_profiler_free(struct nvgpu_profiler_object *prof);

int nvgpu_profiler_bind_context(struct nvgpu_profiler_object *prof,
	struct nvgpu_tsg *tsg);
int nvgpu_profiler_unbind_context(struct nvgpu_profiler_object *prof);

int nvgpu_profiler_pm_resource_reserve(struct nvgpu_profiler_object *prof,
	enum nvgpu_profiler_pm_resource_type pm_resource);
int nvgpu_profiler_pm_resource_release(struct nvgpu_profiler_object *prof,
	enum nvgpu_profiler_pm_resource_type pm_resource);

int nvgpu_profiler_bind_hwpm(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg);
int nvgpu_profiler_unbind_hwpm(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg);
int nvgpu_profiler_bind_hwpm_streamout(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg,
		u64 pma_buffer_va,
		u32 pma_buffer_size,
		u64 pma_bytes_available_buffer_va);
int nvgpu_profiler_unbind_hwpm_streamout(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg,
		void *pma_bytes_available_buffer_cpuva,
		bool smpc_reserved);
int nvgpu_profiler_bind_smpc(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg);
int nvgpu_profiler_unbind_smpc(struct gk20a *g, u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg);

int nvgpu_profiler_bind_pm_resources(struct nvgpu_profiler_object *prof);
int nvgpu_profiler_unbind_pm_resources(struct nvgpu_profiler_object *prof);

int nvgpu_profiler_alloc_pma_stream(struct nvgpu_profiler_object *prof);
void nvgpu_profiler_free_pma_stream(struct nvgpu_profiler_object *prof);

bool nvgpu_profiler_allowlist_range_search(struct gk20a *g,
		struct nvgpu_pm_resource_register_range_map *map,
		u32 map_count, u32 offset,
		struct nvgpu_pm_resource_register_range_map *entry);

bool nvgpu_profiler_validate_regops_allowlist(struct nvgpu_profiler_object *prof,
		u32 offset, enum nvgpu_pm_resource_hwpm_register_type type);

#ifdef CONFIG_NVGPU_NON_FUSA
void nvgpu_profiler_hs_stream_quiesce(struct gk20a *g);
#endif /* CONFIG_NVGPU_NON_FUSA */

#endif /* CONFIG_NVGPU_PROFILER */
#endif /* NVGPU_PROFILER_H */
