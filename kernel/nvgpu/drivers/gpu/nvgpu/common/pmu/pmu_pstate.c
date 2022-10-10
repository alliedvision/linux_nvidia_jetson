/*
 * general p state infrastructure
 *
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/bios.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/pmu.h>
#include <nvgpu/boardobjgrp.h>
#include <nvgpu/boardobjgrp_e32.h>
#include <nvgpu/boardobjgrp_e255.h>
#include <nvgpu/pmu/clk/clk.h>
#include <nvgpu/pmu/pmgr.h>
#include <nvgpu/pmu/therm.h>
#include <nvgpu/pmu/perf.h>
#include <nvgpu/pmu/volt.h>
#include <nvgpu/pmu/pmu_pstate.h>

#include "boardobj/boardobj.h"

void nvgpu_pmu_pstate_deinit(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_DGPU
	pmgr_pmu_free_pmupstate(g);
	nvgpu_pmu_therm_deinit(g, g->pmu);

	if (g->pmu->perf_pmu != NULL) {
		nvgpu_pmu_perf_deinit(g);
	}

	if (g->pmu->volt != NULL) {
		nvgpu_pmu_volt_deinit(g);
	}

	nvgpu_pmu_clk_deinit(g);
#endif

	if (g->ops.clk.mclk_deinit != NULL) {
		g->ops.clk.mclk_deinit(g);
	}
}

#ifdef CONFIG_NVGPU_DGPU
static int pmu_pstate_init(struct gk20a *g)
{
	int err;
	nvgpu_log_fn(g, " ");

	err = nvgpu_pmu_therm_init(g, g->pmu);
	if (err != 0) {
		nvgpu_pmu_therm_deinit(g, g->pmu);
		return err;
	}

	err = nvgpu_pmu_clk_init(g);
	if (err != 0) {
		return err;
	}

	err = nvgpu_pmu_perf_init(g);
	if (err != 0) {
		nvgpu_pmu_perf_deinit(g);
		return err;
	}

	err = nvgpu_pmu_volt_init(g);
	if (err != 0) {
		return err;
	}

	err = pmgr_pmu_init_pmupstate(g);
	if (err != 0) {
		pmgr_pmu_free_pmupstate(g);
		return err;
	}

	return 0;
}
#endif

/*sw setup for pstate components*/
int nvgpu_pmu_pstate_sw_setup(struct gk20a *g)
{
	int err;
	nvgpu_log_fn(g, " ");

	err = nvgpu_pmu_wait_fw_ready(g, g->pmu);
	if (err != 0) {
		nvgpu_err(g, "PMU not ready to process pstate requests");
		return err;
	}
#ifdef CONFIG_NVGPU_DGPU
	err = pmu_pstate_init(g);
	if (err != 0) {
		nvgpu_err(g, "Pstate init failed");
		return err;
	}

	err = nvgpu_pmu_volt_sw_setup(g);
	if (err != 0) {
		nvgpu_err(g, "Volt sw setup failed");
		return err;
	}

	err = nvgpu_pmu_therm_sw_setup(g, g->pmu);
	if (err != 0) {
		goto err_therm_pmu_init_pmupstate;
	}

	err = nvgpu_pmu_clk_sw_setup(g);
	if (err != 0) {
		nvgpu_err(g, "Clk sw setup failed");
		return err;
	}

	err = nvgpu_pmu_perf_sw_setup(g);
	if (err != 0) {
		nvgpu_err(g, "Perf sw setup failed");
		goto err_perf_pmu_init_pmupstate;
	}

	if (g->ops.clk.support_pmgr_domain) {
		err = pmgr_domain_sw_setup(g);
		if (err != 0) {
			goto err_pmgr_pmu_init_pmupstate;
		}
	}
#endif

	return 0;

#ifdef CONFIG_NVGPU_DGPU
err_pmgr_pmu_init_pmupstate:
	pmgr_pmu_free_pmupstate(g);
err_therm_pmu_init_pmupstate:
	nvgpu_pmu_therm_deinit(g, g->pmu);
err_perf_pmu_init_pmupstate:
	nvgpu_pmu_perf_deinit(g);
#endif
	return err;
}

/*sw setup for pstate components*/
int nvgpu_pmu_pstate_pmu_setup(struct gk20a *g)
{
	int err;
	nvgpu_log_fn(g, " ");

	if (g->ops.clk.mclk_init != NULL) {
		err = g->ops.clk.mclk_init(g);
		if (err != 0) {
			nvgpu_err(g, "failed to set mclk");
			/* Indicate error and continue */
		}
	}

#ifdef CONFIG_NVGPU_DGPU
	err = nvgpu_pmu_volt_pmu_setup(g);
	if (err != 0) {
		nvgpu_err(g, "Failed to send VOLT pmu setup");
		return err;
	}

	err = nvgpu_pmu_therm_pmu_setup(g, g->pmu);
	if (err != 0) {
		return err;
	}

	err = nvgpu_pmu_clk_pmu_setup(g);
	if (err != 0) {
		nvgpu_err(g, "Failed to send CLK pmu setup");
		return err;
	}

	err = nvgpu_pmu_perf_pmu_setup(g);
	if (err != 0) {
		nvgpu_err(g, "Failed to send Perf pmu setup");
		return err;
	}

	if (g->ops.clk.support_pmgr_domain) {
		err = pmgr_domain_pmu_setup(g);
		if (err != 0) {
			return err;
		}
	}
#endif

	err = g->ops.clk.perf_pmu_vfe_load(g);
	if (err != 0) {
		return err;
	}

	return err;
}

