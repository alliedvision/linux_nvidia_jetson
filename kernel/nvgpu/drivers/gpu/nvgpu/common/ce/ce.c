/*
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/types.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/engines.h>
#include <nvgpu/device.h>
#include <nvgpu/ce.h>
#include <nvgpu/power_features/cg.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/mc.h>
#include <nvgpu/rc.h>
#include <nvgpu/nvgpu_init.h>

int nvgpu_ce_init_support(struct gk20a *g)
{
	int err = 0;

	if (g->ops.ce.set_pce2lce_mapping != NULL) {
		g->ops.ce.set_pce2lce_mapping(g);
	}

	/*
	 * Bug 1895019
	 * Each time PCE2LCE config is updated and if it happens to
	 * map a LCE which was previously unmapped, then ELCG would have turned
	 * off the clock to the unmapped LCE and when the LCE config is updated,
	 * a race occurs between the config update and ELCG turning on the clock
	 * to that LCE, this might result in LCE dropping the config update.
	 * To avoid such a race, each time PCE2LCE config is updated toggle
	 * resets for all LCEs.
	 */
	err = nvgpu_mc_reset_devtype(g, NVGPU_DEVTYPE_LCE);
	if (err != 0) {
		nvgpu_err(g, "NVGPU_DEVTYPE_LCE reset failed");
		return err;
	}

	nvgpu_cg_slcg_ce2_load_enable(g);

	nvgpu_cg_blcg_ce_load_enable(g);

#if defined(CONFIG_NVGPU_NON_FUSA)
	nvgpu_cg_elcg_ce_load_enable(g);
#endif

	if (g->ops.ce.init_prod_values != NULL) {
		g->ops.ce.init_prod_values(g);
	}

#ifdef CONFIG_NVGPU_NONSTALL_INTR
	if (g->ops.ce.init_hw != NULL) {
		g->ops.ce.init_hw(g);
	}
#endif

	if (g->ops.ce.intr_enable != NULL) {
		g->ops.ce.intr_enable(g, true);
	}

	/** Enable interrupts at MC level */
	nvgpu_cic_mon_intr_stall_unit_config(g, NVGPU_CIC_INTR_UNIT_CE, NVGPU_CIC_INTR_ENABLE);
#ifdef CONFIG_NVGPU_NONSTALL_INTR
	nvgpu_cic_mon_intr_nonstall_unit_config(g, NVGPU_CIC_INTR_UNIT_CE, NVGPU_CIC_INTR_ENABLE);
#endif

	return 0;
}

void nvgpu_ce_stall_isr(struct gk20a *g, u32 inst_id, u32 pri_base)
{
	bool needs_rc = false;
	bool needs_quiesce = false;

	if (g->ops.ce.isr_stall != NULL) {
		g->ops.ce.isr_stall(g, inst_id, pri_base, &needs_rc,
					&needs_quiesce);
	}

	if (needs_quiesce) {
		nvgpu_sw_quiesce(g);
	}

	if (needs_rc) {
		nvgpu_log(g, gpu_dbg_intr,
			"Recovery needed to handle CE interrupt.");
		nvgpu_rc_ce_fault(g, inst_id);
	}
}
