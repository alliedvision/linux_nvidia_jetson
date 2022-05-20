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
#include <nvgpu/gk20a.h>
#include <nvgpu/dma.h>
#include <nvgpu/class.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr_utils.h>
#include <nvgpu/gr/subctx.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/gr/obj_ctx.h>

#include <nvgpu/posix/posix-fault-injection.h>
#include <nvgpu/posix/dma.h>

#include "common/gr/gr_priv.h"
#include "common/gr/ctx_priv.h"
#include "common/gr/obj_ctx_priv.h"
#include "common/gr/gr_config_priv.h"

#include "../nvgpu-gr.h"
#include "nvgpu-gr-obj-ctx.h"

#define DUMMY_SIZE	0xF0U

static int fe_pwr_mode_count;
static int test_fe_pwr_mode_force_on(struct gk20a *g, bool force_on)
{
	if (fe_pwr_mode_count == 0) {
		return -1;
	} else {
		fe_pwr_mode_count--;
		return 0;
	}
}

static int test_l2_flush(struct gk20a *g, bool flag)
{
	return 0;
}

static int test_init_sm_id_table(struct gk20a *g,
	struct nvgpu_gr_config *gr_config)
{
	return -1;
}

static int ctrl_ctxsw_count;
static int test_falcon_ctrl_ctxsw(struct gk20a *g, u32 fecs_method,
	u32 data, u32 *ret_val)
{
	if (ctrl_ctxsw_count == 0) {
		return -1;
	} else {
		ctrl_ctxsw_count--;
		return 0;
	}
}

static int gr_wait_idle_count;
static int test_gr_wait_idle(struct gk20a *g)
{
	if (gr_wait_idle_count == 0) {
		gr_wait_idle_count--;
		return -1;
	} else {
		gr_wait_idle_count--;
		return 0;
	}
}

static int load_sw_bundle_count;
static int test_load_sw_bundle(struct gk20a *g,
	struct netlist_av_list *sw_bundle_init)
{
	if (load_sw_bundle_count == 0) {
		return -1;
	} else {
		load_sw_bundle_count--;
		return 0;
	}
}

int test_gr_obj_ctx_error_injection(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int err;
	struct nvgpu_gr_obj_ctx_golden_image *golden_image;
	struct vm_gk20a *vm;
	struct nvgpu_gr_ctx_desc *desc;
	struct nvgpu_gr_global_ctx_buffer_desc *global_desc;
	struct nvgpu_gr_ctx *gr_ctx = NULL;
	struct nvgpu_gr_subctx *subctx = NULL;
	struct nvgpu_mem inst_block;
	struct nvgpu_gr_config *config = nvgpu_gr_get_config_ptr(g);
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();
	struct nvgpu_posix_fault_inj *golden_ctx_verif_fi =
		nvgpu_golden_ctx_verif_get_fault_injection();
	struct nvgpu_posix_fault_inj *local_golden_image_fi =
		nvgpu_local_golden_image_get_fault_injection();
	int (*init_sm_id_table_tmp)(struct gk20a *g,
		struct nvgpu_gr_config *config);

	/* Inject allocation failures and initialize obj_ctx, should fail */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	err = nvgpu_gr_obj_ctx_init(g, &golden_image, DUMMY_SIZE);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	g->ops.mm.cache.l2_flush = test_l2_flush;

	/* Disable error injection and initialize obj_ctx, should pass */
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	err = nvgpu_gr_obj_ctx_init(g, &golden_image, DUMMY_SIZE);
	if (err != 0) {
		unit_return_fail(m, "failed to init obj_ctx");
	}

	/* Setup VM */
	vm = nvgpu_vm_init(g, SZ_4K, SZ_4K << 10,
		nvgpu_safe_sub_u64(1ULL << 37, SZ_4K << 10),
		(1ULL << 32), 0ULL,
		false, false, false, "dummy");
	if (!vm) {
		unit_return_fail(m, "failed to allocate VM");
	}

	/* Allocate inst_block */
	err = nvgpu_dma_alloc(g, DUMMY_SIZE, &inst_block);
	if (err) {
		unit_return_fail(m, "failed to allocate instance block");
	}

	/* Setup graphics context prerequisites, global buffers and subcontext */
	desc = nvgpu_gr_ctx_desc_alloc(g);
	if (!desc) {
		unit_return_fail(m, "failed to allocate memory");
	}

	gr_ctx = nvgpu_alloc_gr_ctx_struct(g);
	if (!gr_ctx) {
		unit_return_fail(m, "failed to allocate memory");
	}

	global_desc = nvgpu_gr_global_ctx_desc_alloc(g);
	if (!global_desc) {
		unit_return_fail(m, "failed to allocate desc");
	}

	nvgpu_gr_global_ctx_set_size(global_desc, NVGPU_GR_GLOBAL_CTX_CIRCULAR,
		DUMMY_SIZE);
	nvgpu_gr_global_ctx_set_size(global_desc, NVGPU_GR_GLOBAL_CTX_PAGEPOOL,
		DUMMY_SIZE);
	nvgpu_gr_global_ctx_set_size(global_desc, NVGPU_GR_GLOBAL_CTX_ATTRIBUTE,
		DUMMY_SIZE);
	nvgpu_gr_global_ctx_set_size(global_desc, NVGPU_GR_GLOBAL_CTX_PRIV_ACCESS_MAP,
		DUMMY_SIZE);

	err = nvgpu_gr_global_ctx_buffer_alloc(g, global_desc);
	if (err != 0) {
		unit_return_fail(m, "failed to allocate global buffers");
	}

	subctx = nvgpu_gr_subctx_alloc(g, vm);
	if (!subctx) {
		unit_return_fail(m, "failed to allocate subcontext");
	}

	/* Fail gr_ctx allocation */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	err = nvgpu_gr_obj_ctx_alloc(g, golden_image, global_desc, desc,
			config, gr_ctx, subctx, vm, &inst_block,
			VOLTA_COMPUTE_A, 0, false, false);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	/* Fail patch_ctx allocation */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 3);
	err = nvgpu_gr_obj_ctx_alloc(g, golden_image, global_desc, desc,
			config, gr_ctx, subctx, vm, &inst_block,
			VOLTA_COMPUTE_A, 0, false, false);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	/* Fail circular buffer mapping */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 8);
	err = nvgpu_gr_obj_ctx_alloc(g, golden_image, global_desc, desc,
			config, gr_ctx, subctx, vm, &inst_block,
			VOLTA_COMPUTE_A, 0, false, false);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	/* Fail first call to gops.gr.init.fe_pwr_mode_force_on */
	g->ops.gr.init.fe_pwr_mode_force_on = test_fe_pwr_mode_force_on;
	fe_pwr_mode_count = 0;
	err = nvgpu_gr_obj_ctx_alloc(g, golden_image, global_desc, desc,
			config, gr_ctx, subctx, vm, &inst_block,
			VOLTA_COMPUTE_A, 0, false, false);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	/* Fail second call to gops.gr.init.fe_pwr_mode_force_on */
	fe_pwr_mode_count = 1;
	err = nvgpu_gr_obj_ctx_alloc(g, golden_image, global_desc, desc,
			config, gr_ctx, subctx, vm, &inst_block,
			VOLTA_COMPUTE_A, 0, false, false);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	/* Re-enable gops.gr.init.fe_pwr_mode_force_on */
	fe_pwr_mode_count = -1;

	/* Fail nvgpu_gr_fs_state_init() */
	init_sm_id_table_tmp = g->ops.gr.config.init_sm_id_table;
	g->ops.gr.config.init_sm_id_table = test_init_sm_id_table;
	g->ops.gr.falcon.ctrl_ctxsw = test_falcon_ctrl_ctxsw;
	ctrl_ctxsw_count = -1;
	err = nvgpu_gr_obj_ctx_alloc(g, golden_image, global_desc, desc,
			config, gr_ctx, subctx, vm, &inst_block,
			VOLTA_COMPUTE_A, 0, false, false);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	/* restore gops.gr.config.init_sm_id_table */
	g->ops.gr.config.init_sm_id_table = init_sm_id_table_tmp;

	/* Fail 3rd gops.gr.init.wait_idle */
	g->ops.gr.init.wait_idle = test_gr_wait_idle;
	gr_wait_idle_count = 2;
	err = nvgpu_gr_obj_ctx_alloc(g, golden_image, global_desc, desc,
			config, gr_ctx, subctx, vm, &inst_block,
			VOLTA_COMPUTE_A, 0, false, false);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	/* Pass gops.gr.init.wait_idle */
	gr_wait_idle_count = -1;

	/* Fail gops.gr.init.load_sw_bundle_init */
	g->ops.gr.init.load_sw_bundle_init = test_load_sw_bundle;
	load_sw_bundle_count = 0;
	err = nvgpu_gr_obj_ctx_alloc(g, golden_image, global_desc, desc,
			config, gr_ctx, subctx, vm, &inst_block,
			VOLTA_COMPUTE_A, 0, false, false);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	/* Fail gops.gr.init.load_sw_veid_bundle */
	g->ops.gr.init.load_sw_veid_bundle = test_load_sw_bundle;
	load_sw_bundle_count = 1;
	err = nvgpu_gr_obj_ctx_alloc(g, golden_image, global_desc, desc,
			config, gr_ctx, subctx, vm, &inst_block,
			VOLTA_COMPUTE_A, 0, false, false);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	/* Pass load sw bundle */
	load_sw_bundle_count = -1;

	/* gops.gr.init.load_sw_veid_bundle could be NULL */
	g->ops.gr.init.load_sw_veid_bundle = NULL;
#ifdef CONFIG_NVGPU_GR_GOLDEN_CTX_VERIFICATION
	/* gops.gr.init.restore_stats_counter_bundle_data could be NULL */
	g->ops.gr.init.restore_stats_counter_bundle_data = NULL;
#endif

	/* Fail 4th gops.gr.init.wait_idle */
	g->ops.gr.init.wait_idle = test_gr_wait_idle;
	gr_wait_idle_count = 4;
	err = nvgpu_gr_obj_ctx_alloc(g, golden_image, global_desc, desc,
			config, gr_ctx, subctx, vm, &inst_block,
			VOLTA_COMPUTE_A, 0, false, false);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	/* Disable error injection */
	nvgpu_posix_enable_fault_injection(local_golden_image_fi, false, 0);

	/*
	 * Fail first gops.gr.falcon.ctrl_ctxsw in
	 * nvgpu_gr_obj_ctx_save_golden_ctx()
	 */
	ctrl_ctxsw_count = 1;
	err = nvgpu_gr_obj_ctx_alloc(g, golden_image, global_desc, desc,
			config, gr_ctx, subctx, vm, &inst_block,
			VOLTA_COMPUTE_A, 0, false, false);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	/*
	 * Fail second gops.gr.falcon.ctrl_ctxsw in
	 * nvgpu_gr_obj_ctx_save_golden_ctx()
	 */
	ctrl_ctxsw_count = 2;
	err = nvgpu_gr_obj_ctx_alloc(g, golden_image, global_desc, desc,
			config, gr_ctx, subctx, vm, &inst_block,
			VOLTA_COMPUTE_A, 0, false, false);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	/* Pass gops.gr.falcon.ctrl_ctxsw */
	ctrl_ctxsw_count = -1;

	/* Fail golden context verification */
	nvgpu_posix_enable_fault_injection(golden_ctx_verif_fi, true, 0);
	err = nvgpu_gr_obj_ctx_alloc(g, golden_image, global_desc, desc,
			config, gr_ctx, subctx, vm, &inst_block,
			VOLTA_COMPUTE_A, 0, false, false);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	/* Pass golden context verification */
	nvgpu_posix_enable_fault_injection(golden_ctx_verif_fi, false, 0);

	/* Finally, successful obj_ctx allocation */
	err = nvgpu_gr_obj_ctx_alloc(g, golden_image, global_desc, desc,
			config, gr_ctx, subctx, vm, &inst_block,
			VOLTA_COMPUTE_A, 0, false, false);
	if (err != 0) {
		unit_return_fail(m, "failed to allocate obj_ctx");
	}

	/* Check if golden image is ready */
	if (!nvgpu_gr_obj_ctx_is_golden_image_ready(golden_image)) {
		unit_return_fail(m, "golden image is not initialzed");
	}

	/* Reallocation with golden image already created */
	err = nvgpu_gr_obj_ctx_alloc(g, golden_image, global_desc, desc,
			config, gr_ctx, subctx, vm, &inst_block,
			VOLTA_COMPUTE_A, 0, false, false);
	if (err != 0) {
		unit_return_fail(m, "failed to re-allocate obj_ctx");
	}

	/* Set preemption mode with invalid compute class */
	err = nvgpu_gr_obj_ctx_set_ctxsw_preemption_mode(g, config, desc, gr_ctx, vm,
		VOLTA_DMA_COPY_A, 0, NVGPU_PREEMPTION_MODE_COMPUTE_CTA);
	if (err == 0) {
		unit_return_fail(m, "unexpected success");
	}

	/* Cleanup */
	nvgpu_gr_subctx_free(g, subctx, vm);
	nvgpu_gr_ctx_free_patch_ctx(g, vm, gr_ctx);
	nvgpu_gr_ctx_free(g, gr_ctx, global_desc, vm);
	nvgpu_free_gr_ctx_struct(g, gr_ctx);
	nvgpu_gr_ctx_desc_free(g, desc);
	nvgpu_gr_obj_ctx_deinit(g, golden_image);
	nvgpu_vm_put(vm);

	return UNIT_SUCCESS;
}

struct unit_module_test nvgpu_gr_obj_ctx_tests[] = {
	UNIT_TEST(gr_obj_ctx_setup, test_gr_init_setup_ready, NULL, 0),
	UNIT_TEST(gr_obj_ctx_alloc_errors, test_gr_obj_ctx_error_injection, NULL, 2),
	UNIT_TEST(gr_obj_ctx_cleanup, test_gr_init_setup_cleanup, NULL, 0),
};

UNIT_MODULE(nvgpu_gr_obj_ctx, nvgpu_gr_obj_ctx_tests, UNIT_PRIO_NVGPU_TEST);
