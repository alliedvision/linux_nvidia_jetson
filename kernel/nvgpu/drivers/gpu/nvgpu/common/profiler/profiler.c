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
#include <nvgpu/static_analysis.h>
#include <nvgpu/profiler.h>
#include <nvgpu/atomic.h>
#include <nvgpu/log.h>
#include <nvgpu/lock.h>
#include <nvgpu/kmem.h>
#include <nvgpu/tsg.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/perfbuf.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/regops_allowlist.h>
#include <nvgpu/regops.h>
#include <nvgpu/sort.h>
#include <nvgpu/gr/gr_instances.h>
#include <nvgpu/grmgr.h>

static int nvgpu_profiler_build_regops_allowlist(struct nvgpu_profiler_object *prof);
static void nvgpu_profiler_destroy_regops_allowlist(struct nvgpu_profiler_object *prof);

static nvgpu_atomic_t unique_id = NVGPU_ATOMIC_INIT(0);
static int generate_unique_id(void)
{
	return nvgpu_atomic_add_return(1, &unique_id);
}

int nvgpu_profiler_alloc(struct gk20a *g,
	struct nvgpu_profiler_object **_prof,
	enum nvgpu_profiler_pm_reservation_scope scope,
	u32 gpu_instance_id)
{
	struct nvgpu_profiler_object *prof;
	*_prof = NULL;

	nvgpu_log(g, gpu_dbg_prof, " ");

	prof = nvgpu_kzalloc(g, sizeof(*prof));
	if (prof == NULL) {
		return -ENOMEM;
	}

	prof->prof_handle = (u32)generate_unique_id();
	prof->scope = scope;
	prof->gpu_instance_id = gpu_instance_id;
	prof->g = g;

	nvgpu_mutex_init(&prof->ioctl_lock);
	nvgpu_init_list_node(&prof->prof_obj_entry);
	nvgpu_list_add(&prof->prof_obj_entry, &g->profiler_objects);

	nvgpu_log(g, gpu_dbg_prof, "Allocated profiler handle %u",
		prof->prof_handle);

	*_prof = prof;
	return 0;
}

void nvgpu_profiler_free(struct nvgpu_profiler_object *prof)
{
	struct gk20a *g = prof->g;

	nvgpu_log(g, gpu_dbg_prof, "Free profiler handle %u",
		prof->prof_handle);

	nvgpu_profiler_unbind_context(prof);
	nvgpu_profiler_free_pma_stream(prof);

	nvgpu_list_del(&prof->prof_obj_entry);
	prof->gpu_instance_id = 0U;
	nvgpu_kfree(g, prof);
}

int nvgpu_profiler_bind_context(struct nvgpu_profiler_object *prof,
		struct nvgpu_tsg *tsg)
{
	struct gk20a *g = prof->g;

	nvgpu_log(g, gpu_dbg_prof, "Request to bind tsgid %u with profiler handle %u",
		tsg->tsgid, prof->prof_handle);

	if (tsg->prof != NULL) {
		nvgpu_err(g, "TSG %u is already bound", tsg->tsgid);
		return -EINVAL;
	}

	if (prof->tsg != NULL) {
		nvgpu_err(g, "Profiler object %u already bound!", prof->prof_handle);
		return -EINVAL;
	}

	prof->tsg = tsg;
	tsg->prof = prof;

	nvgpu_log(g, gpu_dbg_prof, "Bind tsgid %u with profiler handle %u successful",
		tsg->tsgid, prof->prof_handle);

	prof->context_init = true;
	return 0;
}

int nvgpu_profiler_unbind_context(struct nvgpu_profiler_object *prof)
{
	struct gk20a *g = prof->g;
	struct nvgpu_tsg *tsg = prof->tsg;
	int i;

	if (prof->bound) {
		nvgpu_warn(g, "Unbinding resources for handle %u",
			prof->prof_handle);
		nvgpu_profiler_unbind_pm_resources(prof);
	}

	for (i = 0; i < NVGPU_PROFILER_PM_RESOURCE_TYPE_COUNT; i++) {
		if (prof->reserved[i]) {
			nvgpu_warn(g, "Releasing reserved resource %u for handle %u",
				i, prof->prof_handle);
			nvgpu_profiler_pm_resource_release(prof,
				(enum nvgpu_profiler_pm_resource_type)i);
		}
	}

	if (!prof->context_init) {
		return -EINVAL;
	}

	if (tsg != NULL) {
		tsg->prof = NULL;
		prof->tsg = NULL;

		nvgpu_log(g, gpu_dbg_prof, "Unbind profiler handle %u and tsgid %u",
			prof->prof_handle, tsg->tsgid);
	}

	prof->context_init = false;
	return 0;
}

int nvgpu_profiler_pm_resource_reserve(struct nvgpu_profiler_object *prof,
	enum nvgpu_profiler_pm_resource_type pm_resource)
{
	struct gk20a *g = prof->g;
	enum nvgpu_profiler_pm_reservation_scope scope = prof->scope;
	u32 reservation_id = prof->prof_handle;
	int err;

	nvgpu_log(g, gpu_dbg_prof,
		"Request reservation for profiler handle %u, resource %u, scope %u",
		prof->prof_handle, pm_resource, prof->scope);

	if (prof->reserved[pm_resource]) {
		nvgpu_err(g, "Profiler handle %u already has the reservation",
			prof->prof_handle);
		return -EEXIST;
	}

	if (prof->bound) {
		nvgpu_err(g, "PM resources already bound with profiler handle"
			" %u, rejecting reserve request", prof->prof_handle);
		return -EEXIST;
	}

	err = g->ops.pm_reservation.acquire(g, reservation_id, pm_resource,
			scope, 0);
	if (err != 0) {
		nvgpu_err(g, "Profiler handle %u denied the reservation, err %d",
			prof->prof_handle, err);
		return err;
	}

	prof->reserved[pm_resource] = true;

	if (pm_resource == NVGPU_PROFILER_PM_RESOURCE_TYPE_SMPC) {
		if (prof->ctxsw[NVGPU_PROFILER_PM_RESOURCE_TYPE_SMPC]) {
			prof->reg_op_type[NVGPU_HWPM_REGISTER_TYPE_SMPC] =
				NVGPU_DBG_REG_OP_TYPE_GR_CTX;
		} else {
			prof->reg_op_type[NVGPU_HWPM_REGISTER_TYPE_SMPC] =
				NVGPU_DBG_REG_OP_TYPE_GLOBAL;
		}
	}

	if (pm_resource == NVGPU_PROFILER_PM_RESOURCE_TYPE_HWPM_LEGACY) {
		if (prof->ctxsw[NVGPU_PROFILER_PM_RESOURCE_TYPE_HWPM_LEGACY]) {
			prof->reg_op_type[NVGPU_HWPM_REGISTER_TYPE_HWPM_PERFMON] =
				NVGPU_DBG_REG_OP_TYPE_GR_CTX;
			prof->reg_op_type[NVGPU_HWPM_REGISTER_TYPE_HWPM_ROUTER] =
				NVGPU_DBG_REG_OP_TYPE_GR_CTX;
			prof->reg_op_type[NVGPU_HWPM_REGISTER_TYPE_HWPM_PMA_TRIGGER] =
				NVGPU_DBG_REG_OP_TYPE_GR_CTX;
			prof->reg_op_type[NVGPU_HWPM_REGISTER_TYPE_HWPM_PERFMUX] =
				NVGPU_DBG_REG_OP_TYPE_GR_CTX;
			prof->reg_op_type[NVGPU_HWPM_REGISTER_TYPE_CAU] =
				NVGPU_DBG_REG_OP_TYPE_GR_CTX;
		} else {
			prof->reg_op_type[NVGPU_HWPM_REGISTER_TYPE_HWPM_PERFMON] =
				NVGPU_DBG_REG_OP_TYPE_GLOBAL;
			prof->reg_op_type[NVGPU_HWPM_REGISTER_TYPE_HWPM_ROUTER] =
				NVGPU_DBG_REG_OP_TYPE_GLOBAL;
			prof->reg_op_type[NVGPU_HWPM_REGISTER_TYPE_HWPM_PMA_TRIGGER] =
				NVGPU_DBG_REG_OP_TYPE_GLOBAL;
			prof->reg_op_type[NVGPU_HWPM_REGISTER_TYPE_HWPM_PERFMUX] =
				NVGPU_DBG_REG_OP_TYPE_GLOBAL;
			prof->reg_op_type[NVGPU_HWPM_REGISTER_TYPE_CAU] =
				NVGPU_DBG_REG_OP_TYPE_GLOBAL;
		}
	}

	if (pm_resource == NVGPU_PROFILER_PM_RESOURCE_TYPE_PMA_STREAM) {
		prof->reg_op_type[NVGPU_HWPM_REGISTER_TYPE_HWPM_PMA_CHANNEL] =
			NVGPU_DBG_REG_OP_TYPE_GLOBAL;
	}

	if (pm_resource == NVGPU_PROFILER_PM_RESOURCE_TYPE_PC_SAMPLER) {
		prof->reg_op_type[NVGPU_HWPM_REGISTER_TYPE_PC_SAMPLER] =
			NVGPU_DBG_REG_OP_TYPE_GR_CTX;
	}

	nvgpu_log(g, gpu_dbg_prof,
		"Granted reservation for profiler handle %u, resource %u, scope %u",
		prof->prof_handle, pm_resource, prof->scope);

	return 0;
}

int nvgpu_profiler_pm_resource_release(struct nvgpu_profiler_object *prof,
	enum nvgpu_profiler_pm_resource_type pm_resource)
{
	struct gk20a *g = prof->g;
	u32 reservation_id = prof->prof_handle;
	int err;

	nvgpu_log(g, gpu_dbg_prof,
		"Release reservation for profiler handle %u, resource %u, scope %u",
		prof->prof_handle, pm_resource, prof->scope);

	if (!prof->reserved[pm_resource]) {
		nvgpu_log(g, gpu_dbg_prof,
			"Profiler handle %u resource is not reserved",
			prof->prof_handle);
		return -EINVAL;
	}

	if (prof->bound) {
		nvgpu_log(g, gpu_dbg_prof,
			"PM resources alredy bound with profiler handle %u,"
			" unbinding for reservation release",
			prof->prof_handle);
		err = nvgpu_profiler_unbind_pm_resources(prof);
		if (err != 0) {
			nvgpu_err(g, "Profiler handle %u failed to unbound, err %d",
				prof->prof_handle, err);
			return err;
		}
	}

	err = g->ops.pm_reservation.release(g, reservation_id, pm_resource, 0);
	if (err != 0) {
		nvgpu_err(g, "Profiler handle %u does not have valid reservation, err %d",
			prof->prof_handle, err);
		prof->reserved[pm_resource] = false;
		return err;
	}

	prof->reserved[pm_resource] = false;

	nvgpu_log(g, gpu_dbg_prof,
		"Released reservation for profiler handle %u, resource %u, scope %u",
		prof->prof_handle, pm_resource, prof->scope);

	return 0;
}

static bool nvgpu_profiler_is_context_resource(
			struct nvgpu_profiler_object *prof,
			enum nvgpu_profiler_pm_resource_type pm_resource)
{
	return (prof->scope != NVGPU_PROFILER_PM_RESERVATION_SCOPE_DEVICE) ||
		prof->ctxsw[pm_resource];
}

int nvgpu_profiler_bind_smpc(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg)
{
	int err = 0;

	if (!is_ctxsw) {
		if (nvgpu_is_enabled(g, NVGPU_SUPPORT_SMPC_GLOBAL_MODE)) {
			err = nvgpu_gr_exec_with_err_for_instance(g,
				gr_instance_id,
				g->ops.gr.update_smpc_global_mode(g, true));
		} else {
			err = -EINVAL;
		}
	} else {
		err = g->ops.gr.update_smpc_ctxsw_mode(g, tsg, true);
		if (err != 0) {
			goto done;
		}
		if (nvgpu_is_enabled(g, NVGPU_SUPPORT_SMPC_GLOBAL_MODE)) {
			err = nvgpu_gr_exec_with_err_for_instance(g,
				gr_instance_id,
				g->ops.gr.update_smpc_global_mode(g, false));
		}
	}

done:
	if (err != 0) {
		nvgpu_err(g, "nvgpu bind smpc failed, err=%d", err);
	}
	return err;
}

int nvgpu_profiler_unbind_smpc(struct gk20a *g, u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg)
{
	int err;

	if (!is_ctxsw) {
		if (nvgpu_is_enabled(g, NVGPU_SUPPORT_SMPC_GLOBAL_MODE)) {
			err = nvgpu_gr_exec_with_err_for_instance(g,
				gr_instance_id,
				g->ops.gr.update_smpc_global_mode(g, false));
		} else {
			err = -EINVAL;
		}
	} else {
		err = g->ops.gr.update_smpc_ctxsw_mode(g, tsg, false);
	}

	if (err != 0) {
		nvgpu_err(g, "nvgpu unbind smpc failed, err=%d", err);
	}
	return err;
}

static int nvgpu_profiler_bind_hwpm_common(struct gk20a *g, u32 gr_instance_id,
		bool is_ctxsw, struct nvgpu_tsg *tsg, bool streamout)
{
	int err = 0;
	u32 mode = streamout ? NVGPU_GR_CTX_HWPM_CTXSW_MODE_STREAM_OUT_CTXSW :
			       NVGPU_GR_CTX_HWPM_CTXSW_MODE_CTXSW;

	if (!is_ctxsw) {
		if (g->ops.gr.init_cau != NULL) {
			/*
			 * TODO: Currently only one profiler object should be
			 * allowed. Reset CAU is using whole GR space for both
			 * MIG and legacy mode. Need to convert broadcast
			 * address to GR specific unicast programming when
			 * NvGpu supports more than one profiler object
			 * at a time.
			 */
			nvgpu_gr_exec_for_all_instances(g,
				g->ops.gr.init_cau(g));
		}
		if (g->ops.perf.reset_hwpm_pmm_registers != NULL) {
			g->ops.perf.reset_hwpm_pmm_registers(g);
		}
		g->ops.perf.init_hwpm_pmm_register(g);
	} else {
		err = g->ops.gr.update_hwpm_ctxsw_mode(
				g, gr_instance_id, tsg, mode);
	}

	return err;
}

int nvgpu_profiler_bind_hwpm(struct gk20a *g, u32 gr_instance_id,
		bool is_ctxsw, struct nvgpu_tsg *tsg)
{
	return nvgpu_profiler_bind_hwpm_common(g, gr_instance_id, is_ctxsw,
			tsg, false);
}

int nvgpu_profiler_unbind_hwpm(struct gk20a *g, u32 gr_instance_id,
		bool is_ctxsw, struct nvgpu_tsg *tsg)
{
	int err = 0;
	u32 mode = NVGPU_GR_CTX_HWPM_CTXSW_MODE_NO_CTXSW;

	if (is_ctxsw) {
		err = g->ops.gr.update_hwpm_ctxsw_mode(
				g, gr_instance_id, tsg, mode);
	}

	return err;
}

static void nvgpu_profiler_disable_cau_and_smpc(struct gk20a *g)
{
	/* Disable CAUs */
	if (g->ops.gr.disable_cau != NULL) {
		g->ops.gr.disable_cau(g);
	}

	/* Disable SMPC */
	if (g->ops.gr.disable_smpc != NULL) {
		g->ops.gr.disable_smpc(g);
	}
}

static int nvgpu_profiler_quiesce_hwpm_streamout_resident(struct gk20a *g,
					u32 gr_instance_id,
					void *pma_bytes_available_buffer_cpuva,
					bool smpc_reserved)
{
	u64 bytes_available;
	int err = 0;

	(void)gr_instance_id;

	nvgpu_log(g, gpu_dbg_prof,
		"HWPM streamout quiesce in resident state started");

	/* Enable streamout */
	g->ops.perf.pma_stream_enable(g, true);

	/* Disable all perfmons */
	g->ops.perf.disable_all_perfmons(g);

	if (smpc_reserved) {
		/*
		 * TODO: Currently only one profiler object should be
		 * allowed. Reset CAU/smpc is using whole GR space for both
		 * MIG and legacy mode. Need to convert broadcast address
		 * to GR specific unicast programming when NvGpu supports
		 * more than one profiler object at a time.
		 */
		nvgpu_gr_exec_for_all_instances(g,
			nvgpu_profiler_disable_cau_and_smpc(g));
	}

	/* Wait for routers to idle/quiescent */
	err = g->ops.perf.wait_for_idle_pmm_routers(g);
	if (err != 0) {
		goto fail;
	}

	/* Wait for PMA to idle/quiescent */
	err = g->ops.perf.wait_for_idle_pma(g);
	if (err != 0) {
		goto fail;
	}

#ifdef CONFIG_NVGPU_NON_FUSA
	nvgpu_profiler_hs_stream_quiesce(g);
#endif

	/* Disable streamout */
	g->ops.perf.pma_stream_enable(g, false);

	/* wait for all the inflight records from fb-hub to stream out */
	err = nvgpu_perfbuf_update_get_put(g, 0U, &bytes_available,
				pma_bytes_available_buffer_cpuva, true,
				NULL, NULL);

fail:
	if (err != 0) {
		nvgpu_err(g, "Failed to quiesce HWPM streamout in resident state");
	} else {
		nvgpu_log(g, gpu_dbg_prof,
			"HWPM streamout quiesce in resident state successfull");
	}

	return 0;
}

static int nvgpu_profiler_quiesce_hwpm_streamout_non_resident(struct gk20a *g,
		struct nvgpu_tsg *tsg)
{
	struct nvgpu_mem *pm_ctx_mem;

	nvgpu_log(g, gpu_dbg_prof,
		"HWPM streamout quiesce in non-resident state started");

	if (tsg == NULL || tsg->gr_ctx == NULL) {
		return -EINVAL;
	}

	pm_ctx_mem = nvgpu_gr_ctx_get_pm_ctx_mem(tsg->gr_ctx);
	if (pm_ctx_mem == NULL) {
		nvgpu_err(g, "No PM context");
		return -EINVAL;
	}

	nvgpu_memset(g, pm_ctx_mem, 0U, 0U, pm_ctx_mem->size);
	nvgpu_log(g, gpu_dbg_prof,
		"HWPM streamout quiesce in non-resident state successfull");

	return 0;
}

static int nvgpu_profiler_disable_ctxsw_and_check_is_tsg_ctx_resident(
	struct nvgpu_tsg *tsg)
{
	struct gk20a *g = tsg->g;
	int err;

	err = nvgpu_gr_disable_ctxsw(g);
	if (err != 0) {
		nvgpu_err(g, "unable to stop gr ctxsw");
		return err;
	}

	return g->ops.gr.is_tsg_ctx_resident(tsg);
}

static int nvgpu_profiler_quiesce_hwpm_streamout_ctx(struct gk20a *g,
		u32 gr_instance_id,
		struct nvgpu_tsg *tsg,
		void *pma_bytes_available_buffer_cpuva,
		bool smpc_reserved)
{
	bool ctx_resident;
	int err, ctxsw_err;

	ctx_resident = nvgpu_gr_exec_with_err_for_instance(g, gr_instance_id,
		nvgpu_profiler_disable_ctxsw_and_check_is_tsg_ctx_resident(tsg));

	if (ctx_resident) {
		err = nvgpu_profiler_quiesce_hwpm_streamout_resident(g,
				gr_instance_id,
				pma_bytes_available_buffer_cpuva,
				smpc_reserved);
	} else {
		err = nvgpu_profiler_quiesce_hwpm_streamout_non_resident(g, tsg);
	}
	if (err != 0) {
		nvgpu_err(g, "Failed to quiesce HWPM streamout");
	}

	ctxsw_err = nvgpu_gr_exec_with_err_for_instance(g, gr_instance_id,
					nvgpu_gr_enable_ctxsw(g));
	if (ctxsw_err != 0) {
		nvgpu_err(g, "unable to restart ctxsw!");
		err = ctxsw_err;
	}

	return err;
}

static int nvgpu_profiler_quiesce_hwpm_streamout(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg,
		void *pma_bytes_available_buffer_cpuva,
		bool smpc_reserved)
{
	if (!is_ctxsw) {
		return nvgpu_profiler_quiesce_hwpm_streamout_resident(g,
						gr_instance_id,
						pma_bytes_available_buffer_cpuva,
						smpc_reserved);
	} else {
		return nvgpu_profiler_quiesce_hwpm_streamout_ctx(g,
						gr_instance_id,
						tsg,
						pma_bytes_available_buffer_cpuva,
						smpc_reserved);
	}
}

int nvgpu_profiler_bind_hwpm_streamout(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg,
		u64 pma_buffer_va,
		u32 pma_buffer_size,
		u64 pma_bytes_available_buffer_va)
{
	int err;

	err = nvgpu_profiler_bind_hwpm_common(g, gr_instance_id, is_ctxsw, tsg, true);
	if (err) {
		return err;
	}

	err = g->ops.perfbuf.perfbuf_enable(g, pma_buffer_va, pma_buffer_size);
	if (err) {
		nvgpu_profiler_unbind_hwpm(g, gr_instance_id, is_ctxsw, tsg);
		return err;
	}

	g->ops.perf.bind_mem_bytes_buffer_addr(g, pma_bytes_available_buffer_va);
	return 0;
}

int nvgpu_profiler_unbind_hwpm_streamout(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg,
		void *pma_bytes_available_buffer_cpuva,
		bool smpc_reserved)
{
	int err;

	err = nvgpu_profiler_quiesce_hwpm_streamout(g,
			gr_instance_id,
			is_ctxsw, tsg,
			pma_bytes_available_buffer_cpuva,
			smpc_reserved);
	if (err) {
		return err;
	}

	g->ops.perf.bind_mem_bytes_buffer_addr(g, 0ULL);

	err = g->ops.perfbuf.perfbuf_disable(g);
	if (err) {
		return err;
	}

	err = nvgpu_profiler_unbind_hwpm(g, gr_instance_id, is_ctxsw, tsg);
	if (err) {
		return err;
	}

	return 0;
}

int nvgpu_profiler_bind_pm_resources(struct nvgpu_profiler_object *prof)
{
	struct gk20a *g = prof->g;
	bool is_ctxsw;
	int err;
	u32 gr_instance_id;

	nvgpu_log(g, gpu_dbg_prof,
		"Request to bind PM resources with profiler handle %u",
		prof->prof_handle);

	if (prof->bound) {
		nvgpu_err(g, "PM resources are already bound with profiler handle %u",
			prof->prof_handle);
		return -EINVAL;
	}

	if (!prof->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_HWPM_LEGACY] &&
	    !prof->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_SMPC]) {
		nvgpu_err(g, "No PM resources reserved for profiler handle %u",
			prof->prof_handle);
		return -EINVAL;
	}

	err = gk20a_busy(g);
	if (err) {
		nvgpu_err(g, "failed to poweron");
		return err;
	}

	gr_instance_id = nvgpu_grmgr_get_gr_instance_id(g, prof->gpu_instance_id);

	if (prof->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_HWPM_LEGACY]) {
		is_ctxsw = nvgpu_profiler_is_context_resource(prof,
				  NVGPU_PROFILER_PM_RESOURCE_TYPE_HWPM_LEGACY);
		if (prof->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_PMA_STREAM]) {
			err = g->ops.profiler.bind_hwpm_streamout(g,
					gr_instance_id,
					is_ctxsw,
					prof->tsg,
					prof->pma_buffer_va,
					prof->pma_buffer_size,
					prof->pma_bytes_available_buffer_va);
			if (err != 0) {
				nvgpu_err(g,
					"failed to bind HWPM streamout with profiler handle %u",
					prof->prof_handle);
				goto fail;
			}

			nvgpu_log(g, gpu_dbg_prof,
				"HWPM streamout bound with profiler handle %u",
				prof->prof_handle);
		} else {
			err = g->ops.profiler.bind_hwpm(prof->g, gr_instance_id,
					is_ctxsw, prof->tsg);
			if (err != 0) {
				nvgpu_err(g,
					"failed to bind HWPM with profiler handle %u",
					prof->prof_handle);
				goto fail;
			}

			nvgpu_log(g, gpu_dbg_prof,
				"HWPM bound with profiler handle %u",
				prof->prof_handle);
		}
	}

	if (prof->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_SMPC]) {
		is_ctxsw = nvgpu_profiler_is_context_resource(prof,
				NVGPU_PROFILER_PM_RESOURCE_TYPE_SMPC);
		err = g->ops.profiler.bind_smpc(g, gr_instance_id,
				is_ctxsw, prof->tsg);
		if (err) {
			nvgpu_err(g, "failed to bind SMPC with profiler handle %u",
				prof->prof_handle);
			goto fail;
		}

		nvgpu_log(g, gpu_dbg_prof,
			"SMPC bound with profiler handle %u", prof->prof_handle);
	}

	err = nvgpu_profiler_build_regops_allowlist(prof);
	if (err != 0) {
		nvgpu_err(g, "failed to build allowlist");
		goto fail_unbind;
	}

	prof->bound = true;

	gk20a_idle(g);
	return 0;

fail_unbind:
	nvgpu_profiler_unbind_pm_resources(prof);
fail:
	gk20a_idle(g);
	return err;
}

int nvgpu_profiler_unbind_pm_resources(struct nvgpu_profiler_object *prof)
{
	struct gk20a *g = prof->g;
	bool is_ctxsw;
	int err;
	u32 gr_instance_id;

	if (!prof->bound) {
		nvgpu_err(g, "No PM resources bound to profiler handle %u",
			prof->prof_handle);
		return -EINVAL;
	}

	err = gk20a_busy(g);
	if (err) {
		nvgpu_err(g, "failed to poweron");
		return err;
	}

	gr_instance_id = nvgpu_grmgr_get_gr_instance_id(g, prof->gpu_instance_id);

	if (prof->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_HWPM_LEGACY]) {
		is_ctxsw = nvgpu_profiler_is_context_resource(prof,
				  NVGPU_PROFILER_PM_RESOURCE_TYPE_HWPM_LEGACY);
		if (prof->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_PMA_STREAM]) {
			err = g->ops.profiler.unbind_hwpm_streamout(g,
				gr_instance_id,
				is_ctxsw,
				prof->tsg,
				prof->pma_bytes_available_buffer_cpuva,
				prof->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_SMPC]);
			if (err) {
				nvgpu_err(g,
					"failed to unbind HWPM streamout from profiler handle %u",
					prof->prof_handle);
				goto fail;
			}

			nvgpu_log(g, gpu_dbg_prof,
				"HWPM streamout unbound from profiler handle %u",
				prof->prof_handle);
		} else {
			err = g->ops.profiler.unbind_hwpm(g, gr_instance_id,
					is_ctxsw, prof->tsg);
			if (err) {
				nvgpu_err(g,
					"failed to unbind HWPM from profiler handle %u",
					prof->prof_handle);
				goto fail;
			}

			nvgpu_log(g, gpu_dbg_prof,
				"HWPM unbound from profiler handle %u",
				prof->prof_handle);
		}
	}

	if (prof->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_SMPC]) {
		is_ctxsw = nvgpu_profiler_is_context_resource(prof,
				NVGPU_PROFILER_PM_RESOURCE_TYPE_SMPC);
		err = g->ops.profiler.unbind_smpc(g, gr_instance_id,
				is_ctxsw, prof->tsg);
		if (err) {
			nvgpu_err(g,
				"failed to unbind SMPC from profiler handle %u",
				prof->prof_handle);
			goto fail;
		}

		nvgpu_log(g, gpu_dbg_prof,
			"SMPC unbound from profiler handle %u",	prof->prof_handle);
	}

	nvgpu_profiler_destroy_regops_allowlist(prof);
	prof->bound = false;

fail:
	gk20a_idle(g);
	return err;
}

int nvgpu_profiler_alloc_pma_stream(struct nvgpu_profiler_object *prof)
{
	struct gk20a *g = prof->g;
	int err;

	err = nvgpu_profiler_pm_resource_reserve(prof,
			NVGPU_PROFILER_PM_RESOURCE_TYPE_PMA_STREAM);
	if (err) {
		nvgpu_err(g, "failed to reserve PMA stream");
		return err;
	}

	err = nvgpu_perfbuf_init_vm(g);
	if (err) {
		nvgpu_err(g, "failed to initialize perfbuf VM");
		nvgpu_profiler_pm_resource_release(prof,
				NVGPU_PROFILER_PM_RESOURCE_TYPE_PMA_STREAM);
		return err;
	}

	return 0;
}

void nvgpu_profiler_free_pma_stream(struct nvgpu_profiler_object *prof)
{
	struct gk20a *g = prof->g;

	if (prof->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_PMA_STREAM]) {
		nvgpu_perfbuf_deinit_vm(g);
		nvgpu_profiler_pm_resource_release(prof,
				NVGPU_PROFILER_PM_RESOURCE_TYPE_PMA_STREAM);
	}
}

static int map_cmp(const void *a, const void *b)
{
	const struct nvgpu_pm_resource_register_range_map *e1;
	const struct nvgpu_pm_resource_register_range_map *e2;

	e1 = (const struct nvgpu_pm_resource_register_range_map *)a;
	e2 = (const struct nvgpu_pm_resource_register_range_map *)b;

	if (e1->start < e2->start) {
		return -1;
	}

	if (e1->start > e2->start) {
		return 1;
	}

	return 0;
}

static u32 get_pm_resource_register_range_map_entry_count(struct nvgpu_profiler_object *prof)
{
	struct gk20a *g = prof->g;
	u32 count = 0U;
	u32 range_count;

	/* Account for TYPE_TEST entries added in add_test_range_to_map() */
	count += 2U;

	if (prof->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_SMPC]) {
		g->ops.regops.get_smpc_register_ranges(&range_count);
		count += range_count;
	}

	if (prof->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_HWPM_LEGACY]) {
		g->ops.regops.get_hwpm_perfmon_register_ranges(&range_count);
		count += range_count;

		g->ops.regops.get_hwpm_router_register_ranges(&range_count);
		count += range_count;

		g->ops.regops.get_hwpm_pma_trigger_register_ranges(&range_count);
		count += range_count;

		g->ops.regops.get_hwpm_perfmux_register_ranges(&range_count);
		count += range_count;

		if (g->ops.regops.get_cau_register_ranges != NULL) {
			g->ops.regops.get_cau_register_ranges(&range_count);
			count += range_count;
		}
	}

	if (prof->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_PMA_STREAM]) {
		g->ops.regops.get_hwpm_pma_channel_register_ranges(&range_count);
		count += range_count;
	}

	if (prof->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_PC_SAMPLER]) {
		g->ops.regops.get_hwpm_pc_sampler_register_ranges(&range_count);
		count += range_count;
	}

	return count;
}

static void add_range_to_map(const struct nvgpu_pm_resource_register_range *range,
		u32 range_count, struct nvgpu_pm_resource_register_range_map *map,
		u32 *map_index, enum nvgpu_pm_resource_hwpm_register_type type)
{
	u32 index = *map_index;
	u32 i;

	for (i = 0U; i < range_count; i++) {
		map[index].start = range[i].start;
		map[index].end = range[i].end;
		map[index].type = type;
		index++;
	}

	*map_index = index;
}

static void add_test_range_to_map(struct gk20a *g,
		struct nvgpu_pm_resource_register_range_map *map,
		u32 *map_index, enum nvgpu_pm_resource_hwpm_register_type type)
{
	u32 index = *map_index;
	u32 timer0_offset, timer1_offset;

	g->ops.ptimer.get_timer_reg_offsets(&timer0_offset, &timer1_offset);

	map[index].start = timer0_offset;
	map[index].end = timer0_offset;
	map[index].type = type;
	index++;

	map[index].start = timer1_offset;
	map[index].end = timer1_offset;
	map[index].type = type;
	index++;

	*map_index = index;
}

static int nvgpu_profiler_build_regops_allowlist(struct nvgpu_profiler_object *prof)
{
	struct nvgpu_pm_resource_register_range_map *map;
	const struct nvgpu_pm_resource_register_range *range;
	u32 map_count, map_index = 0U;
	u32 range_count;
	struct gk20a *g = prof->g;
	u32 i;

	map_count = get_pm_resource_register_range_map_entry_count(prof);
	if (map_count == 0U) {
		return -EINVAL;
	}

	nvgpu_log(g, gpu_dbg_prof, "Allowlist map number of entries %u for handle %u",
		map_count, prof->prof_handle);

	map = nvgpu_kzalloc(g, sizeof(*map) * map_count);
	if (map == NULL) {
		return -ENOMEM;
	}

	if (prof->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_SMPC]) {
		range = g->ops.regops.get_smpc_register_ranges(&range_count);
		add_range_to_map(range, range_count, map, &map_index,
			NVGPU_HWPM_REGISTER_TYPE_SMPC);
	}

	if (prof->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_HWPM_LEGACY]) {
		range = g->ops.regops.get_hwpm_perfmon_register_ranges(&range_count);
		add_range_to_map(range, range_count, map, &map_index,
			NVGPU_HWPM_REGISTER_TYPE_HWPM_PERFMON);

		range = g->ops.regops.get_hwpm_router_register_ranges(&range_count);
		add_range_to_map(range, range_count, map, &map_index,
			NVGPU_HWPM_REGISTER_TYPE_HWPM_ROUTER);

		range = g->ops.regops.get_hwpm_pma_trigger_register_ranges(&range_count);
		add_range_to_map(range, range_count, map, &map_index,
			NVGPU_HWPM_REGISTER_TYPE_HWPM_PMA_TRIGGER);

		range = g->ops.regops.get_hwpm_perfmux_register_ranges(&range_count);
		add_range_to_map(range, range_count, map, &map_index,
			NVGPU_HWPM_REGISTER_TYPE_HWPM_PERFMUX);

		if (g->ops.regops.get_cau_register_ranges != NULL) {
			range = g->ops.regops.get_cau_register_ranges(&range_count);
			add_range_to_map(range, range_count, map, &map_index,
				NVGPU_HWPM_REGISTER_TYPE_CAU);
		}
	}

	if (prof->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_PMA_STREAM]) {
		range = g->ops.regops.get_hwpm_pma_channel_register_ranges(&range_count);
		add_range_to_map(range, range_count, map, &map_index,
			NVGPU_HWPM_REGISTER_TYPE_HWPM_PMA_CHANNEL);
	}

	if (prof->reserved[NVGPU_PROFILER_PM_RESOURCE_TYPE_PC_SAMPLER]) {
		range = g->ops.regops.get_hwpm_pc_sampler_register_ranges(&range_count);
		add_range_to_map(range, range_count, map, &map_index,
				NVGPU_HWPM_REGISTER_TYPE_PC_SAMPLER);
	}

	add_test_range_to_map(g, map, &map_index, NVGPU_HWPM_REGISTER_TYPE_TEST);

	nvgpu_log(g, gpu_dbg_prof, "Allowlist map created successfully for handle %u",
		prof->prof_handle);

	nvgpu_assert(map_count == map_index);

	sort(map, map_count, sizeof(*map), map_cmp, NULL);

	for (i = 0; i < map_count; i++) {
		nvgpu_log(g, gpu_dbg_prof, "allowlist[%u]: 0x%x-0x%x : type %u",
			i, map[i].start, map[i].end, map[i].type);
	}

	prof->map = map;
	prof->map_count = map_count;
	return 0;
}

static void nvgpu_profiler_destroy_regops_allowlist(struct nvgpu_profiler_object *prof)
{
	nvgpu_log(prof->g, gpu_dbg_prof, "Allowlist map destroy for handle %u",
		prof->prof_handle);

	nvgpu_kfree(prof->g, prof->map);
}

bool nvgpu_profiler_allowlist_range_search(struct gk20a *g,
		struct nvgpu_pm_resource_register_range_map *map,
		u32 map_count, u32 offset,
		struct nvgpu_pm_resource_register_range_map *entry)
{
	s32 start = 0;
	s32 mid = 0;
	s32 end = nvgpu_safe_sub_s32((s32)map_count, 1);
	bool found = false;

	while (start <= end) {
		mid = start + (end - start) / 2;

		if (offset < map[mid].start) {
			end = mid - 1;
		} else if (offset > map[mid].end) {
			start = mid + 1;
		} else {
			found = true;
			break;
		}
	}

	if (found) {
		*entry = map[mid];
		nvgpu_log(g, gpu_dbg_prof, "Offset 0x%x found in range 0x%x-0x%x, type: %u",
			offset, map[mid].start, map[mid].end, map[mid].type);
	} else {
		nvgpu_log(g, gpu_dbg_prof, "Offset 0x%x not found in range search", offset);
	}

	return found;
}

static bool allowlist_offset_search(struct gk20a *g,
		const u32 *offset_allowlist, u32 count, u32 offset)
{
	s32 start = 0;
	s32 mid = 0;
	s32 end = nvgpu_safe_sub_s32((s32)count, 1);
	bool found = false;

	while (start <= end) {
		mid = start + (end - start) / 2;
		if (offset_allowlist[mid] == offset) {
			found = true;
			break;
		}

		if (offset < offset_allowlist[mid]) {
			end = mid - 1;
		} else {
			start = mid + 1;
		}
	}

	if (found) {
		nvgpu_log(g, gpu_dbg_prof, "Offset 0x%x found in offset allowlist",
			offset);
	} else {
		nvgpu_log(g, gpu_dbg_prof, "Offset 0x%x not found in offset allowlist",
			offset);
	}

	return found;
}

bool nvgpu_profiler_validate_regops_allowlist(struct nvgpu_profiler_object *prof,
		u32 offset, enum nvgpu_pm_resource_hwpm_register_type type)
{
	struct gk20a *g = prof->g;
	const u32 *offset_allowlist;
	u32 count;
	u32 stride;

	if ((type == NVGPU_HWPM_REGISTER_TYPE_HWPM_PERFMUX) ||
	    (type == NVGPU_HWPM_REGISTER_TYPE_TEST)) {
		return true;
	}

	switch ((u32)type) {
	case NVGPU_HWPM_REGISTER_TYPE_HWPM_PERFMON:
		offset_allowlist = g->ops.regops.get_hwpm_perfmon_register_offset_allowlist(&count);
		stride = g->ops.regops.get_hwpm_perfmon_register_stride();
		break;

	case NVGPU_HWPM_REGISTER_TYPE_HWPM_ROUTER:
		offset_allowlist = g->ops.regops.get_hwpm_router_register_offset_allowlist(&count);
		stride = g->ops.regops.get_hwpm_router_register_stride();
		break;

	case NVGPU_HWPM_REGISTER_TYPE_HWPM_PMA_TRIGGER:
		offset_allowlist = g->ops.regops.get_hwpm_pma_trigger_register_offset_allowlist(&count);
		stride = g->ops.regops.get_hwpm_pma_trigger_register_stride();
		break;

	case NVGPU_HWPM_REGISTER_TYPE_SMPC:
		offset_allowlist = g->ops.regops.get_smpc_register_offset_allowlist(&count);
		stride = g->ops.regops.get_smpc_register_stride();
		break;

	case NVGPU_HWPM_REGISTER_TYPE_CAU:
		offset_allowlist = g->ops.regops.get_cau_register_offset_allowlist(&count);
		stride = g->ops.regops.get_cau_register_stride();
		break;

	case NVGPU_HWPM_REGISTER_TYPE_HWPM_PMA_CHANNEL:
		offset_allowlist = g->ops.regops.get_hwpm_pma_channel_register_offset_allowlist(&count);
		stride = g->ops.regops.get_hwpm_pma_channel_register_stride();
		break;

	default:
		return false;
	}

	offset = offset & (stride - 1U);
	return allowlist_offset_search(g, offset_allowlist, count, offset);
}

#ifdef CONFIG_NVGPU_NON_FUSA
void nvgpu_profiler_hs_stream_quiesce(struct gk20a *g)
{
	if (g->ops.perf.reset_hs_streaming_credits != NULL) {
		/* Reset high speed streaming credits to 0. */
		g->ops.perf.reset_hs_streaming_credits(g);
	}

	if (g->ops.perf.enable_hs_streaming != NULL) {
		/* Disable high speed streaming */
		g->ops.perf.enable_hs_streaming(g, false);
	}
}
#endif /* CONFIG_NVGPU_NON_FUSA */
