/*
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
#include <nvgpu/pmu/pmuif/nvgpu_cmdif.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/bug.h>
#include <nvgpu/pmu/clk/clk.h>
#include <nvgpu/pmu/volt.h>
#include <nvgpu/pmu/perf.h>
#include <nvgpu/pmu/cmd.h>
#include <nvgpu/timers.h>
#include <nvgpu/pmu/pmu_pstate.h>

#include "ucode_clk_inf.h"
#include "clk_domain.h"
#include "clk_prog.h"
#include "clk_vin.h"
#include "clk_fll.h"
#include "clk_vf_point.h"
#include "clk.h"

static int clk_init_pmupstate(struct gk20a *g)
{
	/* If already allocated, do not re-allocate */
	if (g->pmu->clk_pmu != NULL) {
		return 0;
	}

	g->pmu->clk_pmu = nvgpu_kzalloc(g, sizeof(*g->pmu->clk_pmu));
	if (g->pmu->clk_pmu == NULL) {
		return -ENOMEM;
	}

	return 0;
}

static void clk_free_pmupstate(struct gk20a *g)
{
	nvgpu_kfree(g, g->pmu->clk_pmu);
	g->pmu->clk_pmu = NULL;
}

u32 nvgpu_pmu_clk_mon_init_domains(struct gk20a *g)
{
	u32 domain_mask;

	domain_mask = (CTRL_CLK_DOMAIN_MCLK |
			CTRL_CLK_DOMAIN_XBARCLK 	|
			CTRL_CLK_DOMAIN_SYSCLK		|
			CTRL_CLK_DOMAIN_HUBCLK		|
			CTRL_CLK_DOMAIN_GPCCLK		|
			CTRL_CLK_DOMAIN_HOSTCLK		|
			CTRL_CLK_DOMAIN_UTILSCLK	|
			CTRL_CLK_DOMAIN_PWRCLK		|
			CTRL_CLK_DOMAIN_NVDCLK		|
			CTRL_CLK_DOMAIN_XCLK		|
			CTRL_CLK_DOMAIN_NVL_COMMON	|
			CTRL_CLK_DOMAIN_PEX_REFCLK	);
	return domain_mask;
}

int nvgpu_pmu_clk_pmu_setup(struct gk20a *g)
{
	int err;
	nvgpu_log_fn(g, " ");

	err = clk_domain_pmu_setup(g);
	if (err != 0) {
		return err;
	}

	err = clk_prog_pmu_setup(g);
	if (err != 0) {
		return err;
	}

	err = clk_vin_pmu_setup(g);
	if (err != 0) {
		return err;
	}

	err = clk_fll_pmu_setup(g);
	if (err != 0) {
		return err;
	}

	if (g->ops.clk.support_vf_point) {
		err = clk_vf_point_pmu_setup(g);
		if (err != 0) {
			return err;
		}
	}

	err = clk_pmu_vin_load(g);
	if (err != 0) {
		return err;
	}

	err = clk_pmu_clk_domains_load(g);
	if (err != 0) {
		return err;
	}

	return 0;
}

int nvgpu_pmu_clk_sw_setup(struct gk20a *g)
{
	int err;
	nvgpu_log_fn(g, " ");

	err = clk_vin_sw_setup(g);
	if (err != 0) {
		clk_vin_free_pmupstate(g);
		return err;
	}

	err = clk_fll_sw_setup(g);
	if (err != 0) {
		clk_fll_free_pmupstate(g);
		return err;
	}

	err = clk_domain_sw_setup(g);
	if (err != 0) {
		clk_domain_free_pmupstate(g);
		return err;
	}

	if (g->ops.clk.support_vf_point) {
		err = clk_vf_point_sw_setup(g);
		if (err != 0) {
			clk_vf_point_free_pmupstate(g);
			return err;
		}
	}

	err = clk_prog_sw_setup(g);
	if (err != 0) {
		clk_prog_free_pmupstate(g);
		return err;
	}

	return 0;
}
int nvgpu_pmu_clk_init(struct gk20a *g)
{
	int err;
	nvgpu_log_fn(g, " ");

	err = clk_init_pmupstate(g);
	if (err != 0) {
		clk_free_pmupstate(g);
		return err;
	}

	err = clk_domain_init_pmupstate(g);
	if (err != 0) {
		clk_domain_free_pmupstate(g);
		return err;
	}

	err = clk_prog_init_pmupstate(g);
	if (err != 0) {
		clk_prog_free_pmupstate(g);
		return err;
	}

	err = clk_vf_point_init_pmupstate(g);
	if (err != 0) {
		clk_vf_point_free_pmupstate(g);
		return err;
	}

	err = clk_vin_init_pmupstate(g);
	if (err != 0) {
		clk_vin_free_pmupstate(g);
		return err;
	}

	err = clk_fll_init_pmupstate(g);
	if (err != 0) {
		clk_fll_free_pmupstate(g);
		return err;
	}

	return 0;
}
void nvgpu_pmu_clk_deinit(struct gk20a *g)
{
	if ((g->pmu != NULL) && (g->pmu->clk_pmu != NULL)) {
		clk_domain_free_pmupstate(g);
		clk_prog_free_pmupstate(g);
		clk_vf_point_free_pmupstate(g);
		clk_fll_free_pmupstate(g);
		clk_vin_free_pmupstate(g);
		clk_free_pmupstate(g);
	}
}
