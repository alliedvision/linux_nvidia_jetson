/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/gr/gr_ecc.h>

#include <nvgpu/hw/gp10b/hw_gr_gp10b.h>

#include "ecc_gp10b.h"

static void gp10b_ecc_enable_smlrf(struct gk20a *g,
				u32 fecs_feature_override_ecc, bool opt_ecc_en)
{
	if (gr_fecs_feature_override_ecc_sm_lrf_override_v(
				fecs_feature_override_ecc) == 1U) {
		if (gr_fecs_feature_override_ecc_sm_lrf_v(
				fecs_feature_override_ecc) == 1U) {
			nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_SM_LRF, true);
		}
	} else {
		if (opt_ecc_en) {
			nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_SM_LRF, true);
		}
	}
}

static void gp10b_ecc_enable_smshm(struct gk20a *g,
				u32 fecs_feature_override_ecc, bool opt_ecc_en)
{
	if (gr_fecs_feature_override_ecc_sm_shm_override_v(
				fecs_feature_override_ecc) == 1U) {
		if (gr_fecs_feature_override_ecc_sm_shm_v(
				fecs_feature_override_ecc) == 1U) {
			nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_SM_SHM, true);
		}
	} else {
		if (opt_ecc_en) {
			nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_SM_SHM, true);
		}
	}
}

static void gp10b_ecc_enable_tex(struct gk20a *g,
				u32 fecs_feature_override_ecc, bool opt_ecc_en)
{
	if (gr_fecs_feature_override_ecc_tex_override_v(
				fecs_feature_override_ecc) == 1U) {
		if (gr_fecs_feature_override_ecc_tex_v(
				fecs_feature_override_ecc) == 1U) {
			nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_TEX, true);
		}
	} else {
		if (opt_ecc_en) {
			nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_TEX, true);
		}
	}
}

static void gp10b_ecc_enable_ltc(struct gk20a *g,
				u32 fecs_feature_override_ecc, bool opt_ecc_en)
{
	if (gr_fecs_feature_override_ecc_ltc_override_v(
				fecs_feature_override_ecc) == 1U) {
		if (gr_fecs_feature_override_ecc_ltc_v(
				fecs_feature_override_ecc) == 1U) {
			nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_LTC, true);
		}
	} else {
		if (opt_ecc_en) {
			nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_LTC, true);
		}
	}
}

void gp10b_ecc_detect_enabled_units(struct gk20a *g)
{
	bool opt_ecc_en = g->ops.fuse.is_opt_ecc_enable(g);
	bool opt_feature_fuses_override_disable =
			g->ops.fuse.is_opt_feature_override_disable(g);
	u32 fecs_feature_override_ecc =
				nvgpu_readl(g,
					gr_fecs_feature_override_ecc_r());

	if (opt_feature_fuses_override_disable) {
		if (opt_ecc_en) {
			nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_SM_LRF, true);
			nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_SM_SHM, true);
			nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_TEX, true);
			nvgpu_set_enabled(g, NVGPU_ECC_ENABLED_LTC, true);
		}
	} else {
		/* SM LRF */
		gp10b_ecc_enable_smlrf(g,
				fecs_feature_override_ecc, opt_ecc_en);
		/* SM SHM */
		gp10b_ecc_enable_smshm(g,
				fecs_feature_override_ecc, opt_ecc_en);
		/* TEX */
		gp10b_ecc_enable_tex(g,
				fecs_feature_override_ecc, opt_ecc_en);
		/* LTC */
		gp10b_ecc_enable_ltc(g,
				fecs_feature_override_ecc, opt_ecc_en);
	}
}

static int gp10b_ecc_init_tpc_sm(struct gk20a *g)
{
	NVGPU_ECC_COUNTER_INIT_PER_TPC_OR_RETURN(sm_lrf_ecc_single_err_count);
	NVGPU_ECC_COUNTER_INIT_PER_TPC_OR_RETURN(sm_lrf_ecc_double_err_count);
	NVGPU_ECC_COUNTER_INIT_PER_TPC_OR_RETURN(sm_shm_ecc_sec_count);
	NVGPU_ECC_COUNTER_INIT_PER_TPC_OR_RETURN(sm_shm_ecc_sed_count);
	NVGPU_ECC_COUNTER_INIT_PER_TPC_OR_RETURN(sm_shm_ecc_ded_count);

	return 0;
}

static int gp10b_ecc_init_tpc_tex(struct gk20a *g)
{
	NVGPU_ECC_COUNTER_INIT_PER_TPC_OR_RETURN(tex_ecc_total_sec_pipe0_count);
	NVGPU_ECC_COUNTER_INIT_PER_TPC_OR_RETURN(tex_ecc_total_ded_pipe0_count);
	NVGPU_ECC_COUNTER_INIT_PER_TPC_OR_RETURN(tex_unique_ecc_sec_pipe0_count);
	NVGPU_ECC_COUNTER_INIT_PER_TPC_OR_RETURN(tex_unique_ecc_ded_pipe0_count);
	NVGPU_ECC_COUNTER_INIT_PER_TPC_OR_RETURN(tex_ecc_total_sec_pipe1_count);
	NVGPU_ECC_COUNTER_INIT_PER_TPC_OR_RETURN(tex_ecc_total_ded_pipe1_count);
	NVGPU_ECC_COUNTER_INIT_PER_TPC_OR_RETURN(tex_unique_ecc_sec_pipe1_count);
	NVGPU_ECC_COUNTER_INIT_PER_TPC_OR_RETURN(tex_unique_ecc_ded_pipe1_count);

	return 0;
}

static int gp10b_ecc_init_tpc(struct gk20a *g)
{
	int err = 0;

	err = gp10b_ecc_init_tpc_sm(g);
	if (err != 0) {
		goto init_tpc_err;
	}
	err = gp10b_ecc_init_tpc_tex(g);

init_tpc_err:
	return err;
}

int gp10b_gr_ecc_init(struct gk20a *g)
{
	int err = 0;

	err = gp10b_ecc_init_tpc(g);
	if (err != 0) {
		nvgpu_err(g, "ecc counter allocate failed, err=%d", err);
		gp10b_gr_ecc_deinit(g);
	}

	return err;
}

static void gp10b_ecc_deinit_tpc_sm(struct gk20a *g)
{
	NVGPU_ECC_COUNTER_DEINIT_PER_TPC(sm_lrf_ecc_single_err_count);
	NVGPU_ECC_COUNTER_DEINIT_PER_TPC(sm_lrf_ecc_double_err_count);
	NVGPU_ECC_COUNTER_DEINIT_PER_TPC(sm_shm_ecc_sec_count);
	NVGPU_ECC_COUNTER_DEINIT_PER_TPC(sm_shm_ecc_sed_count);
	NVGPU_ECC_COUNTER_DEINIT_PER_TPC(sm_shm_ecc_ded_count);
}

static void gp10b_ecc_deinit_tpc_tex(struct gk20a *g)
{
	NVGPU_ECC_COUNTER_DEINIT_PER_TPC(tex_ecc_total_sec_pipe0_count);
	NVGPU_ECC_COUNTER_DEINIT_PER_TPC(tex_ecc_total_ded_pipe0_count);
	NVGPU_ECC_COUNTER_DEINIT_PER_TPC(tex_unique_ecc_sec_pipe0_count);
	NVGPU_ECC_COUNTER_DEINIT_PER_TPC(tex_unique_ecc_ded_pipe0_count);
	NVGPU_ECC_COUNTER_DEINIT_PER_TPC(tex_ecc_total_sec_pipe1_count);
	NVGPU_ECC_COUNTER_DEINIT_PER_TPC(tex_ecc_total_ded_pipe1_count);
	NVGPU_ECC_COUNTER_DEINIT_PER_TPC(tex_unique_ecc_sec_pipe1_count);
	NVGPU_ECC_COUNTER_DEINIT_PER_TPC(tex_unique_ecc_ded_pipe1_count);
}

void gp10b_gr_ecc_deinit(struct gk20a *g)
{
	gp10b_ecc_deinit_tpc_sm(g);

	gp10b_ecc_deinit_tpc_tex(g);
}
