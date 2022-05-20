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

#include <unit/unit.h>
#include <unit/io.h>
#include <nvgpu/posix/io.h>
#include <nvgpu/posix/posix-fault-injection.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/fbp.h>

#include <hal/top/top_gm20b.h>
#include <hal/fuse/fuse_gm20b.h>

#include <nvgpu/hw/gv11b/hw_top_gv11b.h>
#include <nvgpu/hw/gv11b/hw_fuse_gv11b.h>
#include "nvgpu-fbp.h"

/*
 * Write callback.
 */
static void writel_access_reg_fn(struct gk20a *g,
					struct nvgpu_reg_access *access)
{
	nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
}

/*
 * Read callback.
 */
static void readl_access_reg_fn(struct gk20a *g,
				struct nvgpu_reg_access *access)
{
	access->value = nvgpu_posix_io_readl_reg_space(g, access->addr);
}

static struct nvgpu_posix_io_callbacks test_reg_callbacks = {
	/* Write APIs all can use the same accessor. */
	.writel          = writel_access_reg_fn,
	.writel_check    = writel_access_reg_fn,
	.bar1_writel     = writel_access_reg_fn,
	.usermode_writel = writel_access_reg_fn,

	/* Likewise for the read APIs. */
	.__readl         = readl_access_reg_fn,
	.readl           = readl_access_reg_fn,
	.bar1_readl      = readl_access_reg_fn,
};

int test_fbp_setup(struct unit_module *m, struct gk20a *g, void *args)
{
	/* Init HAL */
	g->ops.top.get_max_fbps_count = gm20b_top_get_max_fbps_count;
	g->ops.fuse.fuse_status_opt_fbp = gm20b_fuse_status_opt_fbp;

	/* Map register space for FUSE_STATUS_OPT_FBP */
	if (nvgpu_posix_io_add_reg_space(g, fuse_status_opt_fbp_r(), 0x4)
									!= 0) {
		unit_err(m, "%s: failed to register NV_FUSE space\n", __func__);
		return UNIT_FAIL;
	}

	/* Map register space for TOP_SCAL_NUM_FBPS */
	if (nvgpu_posix_io_add_reg_space(g, top_num_fbps_r(), 0x4) != 0) {
		unit_err(m, "%s: failed to register NV_TOP space\n", __func__);
		return UNIT_FAIL;
	}

	(void)nvgpu_posix_register_io(g, &test_reg_callbacks);

	return UNIT_SUCCESS;
}

int test_fbp_free_reg_space(struct unit_module *m, struct gk20a *g, void *args)
{
	/* Free register space */
	nvgpu_posix_io_delete_reg_space(g, fuse_status_opt_fbp_r());
	nvgpu_posix_io_delete_reg_space(g, top_num_fbps_r());

	return UNIT_SUCCESS;
}

int test_fbp_init_and_query(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_SUCCESS;
	int val = 0;
	u32 fbp_en_mask;
	u32 max_fbps_count;
	struct nvgpu_fbp *fbp = g->fbp;
	struct nvgpu_posix_fault_inj *kmem_fi =
					nvgpu_kmem_get_fault_injection();

	/* First, cover the memory allocation failure path. */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);

	/* Call fbp_init_support and confirm it returns -ENOMEM. */
	val = nvgpu_fbp_init_support(g);
	if (val != -ENOMEM) {
		unit_err(m,
		"%s: fbp_init_support did not fail due to memory allocation.\n",
		 __func__);
		return UNIT_FAIL;
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	/* Initialize the FBP floorsweeping status in fuse to 0xE1. */
	fbp_en_mask = 0xE1U;
	nvgpu_posix_io_writel_reg_space(g, fuse_status_opt_fbp_r(),
								fbp_en_mask);

	/* Initialize the  maximum number of FBPs to 8. */
	max_fbps_count = 8U;
	nvgpu_posix_io_writel_reg_space(g, top_num_fbps_r(), max_fbps_count);

	/* Call fbp_init_support to initialize g->fbp */
	val = nvgpu_fbp_init_support(g);
	if (val != 0) {
		unit_err(m, "%s: Failed to initialize g->fbp.\n", __func__);
		return UNIT_FAIL;
	}

	fbp = g->fbp;

	/* Check if the max_fbps_count is read correctly. */
	max_fbps_count = nvgpu_fbp_get_max_fbps_count(fbp);
	if (max_fbps_count != 8U) {
		unit_err(m, "%s: fbp->max_fbps_count is incorrect.\n",
								__func__);
		return UNIT_FAIL;
	}

	/* Check if the FBP en_mask is calculated correctly.
	 * Note: 0:enable and 1:disable in value read from fuse.
	 * so we've to flip the bits and also set unused bits to zero.
	 */
	fbp_en_mask = nvgpu_fbp_get_fbp_en_mask(fbp);
	if (fbp_en_mask != 0x1EU) {
		unit_err(m, "%s: fbp->fbp_en_mask is incorrect.\n", __func__);
		return UNIT_FAIL;
	}

	/* Initialize the FBP floorsweeping status in fuse to 5.
	 * Use different value than above to check if init occurs once.
	 */
	max_fbps_count = 5U;
	nvgpu_posix_io_writel_reg_space(g, top_num_fbps_r(), max_fbps_count);

	/* Call fbp_init_support again to ensure the initialization is
	 * done once.
	 */
	val = nvgpu_fbp_init_support(g);
	if (val != 0) {
		unit_err(m, "%s: Failed to initialize g->fbp.\n", __func__);
		return UNIT_FAIL;
	}

	/* Check if the max_fbps_count is NOT set to 5. */
	max_fbps_count = nvgpu_fbp_get_max_fbps_count(fbp);
	if (max_fbps_count == 5U) {
		unit_err(m, "%s: g->fbp initialized again.\n", __func__);
		return UNIT_FAIL;
	}

	return ret;
}

int test_fbp_remove_support(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_SUCCESS;

	/* Confirm if g->fbp != NULL before calling fbp_remov_support API. */
	if (g->fbp == NULL) {
		unit_err(m, "%s: g->fbp is uninitialized.\n", __func__);
		return UNIT_FAIL;
	}

	/* Call fbp_remove_support to cleanup the saved FBP data */
	nvgpu_fbp_remove_support(g);

	/* Confirm if g->fbp == NULL after cleanup. */
	if (g->fbp != NULL) {
		unit_err(m, "%s: g->fbp is not cleaned up.\n", __func__);
		return UNIT_FAIL;
	}

	/*
	 * Call fbp_remove_support with fbp pointer set to NULL for branch
	 * coverage.
	 */
	nvgpu_fbp_remove_support(g);

	return ret;
}

struct unit_module_test fbp_tests[] = {
	UNIT_TEST(fbp_setup,	          test_fbp_setup,            NULL, 0),
	UNIT_TEST(fbp_init_and_query,	  test_fbp_init_and_query,   NULL, 0),
	UNIT_TEST(fbp_remove_support,	  test_fbp_remove_support,   NULL, 0),
	UNIT_TEST(fbp_free_reg_space,	  test_fbp_free_reg_space,   NULL, 0),
};

UNIT_MODULE(fbp, fbp_tests, UNIT_PRIO_NVGPU_TEST);
