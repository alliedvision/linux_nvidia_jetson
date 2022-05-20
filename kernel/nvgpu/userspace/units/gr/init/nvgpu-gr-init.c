/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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


#include <stdlib.h>

#include <unit/unit.h>
#include <unit/io.h>

#include <nvgpu/posix/io.h>
#include <nvgpu/posix/kmem.h>
#include <nvgpu/posix/dma.h>
#include <nvgpu/posix/posix-fault-injection.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr_falcon.h>
#include <nvgpu/gr/gr_intr.h>

#include <nvgpu/hw/gv11b/hw_fuse_gv11b.h>
#include <nvgpu/hw/gv11b/hw_gr_gv11b.h>

#include "common/gr/gr_falcon_priv.h"
#include "common/gr/gr_priv.h"
#include "common/gr/obj_ctx_priv.h"

#include "../nvgpu-gr.h"
#include "nvgpu-gr-init-hal-gv11b.h"

#define GR_TEST_FUSES_OVERRIDE_DISABLE_TRUE	0x1U
#define GR_TEST_FUSES_OVERRIDE_DISABLE_FALSE	0x0U

#define GR_TEST_FECS_FEATURE_OVERRIDE_ECC		0x00909999
#define GR_TEST_FECS_FEATURE_OVERRIDE_ECC_ONLY		0x00808888
#define GR_TEST_FECS_FEATURE_OVERRIDE_ECC1		0x0000000F
#define GR_TEST_FECS_FEATURE_OVERRIDE_ECC1_ONLY		0x0000000A
#define GR_TEST_FECS_FEATURE_OVERRIDE_ECC1_FAIL1	0x00000002
#define GR_TEST_FECS_FEATURE_OVERRIDE_ECC1_FAIL2	0x0000000B

static struct gpu_ops gr_init_gops;

struct gr_test_init_org_ptrs {
	void (*gr_remove_support)(struct gk20a *g);
	struct nvgpu_gr_global_ctx_buffer_desc *ctx_buffer;
	struct nvgpu_gr_ctx_desc *ctx;
	struct nvgpu_gr_config *config;
	struct nvgpu_gr_obj_ctx_golden_image *golden_image;
	struct nvgpu_netlist_vars *netlist_vars;
};

static struct gr_test_init_org_ptrs gr_test_init_ptrs;
static void gr_test_init_save_gops(struct gk20a *g)
{
	gr_init_gops = g->ops;

	gr_test_init_ptrs.ctx_buffer = g->gr->global_ctx_buffer;
	gr_test_init_ptrs.ctx = g->gr->gr_ctx_desc;
	gr_test_init_ptrs.config = g->gr->config;
	gr_test_init_ptrs.golden_image = g->gr->golden_image;
	gr_test_init_ptrs.netlist_vars = g->netlist_vars;
}

static void gr_test_init_reset_gr_ptrs(struct gk20a *g)
{
	g->gr->global_ctx_buffer = NULL;
	g->gr->gr_ctx_desc = NULL;
	g->gr->config = NULL;
	g->gr->golden_image = NULL;
	g->netlist_vars = NULL;
}

static void gr_test_init_restore_gr_ptrs(struct gk20a *g)
{
	g->gr->global_ctx_buffer = gr_test_init_ptrs.ctx_buffer;
	g->gr->gr_ctx_desc = gr_test_init_ptrs.ctx;
	g->gr->config = gr_test_init_ptrs.config;
	g->gr->golden_image = gr_test_init_ptrs.golden_image;
	g->netlist_vars = gr_test_init_ptrs.netlist_vars;
}

static void gr_test_init_restore_gops(struct gk20a *g)
{
	g->ops = gr_init_gops;
}

static int gr_test_init_load_ctxsw_ucode_fail(struct gk20a *g,
			struct nvgpu_gr_falcon *falcon)
{
	return -EINVAL;
}

static int gr_test_init_load_ctxsw_ucode_pass(struct gk20a *g,
			struct nvgpu_gr_falcon *falcon)
{
	return 0;
}

static int gr_test_init_ctx_state(struct gk20a *g,
		struct nvgpu_gr_falcon_query_sizes *sizes)
{
	return -EINVAL;
}

static int gr_test_init_ctx_state_pass(struct gk20a *g,
		struct nvgpu_gr_falcon_query_sizes *sizes)
{
	return 0;
}

static int gr_test_init_ecc_scrub_reg(struct gk20a *g,
		struct nvgpu_gr_config * gr_config)
{
	return -EINVAL;
}

static int gr_test_init_wait_stub_error(struct gk20a *g)
{
	return -EINVAL;
}


static int gr_init_ecc_fail_alloc(struct gk20a *g)
{
	int err, i, loop = 26;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();
	struct nvgpu_gr_config *save_gr_config = g->gr->config;

	for (i = 0; i < loop; i++) {
		nvgpu_posix_enable_fault_injection(kmem_fi, true, i);
		err = g->ops.gr.ecc.gpc_tpc_ecc_init(g);
		if (err == 0) {
			return UNIT_FAIL;
		}
		nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
		g->ops.ecc.ecc_init_support(g);
	}

	loop = 2;

	for (i = 0; i < loop; i++) {
		nvgpu_posix_enable_fault_injection(kmem_fi, true, i);
		err = g->ops.gr.ecc.fecs_ecc_init(g);
		if (err == 0) {
			return UNIT_FAIL;
		}
		nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
		g->ops.ecc.ecc_init_support(g);
	}

	/* Set gr->config to NULL for branch covergae */
	g->gr->config = NULL;
	g->ecc.initialized = true;
	g->ops.ecc.ecc_remove_support(g);
	g->ecc.initialized = false;
	g->gr->config = save_gr_config;

	return UNIT_SUCCESS;
}

struct gr_init_ecc_stats {
	u32 fuse_override;
	u32 opt_enable;
	u32 fecs_override0;
	u32 fecs_override1;
};

int test_gr_init_ecc_features(struct unit_module *m,
		struct gk20a *g, void *args)
{
struct gr_init_ecc_stats ecc_stats[] = {
	[0] = {
		.fuse_override = GR_TEST_FUSES_OVERRIDE_DISABLE_TRUE,
		.opt_enable = 0x1,
		.fecs_override0 = 0x0,
		.fecs_override1 = 0x0,
	      },
	[1] = {
		.fuse_override = GR_TEST_FUSES_OVERRIDE_DISABLE_TRUE,
		.opt_enable = 0x0,
		.fecs_override0 = 0x0,
		.fecs_override1 = 0x0,
	      },
	[2] = {
		.fuse_override = GR_TEST_FUSES_OVERRIDE_DISABLE_FALSE,
		.opt_enable = 0x0,
		.fecs_override0 = 0x0,
		.fecs_override1 = 0x0,
	      },
	[3] = {
		.fuse_override = GR_TEST_FUSES_OVERRIDE_DISABLE_FALSE,
		.opt_enable = 0x1,
		.fecs_override0 = 0,
		.fecs_override1 = GR_TEST_FECS_FEATURE_OVERRIDE_ECC1_FAIL1,
	      },
	[4] = {
		.fuse_override = GR_TEST_FUSES_OVERRIDE_DISABLE_FALSE,
		.opt_enable = 0x1,
		.fecs_override0 = 0,
		.fecs_override1 = GR_TEST_FECS_FEATURE_OVERRIDE_ECC1_FAIL2,
	      },
	[5] = {
		.fuse_override = GR_TEST_FUSES_OVERRIDE_DISABLE_FALSE,
		.opt_enable = 0x1,
		.fecs_override0 = GR_TEST_FECS_FEATURE_OVERRIDE_ECC_ONLY,
		.fecs_override1 = GR_TEST_FECS_FEATURE_OVERRIDE_ECC1_ONLY,
	      },
	[6] = {
		.fuse_override = GR_TEST_FUSES_OVERRIDE_DISABLE_FALSE,
		.opt_enable = 0x1,
		.fecs_override0 = GR_TEST_FECS_FEATURE_OVERRIDE_ECC,
		.fecs_override1 = GR_TEST_FECS_FEATURE_OVERRIDE_ECC1,
	      },
};
	int err, i;
	int arry_cnt = sizeof(ecc_stats)/
			sizeof(struct gr_init_ecc_stats);

	for (i = 0; i < arry_cnt; i++) {
		nvgpu_posix_io_writel_reg_space(g,
			fuse_opt_feature_fuses_override_disable_r(),
			ecc_stats[i].fuse_override);

		nvgpu_posix_io_writel_reg_space(g,
			fuse_opt_ecc_en_r(),
			ecc_stats[i].opt_enable);

		/* set fecs ecc override */
		nvgpu_posix_io_writel_reg_space(g,
			gr_fecs_feature_override_ecc_r(),
			ecc_stats[i].fecs_override0);

		nvgpu_posix_io_writel_reg_space(g,
			gr_fecs_feature_override_ecc_1_r(),
			ecc_stats[i].fecs_override1);

		g->ops.gr.ecc.detect(g);

	}

	err = gr_init_ecc_fail_alloc(g);
	if (err != 0) {
		unit_return_fail(m, "stall isr failed\n");
	}

	return UNIT_SUCCESS;
}

static int test_gr_alloc_errors(struct gk20a *g)
{
	int err;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();
	struct nvgpu_gr *local_gr = g->gr;

	/* Free NULL gr */
	g->gr = NULL;
	nvgpu_gr_free(g);

	/* Alloc/free errors for nvgpu_gr_alloc */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	err = nvgpu_gr_alloc(g);
	if (err == 0) {
		return UNIT_FAIL;
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	g->gr = local_gr;

	/* Realloc with valid g->gr */
	err = nvgpu_gr_alloc(g);
	if (err != 0) {
		return UNIT_FAIL;
	}

	gr_test_init_ptrs.gr_remove_support =
					g->gr->remove_support;
	g->gr->remove_support = NULL;
	nvgpu_gr_remove_support(g);
	g->gr->remove_support = gr_test_init_ptrs.gr_remove_support;

	return UNIT_SUCCESS;
}

static int test_gr_init_ctxsw_ucode_alloc_error(struct gk20a *g)
{

	int err;

	g->ops.gr.falcon.load_ctxsw_ucode = gr_test_init_load_ctxsw_ucode_fail;

	err = nvgpu_gr_init_support(g);
	if (err == 0) {
		return UNIT_FAIL;
	}

	g->ops.gr.falcon.load_ctxsw_ucode = gr_test_init_load_ctxsw_ucode_pass;
	return UNIT_SUCCESS;
}

static int test_gr_init_enable_hw_error(struct gk20a *g)
{
	int err;

	/* fail wait idle */
	g->ops.gr.init.wait_idle = gr_test_init_wait_stub_error;
	g->ops.gr.init.wait_empty = gr_test_init_wait_stub_error;
	err = nvgpu_gr_enable_hw(g);
	if (err == 0) {
		return UNIT_FAIL;
	}

	/* fail me scrubbing */
	g->ops.gr.falcon.wait_mem_scrubbing =
			gr_test_init_wait_stub_error;
	err = nvgpu_gr_enable_hw(g);
	if (err == 0) {
		return UNIT_FAIL;
	}

	err = nvgpu_gr_suspend(g);
	if (err == 0) {
		return UNIT_FAIL;
	}

	g->ops.gr.init.wait_empty =
		gr_init_gops.gr.init.wait_empty;
	g->ops.gr.init.wait_idle =
		gr_init_gops.gr.init.wait_idle;
	g->ops.gr.falcon.wait_mem_scrubbing =
			gr_init_gops.gr.falcon.wait_mem_scrubbing;
	return UNIT_SUCCESS;
}

static int test_gr_init_setup_hw_error(struct gk20a *g)
{
	int err;

	g->ops.priv_ring.set_ppriv_timeout_settings = NULL;
	g->ops.gr.init.ecc_scrub_reg = NULL;
	err = nvgpu_gr_init_support(g);
	if (err != 0) {
		return UNIT_FAIL;
	}
	g->ops.gr.init.ecc_scrub_reg = gr_test_init_ecc_scrub_reg;
	g->ops.gr.init.su_coalesce = NULL;
	g->ops.gr.init.lg_coalesce = NULL;

	err = nvgpu_gr_init_support(g);
	if (err == 0) {
		return UNIT_FAIL;
	}
	g->ops.priv_ring.set_ppriv_timeout_settings =
		gr_init_gops.priv_ring.set_ppriv_timeout_settings;
	g->ops.gr.init.ecc_scrub_reg =
		gr_init_gops.gr.init.ecc_scrub_reg;

	return UNIT_SUCCESS;
}

static int test_gr_init_ctx_state_error(struct gk20a *g)
{
	int err;

	g->gr->golden_image->ready = true;
	err = nvgpu_gr_init_support(g);
	if (err != 0) {
		return UNIT_FAIL;
	}

	g->gr->golden_image = NULL;
	g->ops.gr.falcon.init_ctx_state = gr_test_init_ctx_state;
	err = nvgpu_gr_init_support(g);
	if (err == 0) {
		return UNIT_FAIL;
	}

	gr_test_init_restore_gr_ptrs(g);
	g->gr->golden_image->ready = false;
	err = nvgpu_gr_init_support(g);
	if (err == 0) {
		return UNIT_FAIL;
	}

	g->ops.gr.falcon.init_ctx_state =
			gr_init_gops.gr.falcon.init_ctx_state;
	g->gr->golden_image->ready = true;

	return UNIT_SUCCESS;
}

static int test_gr_init_ecc_init_pass(struct gk20a *g)
{
	int err;

	g->ecc.initialized = 1;
	g->gr->falcon->sizes.golden_image_size = 0x10;

	err = nvgpu_gr_init_support(g);
	if (err != 0) {
		return UNIT_FAIL;
	}

	g->ops.gr.config.init_sm_id_table =
		gr_test_init_ecc_scrub_reg;
	g->ops.gr.ecc.gpc_tpc_ecc_init = NULL;
	g->ecc.initialized = 0;
	err = nvgpu_gr_init_support(g);
	if (err == 0) {
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

static int test_gr_init_setup_sw_error(struct gk20a *g)
{
	int err, j;
	int ecc_init = g->ecc.initialized;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();
	struct nvgpu_posix_fault_inj *dma_fi =
		nvgpu_dma_alloc_get_fault_injection();

	err = test_gr_init_ctxsw_ucode_alloc_error(g);
	if (err) {
		return UNIT_FAIL;
	}

	gr_test_init_reset_gr_ptrs(g);
	g->gr->sw_ready = 0;
	g->ops.gr.falcon.init_ctx_state = gr_test_init_ctx_state_pass;
	g->ops.gr.ecc.gpc_tpc_ecc_init = gr_test_init_wait_stub_error;

	for (j = 0; j < 16; j++) {
		if (j > 0) {
			g->ecc.initialized = 1;
			g->gr->falcon->sizes.golden_image_size = 0x10;
		}

		if (j > 14) {
			nvgpu_posix_enable_fault_injection(dma_fi, true, 0);
		} else {
			nvgpu_posix_enable_fault_injection(kmem_fi, true, j);
			g->ecc.initialized = 0;
		}

		err = nvgpu_gr_init_support(g);
		if (err == 0) {
			return UNIT_FAIL;
		}
		nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
		nvgpu_posix_enable_fault_injection(dma_fi, false, 0);
	}

	/* branch test - ecc_init*/
	err = test_gr_init_ecc_init_pass(g);
	if (err) {
		return UNIT_FAIL;
	}

	g->gr->sw_ready = 1;
	g->ecc.initialized = ecc_init;
	gr_test_init_restore_gr_ptrs(g);
	gr_test_init_restore_gops(g);

	return UNIT_SUCCESS;
}

static int test_gr_init_support_alloc_error(struct gk20a *g)
{
	int err;

	/* Fail init_ctx_state */
	err = test_gr_init_ctx_state_error(g);
	if (err != 0) {
		return UNIT_FAIL;
	}

	/* Fail gr_init_setup_hw */
	err = test_gr_init_setup_hw_error(g);
	if (err != 0) {
		return UNIT_FAIL;
	}

	/* enable_hw errors */
	err = test_gr_init_enable_hw_error(g);
	if (err != 0) {
		return UNIT_FAIL;
	}

	/* fail gr_prepare_sw */
	err = test_gr_init_setup_sw_error(g);
	if (err) {
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

static int test_gr_init_support_errors(struct gk20a *g)
{
	int err;

	err = test_gr_init_support_alloc_error(g);
	if (err) {
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

int test_gr_init_error_injections(struct unit_module *m,
				struct gk20a *g, void *args)
{
	int err;

	gr_test_init_save_gops(g);

	/* Alloc/free errors for nvgpu_gr_alloc */
	err = test_gr_alloc_errors(g);
	if (err != 0) {
		unit_return_fail(m, "test_gr_alloc failed\n");
	}

	/* Errors in nvgpu_gr_init_support */
	err = test_gr_init_support_errors(g);
	if (err != 0) {
		unit_return_fail(m, "test_gr_alloc failed\n");
	}

	return UNIT_SUCCESS;
}

struct unit_module_test nvgpu_gr_init_tests[] = {
	UNIT_TEST(gr_init_setup, test_gr_init_setup, NULL, 0),
	UNIT_TEST(gr_init_prepare, test_gr_init_prepare, NULL, 0),
	UNIT_TEST(gr_init_support, test_gr_init_support, NULL, 0),
	UNIT_TEST(gr_init_hal_error_injection, test_gr_init_hal_error_injection, NULL, 0),
	UNIT_TEST(gr_init_hal_wait_empty, test_gr_init_hal_wait_empty, NULL, 0),
	UNIT_TEST(gr_init_hal_wait_idle, test_gr_init_hal_wait_idle, NULL, 0),
	UNIT_TEST(gr_init_hal_wait_fe_idle, test_gr_init_hal_wait_fe_idle, NULL, 0),
	UNIT_TEST(gr_init_hal_fe_pwr_mode, test_gr_init_hal_fe_pwr_mode, NULL, 0),
	UNIT_TEST(gr_init_hal_ecc_scrub_reg, test_gr_init_hal_ecc_scrub_reg, NULL, 0),
	UNIT_TEST(gr_init_hal_config_error_injection, test_gr_init_hal_config_error_injection, NULL, 2),
	UNIT_TEST(gr_suspend, test_gr_suspend, NULL, 0),
	UNIT_TEST(gr_ecc_features, test_gr_init_ecc_features, NULL, 0),
	UNIT_TEST(gr_init_error_injections, test_gr_init_error_injections, NULL, 2),
	UNIT_TEST(gr_remove_support, test_gr_remove_support, NULL, 0),
	UNIT_TEST(gr_remove_setup, test_gr_remove_setup, NULL, 0),
};

UNIT_MODULE(nvgpu_gr_init, nvgpu_gr_init_tests, UNIT_PRIO_NVGPU_TEST);
