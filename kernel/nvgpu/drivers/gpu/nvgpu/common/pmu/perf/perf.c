/*
 *
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/pmu.h>
#include <nvgpu/bug.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/pmu/clk/clk.h>
#include <nvgpu/clk_arb.h>
#include <nvgpu/pmu/perf.h>
#include <nvgpu/pmu/cmd.h>

#include "ucode_perf_change_seq_inf.h"
#include "ucode_perf_pstate_inf.h"
#include "ucode_perf_vfe_inf.h"
#include "perf.h"

static int pmu_set_boot_clk_runcb_fn(void *arg)
{
	struct gk20a *g = (struct gk20a *)arg;
	struct nvgpu_pmu_perf *perf_pmu = g->pmu->perf_pmu;
	struct perf_vfe_invalidate *vfe_init = &perf_pmu->vfe_init;

	nvgpu_log_fn(g, "thread start");

	while (true) {
		NVGPU_COND_WAIT_INTERRUPTIBLE(&vfe_init->wq,
			(vfe_init->state_change ||
			nvgpu_thread_should_stop(&vfe_init->state_task)), 0U);
		if (nvgpu_thread_should_stop(&vfe_init->state_task)) {
			break;
		}
		vfe_init->state_change = false;

#ifdef CONFIG_NVGPU_CLK_ARB
		nvgpu_clk_arb_schedule_vf_table_update(g);
#endif
	}

	return 0;
}

static int tu104_pmu_handle_perf_event(struct gk20a *g, void *pmumsg)
{
	struct pmu_nvgpu_rpc_perf_event *msg =
			(struct pmu_nvgpu_rpc_perf_event *)pmumsg;
	struct nvgpu_pmu_perf *perf_pmu = g->pmu->perf_pmu;
	struct change_seq_pmu *change_pmu = &g->pmu->perf_pmu->changeseq_pmu;

	nvgpu_log_fn(g, " ");
	switch (msg->rpc_hdr.function) {
	case NV_PMU_RPC_ID_PERF_VFE_CALLBACK:
		perf_pmu->vfe_init.state_change = true;
		(void) nvgpu_cond_signal_interruptible(&perf_pmu->vfe_init.wq);
		break;
	case NV_PMU_RPC_ID_PERF_SEQ_COMPLETION:
		change_pmu->change_state = 1U;
		nvgpu_log_info(g, "Change Seq Completed");
		break;
	case NV_PMU_RPC_ID_PERF_PSTATES_INVALIDATE:
		nvgpu_log_info(g, "Pstate Invalidated");
		break;
	default:
		WARN_ON(true);
		break;
	}
	return 0;
}

static int perf_pmu_init_vfe_perf_event(struct gk20a *g)
{
	struct nvgpu_pmu_perf *perf_pmu = g->pmu->perf_pmu;
	char thread_name[64];
	int err = 0;

	nvgpu_log_fn(g, " ");

	nvgpu_cond_init(&perf_pmu->vfe_init.wq);

	(void) snprintf(thread_name, sizeof(thread_name),
				"nvgpu_vfe_invalidate_init_%s", g->name);

	err = nvgpu_thread_create(&perf_pmu->vfe_init.state_task, g,
			pmu_set_boot_clk_runcb_fn, thread_name);
	if (err != 0) {
		nvgpu_err(g, "failed to start nvgpu_vfe_invalidate_init thread");
	}

	return err;

}

int nvgpu_pmu_perf_load(struct gk20a *g)
{
	struct nvgpu_pmu *pmu = g->pmu;
	struct nv_pmu_rpc_struct_perf_load rpc;
	int status = 0;

	status = perf_pmu_init_vfe_perf_event(g);
	if (status != 0) {
		return status;
	}

	/*register call back for future VFE updates*/
	g->ops.pmu_perf.handle_pmu_perf_event = tu104_pmu_handle_perf_event;

	(void) memset(&rpc, 0, sizeof(struct nv_pmu_rpc_struct_perf_load));
	rpc.b_load = true;
	PMU_RPC_EXECUTE_CPB(status, pmu, PERF, LOAD, &rpc, 0);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute RPC status=0x%x",
			status);
		nvgpu_thread_stop(&g->pmu->perf_pmu->vfe_init.state_task);
	}

	return status;
}

int nvgpu_pmu_perf_init(struct gk20a *g)
{
	/* If already allocated, do not re-allocate */
	if (g->pmu->perf_pmu != NULL) {
		return 0;
	}

	g->pmu->perf_pmu = nvgpu_kzalloc(g, sizeof(*g->pmu->perf_pmu));
	if (g->pmu->perf_pmu == NULL) {
		return -ENOMEM;
	}

	return 0;
}

static void vfe_thread_stop_cb(void *data)
{
	struct nvgpu_cond *cond = (struct nvgpu_cond *)data;

	nvgpu_cond_signal(cond);
}

void nvgpu_pmu_perf_deinit(struct gk20a *g)
{
	if (nvgpu_thread_is_running(&g->pmu->perf_pmu->vfe_init.state_task)) {
		nvgpu_thread_stop_graceful(&g->pmu->perf_pmu->vfe_init.state_task,
				vfe_thread_stop_cb, &g->pmu->perf_pmu->vfe_init.wq);
	}
	nvgpu_cond_destroy(&g->pmu->perf_pmu->vfe_init.wq);
	nvgpu_kfree(g, g->pmu->perf_pmu);
	g->pmu->perf_pmu = NULL;
}

int nvgpu_pmu_perf_sw_setup(struct gk20a *g)
{
	int err;
	nvgpu_log_fn(g, " ");

	err = perf_vfe_var_sw_setup(g);
	if (err != 0) {
		return err;
	}

	err = perf_vfe_equ_sw_setup(g);
	if (err != 0) {
		return err;
	}

	err = perf_pstate_sw_setup(g);
	if (err != 0) {
		return err;
	}

	err = perf_change_seq_sw_setup(g);
	if (err != 0) {
		return err;
	}

	return 0;
}

int nvgpu_pmu_perf_pmu_setup(struct gk20a *g)
{
	int err;
	nvgpu_log_fn(g, " ");

	err = perf_vfe_var_pmu_setup(g);
	if (err != 0) {
		return err;
	}

	err = perf_vfe_equ_pmu_setup(g);
	if (err != 0) {
		return err;
	}

	err = perf_pstate_pmu_setup(g);
	if (err != 0) {
		return err;
	}

	err = perf_change_seq_pmu_setup(g);
	if (err != 0) {
		return err;
	}

	return 0;
}

