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

#include <nvgpu/gk20a.h>
#include <nvgpu/pm_reservation.h>
#include <nvgpu/log.h>
#include <nvgpu/mc.h>
#include <nvgpu/power_features/cg.h>
#include <nvgpu/kmem.h>
#include <nvgpu/lock.h>

static void prepare_resource_reservation(struct gk20a *g,
		enum nvgpu_profiler_pm_resource_type pm_resource, bool acquire)
{
	int err;

	if ((pm_resource != NVGPU_PROFILER_PM_RESOURCE_TYPE_HWPM_LEGACY) &&
	    (pm_resource != NVGPU_PROFILER_PM_RESOURCE_TYPE_PMA_STREAM)) {
		return;
	}

	if (acquire) {
		nvgpu_atomic_inc(&g->hwpm_refcount);
		nvgpu_log(g, gpu_dbg_prof, "HWPM refcount acquired %u, resource %u",
			nvgpu_atomic_read(&g->hwpm_refcount), pm_resource);

		if (nvgpu_atomic_read(&g->hwpm_refcount) == 1) {
			nvgpu_log(g, gpu_dbg_prof,
				"Trigger HWPM system reset, disable perf SLCG");
			err = nvgpu_mc_reset_units(g, NVGPU_UNIT_PERFMON);
			if (err != 0) {
				nvgpu_err(g, "Failed to reset PERFMON unit");
			}
			nvgpu_cg_slcg_perf_load_enable(g, false);
#ifdef CONFIG_NVGPU_NON_FUSA
			/*
			 * By default, disable the PMASYS legacy mode for
			 * NVGPU_NEXT.
			 */
			if (g->ops.perf.enable_pmasys_legacy_mode != NULL) {
				g->ops.perf.enable_pmasys_legacy_mode(g, false);
			}
#endif
		}
	} else {
		nvgpu_atomic_dec(&g->hwpm_refcount);
		nvgpu_log(g, gpu_dbg_prof, "HWPM refcount released %u, resource %u",
			nvgpu_atomic_read(&g->hwpm_refcount), pm_resource);

		if (nvgpu_atomic_read(&g->hwpm_refcount) == 0) {
			nvgpu_log(g, gpu_dbg_prof,
				"Trigger HWPM system reset, re-enable perf SLCG");
			err = nvgpu_mc_reset_units(g, NVGPU_UNIT_PERFMON);
			if (err != 0) {
				nvgpu_err(g, "Failed to reset PERFMON unit");
			}
			nvgpu_cg_slcg_perf_load_enable(g, true);
		}
	}
}

static bool check_pm_resource_existing_reservation_locked(
		struct nvgpu_pm_resource_reservations *reservations,
		u32 reservation_id, u32 vmid)
{
	struct nvgpu_pm_resource_reservation_entry *reservation_entry;
	bool reserved = false;

	nvgpu_list_for_each_entry(reservation_entry,
			&reservations->head,
			nvgpu_pm_resource_reservation_entry,
			entry) {
		if ((reservation_entry->reservation_id == reservation_id) &&
		    (reservation_entry->vmid == vmid )) {
			reserved = true;
			break;
		}
	}

	return reserved;
}

static bool check_pm_resource_reservation_allowed_locked(
		struct nvgpu_pm_resource_reservations *reservations,
		enum nvgpu_profiler_pm_reservation_scope scope,
		u32 reservation_id, u32 vmid)
{
	struct nvgpu_pm_resource_reservation_entry *reservation_entry;
	bool allowed = false;

	switch (scope) {
	case NVGPU_PROFILER_PM_RESERVATION_SCOPE_DEVICE:
		/*
		 * Reservation of SCOPE_DEVICE is allowed only if there is
		 * no current reservation of any scope by any profiler object.
		 */
		if (reservations->count == 0U) {
			allowed = true;
		}
		break;

	case NVGPU_PROFILER_PM_RESERVATION_SCOPE_CONTEXT:
		/*
		 * Reservation of SCOPE_CONTEXT is allowed only if -
		 * 1. There is no current SCOPE_DEVICE reservation by any other profiler
		 *    object.
		 * 2. Requesting profiler object does not already have the reservation.
		 */

		if (!nvgpu_list_empty(&reservations->head)) {
			reservation_entry = nvgpu_list_first_entry(
				&reservations->head,
				nvgpu_pm_resource_reservation_entry,
				entry);
			if (reservation_entry->scope ==
					NVGPU_PROFILER_PM_RESERVATION_SCOPE_DEVICE) {
				break;
			}
		}

		if (check_pm_resource_existing_reservation_locked(reservations,
				reservation_id, vmid)) {
			break;
		}

		allowed = true;
		break;
	}

	return allowed;
}

int nvgpu_pm_reservation_acquire(struct gk20a *g, u32 reservation_id,
		enum nvgpu_profiler_pm_resource_type pm_resource,
		enum nvgpu_profiler_pm_reservation_scope scope,
		u32 vmid)
{
	struct nvgpu_pm_resource_reservations *reservations =
		&g->pm_reservations[pm_resource];
	struct nvgpu_pm_resource_reservation_entry *reservation_entry;
	int err = 0;

	nvgpu_mutex_acquire(&reservations->lock);

	if (!check_pm_resource_reservation_allowed_locked(reservations, scope,
			reservation_id, vmid)) {
		err = -EBUSY;
		goto done;
	}

	reservation_entry = nvgpu_kzalloc(g, sizeof(*reservation_entry));
	if (reservation_entry == NULL) {
		err = -ENOMEM;
		goto done;
	}

	nvgpu_init_list_node(&reservation_entry->entry);

	reservation_entry->reservation_id = reservation_id;
	reservation_entry->scope = scope;
	reservation_entry->vmid = vmid;

	nvgpu_list_add(&reservation_entry->entry, &reservations->head);
	reservations->count++;

	prepare_resource_reservation(g, pm_resource, true);

done:
	nvgpu_mutex_release(&reservations->lock);

	return err;
}

int nvgpu_pm_reservation_release(struct gk20a *g, u32 reservation_id,
		enum nvgpu_profiler_pm_resource_type pm_resource,
		u32 vmid)
{
	struct nvgpu_pm_resource_reservations *reservations =
		&g->pm_reservations[pm_resource];
	struct nvgpu_pm_resource_reservation_entry *reservation_entry, *n;
	bool was_reserved = false;
	int err = 0;

	nvgpu_mutex_acquire(&reservations->lock);

	nvgpu_list_for_each_entry_safe(reservation_entry, n,
			&reservations->head,
			nvgpu_pm_resource_reservation_entry,
			entry) {
		if ((reservation_entry->reservation_id == reservation_id) &&
		    (reservation_entry->vmid == vmid)) {
			was_reserved = true;
			nvgpu_list_del(&reservation_entry->entry);
			reservations->count--;
			nvgpu_kfree(g, reservation_entry);
			break;
		}
	}

	if (was_reserved) {
		prepare_resource_reservation(g, pm_resource, false);
	} else {
		err = -EINVAL;
	}

	nvgpu_mutex_release(&reservations->lock);

	return err;
}

void nvgpu_pm_reservation_release_all_per_vmid(struct gk20a *g, u32 vmid)
{
	struct nvgpu_pm_resource_reservations *reservations;
	struct nvgpu_pm_resource_reservation_entry *reservation_entry, *n;
	int i;

	for (i = 0; i < NVGPU_PROFILER_PM_RESOURCE_TYPE_COUNT; i++) {
		reservations = &g->pm_reservations[i];

		nvgpu_mutex_acquire(&reservations->lock);
		nvgpu_list_for_each_entry_safe(reservation_entry, n,
				&reservations->head,
				nvgpu_pm_resource_reservation_entry,
				entry) {
			if (reservation_entry->vmid == vmid) {
				nvgpu_list_del(&reservation_entry->entry);
				reservations->count--;
				nvgpu_kfree(g, reservation_entry);
				prepare_resource_reservation(g,
					(enum nvgpu_profiler_pm_resource_type)i,
					false);
			}
		}
		nvgpu_mutex_release(&reservations->lock);
	}
}

int nvgpu_pm_reservation_init(struct gk20a *g)
{
	struct nvgpu_pm_resource_reservations *reservations;
	int i;

	nvgpu_log(g, gpu_dbg_prof, " ");

	if (g->pm_reservations) {
		return 0;
	}

	reservations = nvgpu_kzalloc(g, sizeof(*reservations) *
			NVGPU_PROFILER_PM_RESOURCE_TYPE_COUNT);
	if (reservations == NULL) {
		return -ENOMEM;
	}

	for (i = 0; i < NVGPU_PROFILER_PM_RESOURCE_TYPE_COUNT; i++) {
		nvgpu_init_list_node(&reservations[i].head);
		nvgpu_mutex_init(&reservations[i].lock);
	}

	g->pm_reservations = reservations;

	nvgpu_atomic_set(&g->hwpm_refcount, 0);

	nvgpu_log(g, gpu_dbg_prof, "initialized");

	return 0;
}

void nvgpu_pm_reservation_deinit(struct gk20a *g)
{
	nvgpu_kfree(g, g->pm_reservations);
}
