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
#include <nvgpu/io.h>
#include "hal/fb/intr/fb_intr_gv11b.h"
#include <nvgpu/hw/gv11b/hw_fb_gv11b.h>
#include <nvgpu/hw/gv11b/hw_mc_gv11b.h>

#include "fb_fusa.h"

static bool intercept_mmu_invalidate;
static u32 intercept_fb_mmu_ctrl_r;

void helper_intercept_mmu_write(u32 val)
{
	intercept_fb_mmu_ctrl_r = val;
	intercept_mmu_invalidate = true;
}

/*
 * Write callback (for all nvgpu_writel calls).
 */
static void writel_access_reg_fn(struct gk20a *g,
	struct nvgpu_reg_access *access)
{
	if (intercept_mmu_invalidate &&
		(access->addr == fb_mmu_invalidate_pdb_r())) {
		intercept_mmu_invalidate = false;
		nvgpu_writel(g, fb_mmu_ctrl_r(), intercept_fb_mmu_ctrl_r);
	}
	nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
	nvgpu_posix_io_record_access(g, access);
}

/*
 * Read callback, similar to the write callback above.
 */
static void readl_access_reg_fn(struct gk20a *g,
	struct nvgpu_reg_access *access)
{
	access->value = nvgpu_posix_io_readl_reg_space(g, access->addr);
}

/*
 * Define all the callbacks to be used during the test. Typically all
 * write operations use the same callback, likewise for all read operations.
 */
static struct nvgpu_posix_io_callbacks fb_callbacks = {
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

static int fb_gv11b_init(struct unit_module *m, struct gk20a *g, void *args)
{
	nvgpu_posix_register_io(g, &fb_callbacks);

	/* Register space: FB */
	if (nvgpu_posix_io_add_reg_space(g, fb_niso_intr_r(), SZ_4K) != 0) {
		unit_return_fail(m, "nvgpu_posix_io_add_reg_space failed FB\n");
	}

	/* Register space: MC_INTR */
	if (nvgpu_posix_io_add_reg_space(g, mc_intr_r(0), SZ_1K) != 0) {
		unit_return_fail(m, "nvgpu_posix_io_add_reg_space failed MC\n");
	}

	/* Register space: HSHUB */
	if (nvgpu_posix_io_add_reg_space(g, fb_hshub_num_active_ltcs_r(),
		SZ_256) != 0) {

		unit_return_fail(m,
			"nvgpu_posix_io_add_reg_space failed HSHUB\n");
	}

	/* Register space: FBHUB */
	if (nvgpu_posix_io_add_reg_space(g, fb_fbhub_num_active_ltcs_r(),
		SZ_256) != 0) {

		unit_return_fail(m,
			"nvgpu_posix_io_add_reg_space failed FBHUB\n");
	}

	return UNIT_SUCCESS;
}

static int fb_gv11b_cleanup(struct unit_module *m, struct gk20a *g, void *args)
{
	/* Unregister space: FB */
	nvgpu_posix_io_delete_reg_space(g, fb_niso_intr_r());
	nvgpu_posix_io_delete_reg_space(g, mc_intr_r(0));
	nvgpu_posix_io_delete_reg_space(g, fb_hshub_num_active_ltcs_r());
	nvgpu_posix_io_delete_reg_space(g, fb_fbhub_num_active_ltcs_r());

	return UNIT_SUCCESS;
}

struct unit_module_test fb_tests[] = {
	UNIT_TEST(fb_gv11b_init, fb_gv11b_init, NULL, 0),
	UNIT_TEST(fb_gv11b_init_test, fb_gv11b_init_test, NULL, 0),
	UNIT_TEST(fb_gm20b_tlb_invalidate_test, fb_gm20b_tlb_invalidate_test,
		NULL, 0),
	UNIT_TEST(fb_gm20b_mmu_ctrl_test, fb_gm20b_mmu_ctrl_test, NULL, 0),
	UNIT_TEST(fb_mmu_fault_gv11b_init_test, fb_mmu_fault_gv11b_init_test,
		NULL, 0),
	UNIT_TEST(fb_mmu_fault_gv11b_buffer_test,
		fb_mmu_fault_gv11b_buffer_test, NULL, 0),
	UNIT_TEST(fb_mmu_fault_gv11b_snap_reg, fb_mmu_fault_gv11b_snap_reg,
		NULL, 0),
	UNIT_TEST(fb_mmu_fault_gv11b_handle_fault,
		fb_mmu_fault_gv11b_handle_fault, NULL, 0),
	UNIT_TEST(fb_mmu_fault_gv11b_handle_bar2_fault,
		fb_mmu_fault_gv11b_handle_bar2_fault, NULL, 2),
	UNIT_TEST(fb_intr_gv11b_init_test, fb_intr_gv11b_init_test, NULL, 0),
	UNIT_TEST(fb_intr_gv11b_isr_test, fb_intr_gv11b_isr_test, NULL, 0),

	UNIT_TEST(fb_intr_gv11b_ecc_test_L2TLB, fb_intr_gv11b_ecc_test,
		(void *) TEST_ECC_L2TLB, 0),
	UNIT_TEST(fb_intr_gv11b_ecc_test_HUBTLB, fb_intr_gv11b_ecc_test,
		(void *) TEST_ECC_HUBTLB, 0),
	UNIT_TEST(fb_intr_gv11b_ecc_test_FILLUNIT, fb_intr_gv11b_ecc_test,
		(void *) TEST_ECC_FILLUNIT, 0),

	UNIT_TEST(fb_gv11b_cleanup, fb_gv11b_cleanup, NULL, 0),
};

UNIT_MODULE(fb, fb_tests, UNIT_PRIO_NVGPU_TEST);
