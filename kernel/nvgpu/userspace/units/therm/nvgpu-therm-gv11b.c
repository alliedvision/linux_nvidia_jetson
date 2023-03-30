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

#include <nvgpu/gk20a.h>
#include <nvgpu/therm.h>
#include <hal/therm/therm_gv11b.h>
#include <nvgpu/hw/gv11b/hw_therm_gv11b.h>
#include <nvgpu/power_features/cg.h>
#include <nvgpu/fifo.h>
#include <os/posix/os_posix.h>

#include "nvgpu-therm.h"

#define NUM_ENGINES 2
#define INVALID_GATE_MODE 100

int test_therm_init_elcg_mode(struct unit_module *m, struct gk20a *g,
					void *args)
{
	int ret = UNIT_FAIL;
	unsigned int engine, i;
	u32 val;
	struct match_struct {
		u32 mode;
		u32 mask;
	};
	struct match_struct match_table[] = {
		{ ELCG_RUN,
		  therm_gate_ctrl_idle_holdoff_on_f() |
		  therm_gate_ctrl_eng_clk_run_f()
		},
		{ ELCG_AUTO,		therm_gate_ctrl_eng_clk_auto_f() },
		{ ELCG_STOP,		therm_gate_ctrl_eng_clk_stop_f() },
		{ INVALID_GATE_MODE,	0x00000000 },
	};

	/* enable ELCG */
	nvgpu_set_enabled(g, NVGPU_GPU_CAN_ELCG, true);

	for (engine = 0U; engine < NUM_ENGINES; engine++) {
		for (i = 0U; i < ARRAY_SIZE(match_table); i++) {
			/* clear the therm gate control reg */
			nvgpu_posix_io_writel_reg_space(g,
						therm_gate_ctrl_r(engine), 0U);
			gv11b_therm_init_elcg_mode(g, match_table[i].mode,
						engine);
			val = nvgpu_posix_io_readl_reg_space(g,
						therm_gate_ctrl_r(engine));
			unit_assert(val == match_table[i].mask, goto done);
		}
	}

	/* test with ELCG disabled */
	nvgpu_set_enabled(g, NVGPU_GPU_CAN_ELCG, false);
	nvgpu_posix_io_writel_reg_space(g, therm_gate_ctrl_r(0), 0U);
	gv11b_therm_init_elcg_mode(g, ELCG_RUN, 0);
	val = nvgpu_posix_io_readl_reg_space(g, therm_gate_ctrl_r(0));
	unit_assert(val == 0U, goto done);

	ret = UNIT_SUCCESS;

done:
	return ret;
}

int test_elcg_init_idle_filters(struct unit_module *m, struct gk20a *g,
					void *args)
{
	int ret = UNIT_FAIL;
	int err;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	unsigned int i;
	u32 val;
	const u32 expect_gate_ctrl =
			(therm_gate_ctrl_eng_idle_filt_exp__prod_f() |
			 therm_gate_ctrl_eng_idle_filt_mant__prod_f() |
			 therm_gate_ctrl_eng_delay_before__prod_f() |
			 therm_gate_ctrl_eng_delay_after__prod_f());

	/* setup FIFO info & regs */
	nvgpu_posix_io_writel_reg_space(g, therm_fecs_idle_filter_r(), 0U);
	nvgpu_posix_io_writel_reg_space(g, therm_hubmmu_idle_filter_r(), 0U);

	/* make sure nothing happens if we're in simulation */
	p->is_simulation = true;
	err = gv11b_elcg_init_idle_filters(g);
	unit_assert(err == 0, goto done);
	val = nvgpu_posix_io_readl_reg_space(g, therm_fecs_idle_filter_r());
	unit_assert(val == 0U, goto done);
	val = nvgpu_posix_io_readl_reg_space(g, therm_hubmmu_idle_filter_r());
	unit_assert(val == 0U, goto done);
	for (i = 0U; i < NUM_ENGINES; i++) {
		val = nvgpu_posix_io_readl_reg_space(g, therm_gate_ctrl_r(i));
		unit_assert(val == 0U, goto done);
	}
	p->is_simulation = false;

	/* now test the default case */
	err = gv11b_elcg_init_idle_filters(g);
	unit_assert(err == 0, goto done);
	val = nvgpu_posix_io_readl_reg_space(g, therm_fecs_idle_filter_r());
	unit_assert(val == 0U, goto done);
	val = nvgpu_posix_io_readl_reg_space(g, therm_hubmmu_idle_filter_r());
	unit_assert(val == 0U, goto done);
	for (i = 0U; i < NUM_ENGINES; i++) {
		val = nvgpu_posix_io_readl_reg_space(g, therm_gate_ctrl_r(i));
		unit_assert(val == expect_gate_ctrl, goto done);
	}

	ret = UNIT_SUCCESS;

done:
	return ret;
}
