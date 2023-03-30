/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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
#include <os/posix/os_posix.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/gr_falcon.h>

#include <nvgpu/hw/gv11b/hw_gr_gv11b.h>

#include "common/gr/gr_priv.h"
#include "common/gr/gr_falcon_priv.h"
#include "common/acr/acr_priv.h"

#include "hal/gr/falcon/gr_falcon_gm20b.h"

#include "../nvgpu-gr.h"
#include "nvgpu-gr-falcon-gm20b.h"
#include "nvgpu-gr-falcon.h"

struct gr_gops_falcon_orgs {
	void (*bind_instblk)(struct gk20a *g,
			     struct nvgpu_mem *mem, u64 inst_ptr);
	int (*load_ctxsw_ucode)(struct gk20a *g,
				struct nvgpu_gr_falcon *falcon);
	int (*init_ctx_state)(struct gk20a *g,
				struct nvgpu_gr_falcon_query_sizes *sizes);
	void (*dump_stats)(struct gk20a *g);
};


static struct nvgpu_gr_falcon *unit_gr_falcon;
static struct gr_gops_falcon_orgs gr_falcon_gops;

static void test_gr_falcon_bind_instblk(struct gk20a *g,
				struct nvgpu_mem *mem, u64 inst_ptr)
{
	/* Do nothing */
}

static int gr_falcon_stub_init_ctx_state(struct gk20a *g,
				  struct nvgpu_gr_falcon_query_sizes *sizes)
{
	/* error return */
	return -EINVAL;
}

static int gr_falcon_stub_hs_acr(struct gk20a *g, struct nvgpu_acr *acr)
{
	return 0;
}

static void gr_test_falcon_dump_stats(struct gk20a *g)
{
	/* Do nothing */
}

static void gr_falcon_save_gops(struct gk20a *g)
{
	gr_falcon_gops.load_ctxsw_ucode =
				g->ops.gr.falcon.load_ctxsw_ucode;
	gr_falcon_gops.bind_instblk = g->ops.gr.falcon.bind_instblk;
	gr_falcon_gops.init_ctx_state = g->ops.gr.falcon.init_ctx_state;
	gr_falcon_gops.dump_stats = g->ops.gr.falcon.dump_stats;
}

static void gr_falcon_stub_gops(struct gk20a *g)
{
	g->ops.gr.falcon.load_ctxsw_ucode =
				nvgpu_gr_falcon_load_secure_ctxsw_ucode;
	g->ops.gr.falcon.bind_instblk = test_gr_falcon_bind_instblk;
	g->ops.gr.falcon.dump_stats = gr_test_falcon_dump_stats;
}

int test_gr_falcon_init(struct unit_module *m,
			struct gk20a *g, void *args)
{
	int err = 0;
	u32 fecs_base, ctx_state_rev_id;
	u32 gpccs_base, gpccs_start_offset;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	/* Allocate and Initialize GR */
	err = test_gr_init_setup_ready(m, g, args);
	if (err != 0) {
		unit_return_fail(m, "gr init setup failed\n");
	}

	/* set up test specific HALs */
	gr_falcon_save_gops(g);
	gr_falcon_stub_gops(g);

	/* Fail - kmem alloc */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	unit_gr_falcon = nvgpu_gr_falcon_init_support(g);
	if (unit_gr_falcon != 0) {
		unit_return_fail(m, "nvgpu_gr_falcon_init_support failed\n");
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	unit_gr_falcon = nvgpu_gr_falcon_init_support(g);
	if (unit_gr_falcon == NULL) {
		unit_return_fail(m, "nvgpu_gr_falcon_init_support failed\n");
	}

	fecs_base = g->ops.gr.falcon.fecs_base_addr();
	if (fecs_base == 0) {
		unit_return_fail(m, "Get fecs_base failed\n");
	}

	gpccs_base = g->ops.gr.falcon.gpccs_base_addr();
	if (gpccs_base == 0) {
		unit_return_fail(m, "Get gpccs_base failed\n");
	}

	gpccs_start_offset = g->ops.gr.falcon.get_gpccs_start_reg_offset();
	if (gpccs_start_offset == 0) {
		unit_return_fail(m, "Get gpccs_base start failed\n");
	}

	ctx_state_rev_id =
		g->ops.gr.falcon.get_fecs_ctx_state_store_major_rev_id(g);
	if (ctx_state_rev_id != 0) {
		unit_return_fail(m, "ctx_state rev_id failed\n");
	}

	return UNIT_SUCCESS;
}

static int gr_falcon_bind_instblk(struct unit_module *m, struct gk20a *g)
{
	int err;
	struct nvgpu_ctxsw_ucode_info *ucode_info =
					&unit_gr_falcon->ctxsw_ucode_info;
	struct nvgpu_mem *mem;
	struct nvgpu_posix_fault_inj *nvgpu_readl_fi =
				nvgpu_readl_get_fault_injection();

	g->ops.gr.falcon.bind_instblk = gr_falcon_gops.bind_instblk;

	if (g->ops.gr.falcon.bind_instblk == NULL) {
		return 0;
	}

	/* Generate expected bug with hi32 instr value */
	mem = &ucode_info->inst_blk_desc;
	mem->cpu_va = (void *)0xFFFFFFFFFFFFFFFF;
	if (!EXPECT_BUG(nvgpu_gr_falcon_init_ctxsw(g, unit_gr_falcon))) {
		unit_return_fail(m,
			"falcon_init_ctxsw test1 failed\n");
	}
	mem->cpu_va = NULL;

	err = nvgpu_alloc_inst_block(g, &ucode_info->inst_blk_desc);
	if (err != 0)
		return UNIT_FAIL;

	err = nvgpu_gr_falcon_init_ctxsw(g, unit_gr_falcon);
	if (err != 0) {
		unit_return_fail(m,
			"falcon_init_ctxsw test2 failed\n");
	}

	/* Fail fecs_arb_ctx_cmd_r() readl for branch coverage */
	nvgpu_posix_enable_fault_injection(nvgpu_readl_fi, true, 1);
	err = nvgpu_gr_falcon_init_ctxsw(g, unit_gr_falcon);
	if (err == 0) {
		unit_return_fail(m,
			"falcon_init_ctxsw test3 failed\n");
	}
	nvgpu_posix_enable_fault_injection(nvgpu_readl_fi, false, 0);

	/* Set ctxsw_status_busy for branch coverage */
	nvgpu_posix_io_writel_reg_space(g, gr_fecs_ctxsw_status_1_r(),
						(0x1U << 12U));
	err = nvgpu_gr_falcon_init_ctxsw(g, unit_gr_falcon);
	if (err != 0) {
		unit_return_fail(m,
			"falcon_init_ctxsw test4 failed\n");
	}
	nvgpu_posix_io_writel_reg_space(g, gr_fecs_ctxsw_status_1_r(), 0);

	nvgpu_free_inst_block(g, &ucode_info->inst_blk_desc);
	return err;
}

int test_gr_falcon_init_ctxsw(struct unit_module *m,
				struct gk20a *g, void *args)
{
	int err = 0;
	struct nvgpu_acr  gr_falcon_acr_test;

	/* Test secure gpccs */
	err = nvgpu_gr_falcon_init_ctxsw(g, unit_gr_falcon);
	if (err) {
		unit_return_fail(m, "nvgpu_gr_falcon_init_ctxsw failed\n");
	}
	nvgpu_set_enabled(g, NVGPU_SEC_SECUREGPCCS, true);

	/* Test for recovery to fail */
	err = nvgpu_gr_falcon_init_ctxsw(g, unit_gr_falcon);
	/* Recovey expected to fail */
	if (err == 0) {
		unit_return_fail(m,
			"falcon_init_ctxsw secure recovery failed\n");
	}

	/* Test for recovery to pass */
	g->acr = &gr_falcon_acr_test;
	g->acr->bootstrap_hs_acr = gr_falcon_stub_hs_acr;
	err = nvgpu_gr_falcon_init_ctxsw(g, unit_gr_falcon);
	if (err != 0) {
		unit_return_fail(m,
			"falcon_init_ctxsw secure recovery failed\n");
	}

	/* Test for falcon bind instblk */
	err = gr_falcon_bind_instblk(m, g);
	if (err != 0) {
		unit_return_fail(m,
			"falcon_bind_instblk failed\n");
	}

	return UNIT_SUCCESS;
}

int test_gr_falcon_query_test(struct unit_module *m,
				struct gk20a *g, void *args)
{
#ifdef CONFIG_NVGPU_ENGINE_RESET
	struct nvgpu_mutex *fecs_mutex =
		nvgpu_gr_falcon_get_fecs_mutex(unit_gr_falcon);
#endif
	struct nvgpu_ctxsw_ucode_segments *fecs =
		nvgpu_gr_falcon_get_fecs_ucode_segments(unit_gr_falcon);
	struct nvgpu_ctxsw_ucode_segments *gpccs =
		nvgpu_gr_falcon_get_gpccs_ucode_segments(unit_gr_falcon);
	void *cpu_va = nvgpu_gr_falcon_get_surface_desc_cpu_va(unit_gr_falcon);

#ifdef CONFIG_NVGPU_ENGINE_RESET
	if (fecs_mutex == NULL) {
		unit_return_fail(m, "nvgpu_gr_falcon_get_fecs_mutex failed\n");
	}
#endif
	if (fecs == NULL) {
		unit_return_fail(m,
			"nvgpu_gr_falcon_get_fecs_ucode_segments failed\n");
	}

	if (gpccs == NULL) {
		unit_return_fail(m,
			"nvgpu_gr_falcon_get_gpccs_ucode_segments failed\n");
	}

	unit_info(m, "nvgpu_gr_falcon_get_surface_desc_cpu_va %p\n",
								cpu_va);

	return UNIT_SUCCESS;

}

int test_gr_falcon_init_ctx_state(struct unit_module *m,
				struct gk20a *g, void *args)
{
	int err = 0;

	err = nvgpu_gr_falcon_init_ctx_state(g, unit_gr_falcon);
	if (err) {
		unit_return_fail(m, "nvgpu_gr_falcon_init_ctx_state failed\n");
	}

	/* Error injection for failure coverage */
	g->ops.gr.falcon.init_ctx_state = gr_falcon_stub_init_ctx_state;
	err = nvgpu_gr_falcon_init_ctx_state(g, unit_gr_falcon);
	if (err == 0) {
		unit_return_fail(m, "nvgpu_gr_falcon_init_ctx_state failed\n");
	}
	g->ops.gr.falcon.init_ctx_state = gr_falcon_gops.init_ctx_state;

	return UNIT_SUCCESS;
}

int test_gr_falcon_deinit(struct unit_module *m,
				struct gk20a *g, void *args)
{
	int err = 0;

	if (unit_gr_falcon != NULL) {
		nvgpu_gr_falcon_remove_support(g, unit_gr_falcon);
		unit_gr_falcon = NULL;
	}

	/* Call with NULL pointer */
	nvgpu_gr_falcon_remove_support(g, unit_gr_falcon);

	/* Cleanup GR */
	err = test_gr_init_setup_cleanup(m, g, args);
	if (err != 0) {
		unit_return_fail(m, "gr setup cleanup failed\n");
	}

	return UNIT_SUCCESS;
}

int test_gr_falcon_fail_ctxsw_ucode(struct unit_module *m,
			struct gk20a *g, void *args)
{
	int err = 0;
	int i = 0, kmem_fail = 5, dma_fail = 2;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();
	struct nvgpu_posix_fault_inj *dma_fi =
		nvgpu_dma_alloc_get_fault_injection();

	/* Fail - dma alloc */
	for (i = 0; i < dma_fail; i++) {
		nvgpu_posix_enable_fault_injection(dma_fi, true, i);
		err = nvgpu_gr_falcon_init_ctxsw_ucode(g, unit_gr_falcon);
		if (err == 0) {
			unit_return_fail(m, "nvgpu_gr_falcon_init_support failed\n");
		}
		nvgpu_posix_enable_fault_injection(dma_fi, false, 0);
	}

	/* Fail - kmem alloc */
	for (i = 0; i < kmem_fail; i++) {
		nvgpu_posix_enable_fault_injection(kmem_fi, true, i);
		err = nvgpu_gr_falcon_init_ctxsw_ucode(g, unit_gr_falcon);
		if (err == 0) {
			unit_return_fail(m, "nvgpu_gr_falcon_init_support failed\n");
		}
		nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	}

	return UNIT_SUCCESS;
}

struct unit_module_test nvgpu_gr_falcon_tests[] = {
	UNIT_TEST(gr_falcon_init, test_gr_falcon_init, NULL, 0),
	UNIT_TEST(gr_falcon_init_ctxsw, test_gr_falcon_init_ctxsw, NULL, 0),
	UNIT_TEST(gr_falcon_query_test,
			test_gr_falcon_query_test, NULL, 0),
	UNIT_TEST(gr_falcon_init_ctx_state,
			test_gr_falcon_init_ctx_state, NULL, 0),
	UNIT_TEST(gr_falcon_fail_ctxsw_ucode,
			test_gr_falcon_fail_ctxsw_ucode, NULL, 0),
	UNIT_TEST(gr_falcon_gm20b_ctrl_ctxsw,
			test_gr_falcon_gm20b_ctrl_ctxsw, NULL, 0),
	UNIT_TEST(gr_falcon_deinit, test_gr_falcon_deinit, NULL, 0),
};


UNIT_MODULE(nvgpu_gr_falcon, nvgpu_gr_falcon_tests, UNIT_PRIO_NVGPU_TEST);
