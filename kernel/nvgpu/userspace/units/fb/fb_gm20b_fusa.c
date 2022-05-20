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

#include <unit/unit.h>
#include <unit/io.h>
#include <nvgpu/posix/io.h>
#include <nvgpu/posix/posix-fault-injection.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/io.h>
#include <nvgpu/nvgpu_init.h>
#include "hal/mc/mc_gp10b.h"
#include "hal/fb/fb_gm20b.h"
#include "hal/fb/fb_gv11b.h"
#include "hal/fb/intr/fb_intr_gv11b.h"
#include <nvgpu/hw/gv11b/hw_fb_gv11b.h>

#include "fb_fusa.h"

#define TEST_REG_VALUE 0x8080A0A0

int fb_gm20b_tlb_invalidate_test(struct unit_module *m, struct gk20a *g,
	void *args)
{
	int err;
	struct nvgpu_mem pdb = {0};

	/* Define the operations being tested in this unit test */
	g->ops.fb.tlb_invalidate = gm20b_fb_tlb_invalidate;

	/* Setup PDB */
	pdb.aperture = APERTURE_SYSMEM;

	/* First NVGPU is powered off */
	err = g->ops.fb.tlb_invalidate(g, &pdb);
	if (err != 0) {
		unit_return_fail(m, "tlb_invalidate failed (1)\n");
	}

	/* Set NVGPU as powered on */
	g->power_on_state = NVGPU_STATE_POWERED_ON;

	/* Timeout fail on fb_mmu_ctrl_r() read */
	err = g->ops.fb.tlb_invalidate(g, &pdb);
	if (err != -ETIMEDOUT) {
		unit_return_fail(m,
			"tlb_invalidate did not fail as expected (2)\n");
	}

	/*
	 * Prevent timeout on fb_mmu_ctrl_r() by setting a non-zero value in
	 * the fb_mmu_ctrl_pri_fifo_space_v field.
	 */
	nvgpu_writel(g, fb_mmu_ctrl_r(), 1 << 16U);

	/*
	 * Timeout on fb_mmu_ctrl_r read after MMU invalidate (does not return
	 * a failure)
	 */
	helper_intercept_mmu_write(0);
	err = g->ops.fb.tlb_invalidate(g, &pdb);
	if (err != 0) {
		unit_return_fail(m, "tlb_invalidate failed (2)\n");
	}

	/* Success */
	nvgpu_writel(g, fb_mmu_ctrl_r(), 1 << 16U);
	helper_intercept_mmu_write(1 << 15U);
	err = g->ops.fb.tlb_invalidate(g, &pdb);
	if (err != 0) {
		unit_return_fail(m, "tlb_invalidate failed (3)\n");
	}

	return UNIT_SUCCESS;
}

int fb_gm20b_mmu_ctrl_test(struct unit_module *m, struct gk20a *g, void *args)
{
	int err;
	u64 wpr_base, wpr_size;

	/* Define the operations being tested in this unit test */
	g->ops.fb.mmu_ctrl = gm20b_fb_mmu_ctrl;
	g->ops.fb.mmu_debug_ctrl = gm20b_fb_mmu_debug_ctrl;
	g->ops.fb.mmu_debug_wr = gm20b_fb_mmu_debug_wr;
	g->ops.fb.mmu_debug_rd = gm20b_fb_mmu_debug_rd;
	g->ops.fb.vpr_info_fetch = gm20b_fb_vpr_info_fetch;
	g->ops.fb.dump_vpr_info = gm20b_fb_dump_vpr_info;
	g->ops.fb.dump_wpr_info = gm20b_fb_dump_wpr_info;
	g->ops.fb.read_wpr_info = gm20b_fb_read_wpr_info;

	/* g->ops.mmu_ctrl must return the value in fb_mmu_ctrl_r */
	nvgpu_writel(g, fb_mmu_ctrl_r(), TEST_REG_VALUE);
	if (g->ops.fb.mmu_ctrl(g) != TEST_REG_VALUE) {
		unit_return_fail(m, "ops.mmu_ctrl: incorrect value\n");
	}

	/* g->ops.mmu_debug_ctrl must return the value in fb_mmu_debug_ctrl_r */
	nvgpu_writel(g, fb_mmu_debug_ctrl_r(), TEST_REG_VALUE);
	if (g->ops.fb.mmu_debug_ctrl(g) != TEST_REG_VALUE) {
		unit_return_fail(m, "ops.mmu_debug_ctrl: incorrect value\n");
	}

	/* g->ops.mmu_debug_wr must return the value in fb_mmu_debug_wr_r */
	nvgpu_writel(g, fb_mmu_debug_wr_r(), TEST_REG_VALUE);
	if (g->ops.fb.mmu_debug_wr(g) != TEST_REG_VALUE) {
		unit_return_fail(m, "ops.mmu_debug_wr: incorrect value\n");
	}

	/* g->ops.mmu_debug_rd must return the value in fb_mmu_debug_rd_r */
	nvgpu_writel(g, fb_mmu_debug_rd_r(), TEST_REG_VALUE);
	if (g->ops.fb.mmu_debug_rd(g) != TEST_REG_VALUE) {
		unit_return_fail(m, "ops.mmu_debug_rd: incorrect value\n");
	}

	/* For code coverage, run the VPR/WPR dump ops */
	g->ops.fb.dump_vpr_info(g);
	g->ops.fb.dump_wpr_info(g);
	g->ops.fb.read_wpr_info(g, &wpr_base, &wpr_size);
	g->ops.fb.vpr_info_fetch(g);

	/*
	 * Trigger timeout in the gm20b_fb_vpr_info_fetch_wait function on
	 * fb_mmu_vpr_info_fetch_v(val) == fb_mmu_vpr_info_fetch_false_v()
	 */
	nvgpu_writel(g, fb_mmu_vpr_info_r(), 1 << 2U);
	err = g->ops.fb.vpr_info_fetch(g);
	if (err != -ETIMEDOUT) {
		unit_return_fail(m,
			"vpr_info_fetch did not fail as expected (3)\n");
	}

	return UNIT_SUCCESS;
}
