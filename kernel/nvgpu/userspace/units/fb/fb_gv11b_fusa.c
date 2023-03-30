/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/cic_mon.h>
#include <nvgpu/nvgpu_init.h>
#include "hal/mc/mc_gp10b.h"
#include "hal/fb/fb_gm20b.h"
#include "hal/fb/fb_gv11b.h"
#include "hal/fb/ecc/fb_ecc_gv11b.h"
#include "hal/fb/intr/fb_intr_gv11b.h"
#include "hal/fb/intr/fb_intr_ecc_gv11b.h"
#include "hal/cic/mon/cic_ga10b.h"
#include <nvgpu/hw/gv11b/hw_fb_gv11b.h>

#include "fb_fusa.h"

int fb_gv11b_init_test(struct unit_module *m, struct gk20a *g, void *args)
{
	int err;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	/* Define the operations being targeted in this unit test */
	g->ops.ecc.ecc_init_support = nvgpu_ecc_init_support;
	g->ops.fb.init_hw = gv11b_fb_init_hw;
	g->ops.fb.init_fs_state = gv11b_fb_init_fs_state;
	g->ops.fb.set_atomic_mode = gv11b_fb_set_atomic_mode;
	g->ops.fb.ecc.init = gv11b_fb_ecc_init;
	g->ops.fb.ecc.free = gv11b_fb_ecc_free;
	g->ops.fb.ecc.l2tlb_error_mask = gv11b_fb_ecc_l2tlb_error_mask;
	g->ops.fb.intr.handle_ecc = gv11b_fb_intr_handle_ecc,
	g->ops.fb.intr.handle_ecc_l2tlb = gv11b_fb_intr_handle_ecc_l2tlb,
	g->ops.fb.intr.handle_ecc_hubtlb = gv11b_fb_intr_handle_ecc_hubtlb,
	g->ops.fb.intr.handle_ecc_fillunit = gv11b_fb_intr_handle_ecc_fillunit,

	/* Other HALs */
	g->ops.mc.intr_stall_unit_config = mc_gp10b_intr_stall_unit_config;
	g->ops.mc.intr_nonstall_unit_config =
		mc_gp10b_intr_nonstall_unit_config;
	g->ops.fb.intr.enable = gv11b_fb_intr_enable;
	g->ops.cic_mon.init = ga10b_cic_mon_init;

	/*
	 * Define some arbitrary addresses for test purposes.
	 * Note: no need to malloc any memory as this unit only needs to trigger
	 * MMU faults via register mocking. No other memory accesses are done.
	 */
	g->mm.sysmem_flush.cpu_va = (void *) 0x10000000;
	g->mm.mmu_wr_mem.cpu_va = (void *) 0x20000000;
	g->mm.mmu_wr_mem.aperture = APERTURE_SYSMEM;
	g->mm.mmu_rd_mem.cpu_va = (void *) 0x30000000;
	g->mm.mmu_rd_mem.aperture = APERTURE_SYSMEM;

	if (nvgpu_cic_mon_setup(g) != 0) {
		unit_return_fail(m, "CIC init failed\n");
	}

	if (nvgpu_cic_mon_init_lut(g) != 0) {
		unit_return_fail(m, "CIC LUT init failed\n");
	}

	g->ops.ecc.ecc_init_support(g);

	nvgpu_writel(g, fb_niso_intr_en_set_r(0), 0);
	g->ops.fb.init_hw(g);
	/* Ensure that g->ops.fb.intr.enable set up a mask */
	if (nvgpu_readl(g, fb_niso_intr_en_set_r(0)) == 0) {
		unit_return_fail(m, "FB_NISO mask not set\n");
	}

	g->ops.fb.init_fs_state(g);
	g->ops.fb.set_atomic_mode(g);
	/* Ensure atomic mode was enabled */
	if ((nvgpu_readl(g, fb_mmu_ctrl_r()) &
		fb_mmu_ctrl_atomic_capability_mode_m()) == 0) {

		unit_return_fail(m, "Atomic mode not set\n");
	}

	/* For branch coverage */
	nvgpu_set_enabled(g, NVGPU_SEC_PRIVSECURITY, true);
	g->ops.fb.init_fs_state(g);
	nvgpu_set_enabled(g, NVGPU_SEC_PRIVSECURITY, false);

	/*
	 * gv11b_fb_ecc_init initializes 5 structures via kmem. Test the failure
	 * of all of them.
	 */
	for (int i = 0; i < 5; i++) {
		nvgpu_posix_enable_fault_injection(kmem_fi, true, i);
		err = g->ops.fb.ecc.init(g);
		nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
		if (err != -ENOMEM) {
			unit_return_fail(m, "gv11b_fb_ecc_init did not fail as expected (%d)\n", i);
		}

		g->ops.ecc.ecc_init_support(g);
	}

	err = g->ops.fb.ecc.init(g);
	if (err != 0) {
		unit_return_fail(m, "gv11b_fb_ecc_init failed\n");
	}

	g->ops.fb.ecc.free(g);

	return UNIT_SUCCESS;
}
