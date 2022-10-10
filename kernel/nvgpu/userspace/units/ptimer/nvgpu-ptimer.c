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

#include <nvgpu/gk20a.h>
#include <nvgpu/ptimer.h>
#include <nvgpu/cic_mon.h>
#include <hal/ptimer/ptimer_gk20a.h>
#include <hal/cic/mon/cic_ga10b.h>
#include <nvgpu/hw/gk20a/hw_timer_gk20a.h>

#include "nvgpu-ptimer.h"

/*
 * Mock I/O
 */

/*
 * Write callback. Forward the write access to the mock IO framework.
 */
static void writel_access_reg_fn(struct gk20a *g,
				 struct nvgpu_reg_access *access)
{
	nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
}

/* Used to simulate wrap */
#define TIMER1_VALUES_SIZE 4
static u32 timer1_values[TIMER1_VALUES_SIZE];
static u32 timer1_index;

/*
 * Read callback. Get the register value from the mock IO framework.
 */
static void readl_access_reg_fn(struct gk20a *g,
				struct nvgpu_reg_access *access)
{
	/* Used to simulate wrap */
	if (access->addr == timer_time_1_r()) {
		BUG_ON(timer1_index >= TIMER1_VALUES_SIZE);
		access->value = timer1_values[timer1_index++];
	} else {
		access->value = nvgpu_posix_io_readl_reg_space(g, access->addr);
	}
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

/* map the whole page */
#define PTIMER_REG_SPACE_START (timer_pri_timeout_r() & ~0xfff)
#define PTIMER_REG_SPACE_SIZE 0xfff

int ptimer_test_setup_env(struct unit_module *m,
			  struct gk20a *g, void *args)
{
	/* Setup HAL */
	g->ops.ptimer.isr = gk20a_ptimer_isr;

	g->ops.cic_mon.init = ga10b_cic_mon_init;

	/* Create ptimer register space */
	if (nvgpu_posix_io_add_reg_space(g, PTIMER_REG_SPACE_START,
					 PTIMER_REG_SPACE_SIZE) != 0) {
		unit_err(m, "%s: failed to create register space\n",
			 __func__);
		return UNIT_FAIL;
	}
	(void)nvgpu_posix_register_io(g, &test_reg_callbacks);

	if (nvgpu_cic_mon_setup(g) != 0) {
		unit_err(m, "%s: failed to initialize CIC\n",
				__func__);
		return UNIT_FAIL;
	}

	if (nvgpu_cic_mon_init_lut(g) != 0) {
		unit_err(m, "%s: failed to initialize CIC LUT\n",
				__func__);
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

int ptimer_test_free_env(struct unit_module *m,
			 struct gk20a *g, void *args)
{
	/* Free register space */
	nvgpu_posix_io_delete_reg_space(g, PTIMER_REG_SPACE_START);

	return UNIT_SUCCESS;
}


static u32 received_error_code;
static void mock_decode_error_code(struct gk20a *g, u32 error_code)
{
	received_error_code = error_code;
}

int test_ptimer_isr(struct unit_module *m,
			struct gk20a *g, void *args)
{
	int ret = UNIT_SUCCESS;
	int val0, val1;
	u32 fecs_errcode = 0xa5;

	/* initialize regs to defaults */
	nvgpu_posix_io_writel_reg_space(g, timer_pri_timeout_save_0_r(), 0);
	nvgpu_posix_io_writel_reg_space(g, timer_pri_timeout_save_1_r(), 0);
	nvgpu_posix_io_writel_reg_space(g, timer_pri_timeout_fecs_errcode_r(),
					0);

	/* all zero test */
	g->ops.ptimer.isr(g);
	val0 = nvgpu_posix_io_readl_reg_space(g, timer_pri_timeout_save_0_r());
	val1 = nvgpu_posix_io_readl_reg_space(g, timer_pri_timeout_save_1_r());
	if ((val0 != 0) || (val1 != 0)) {
		unit_err(m, "ptimer isr failed to clear regs\n");
		ret = UNIT_FAIL;
	}

	/* set fecs bits */
	nvgpu_posix_io_writel_reg_space(g, timer_pri_timeout_save_0_r(),
					((u32)1 << 31));
	nvgpu_posix_io_writel_reg_space(g, timer_pri_timeout_fecs_errcode_r(),
					fecs_errcode);
	g->ops.ptimer.isr(g);
	val0 = nvgpu_posix_io_readl_reg_space(g, timer_pri_timeout_save_0_r());
	val1 = nvgpu_posix_io_readl_reg_space(g, timer_pri_timeout_save_1_r());
	if ((val0 != 0) || (val1 != 0)) {
		unit_err(m, "ptimer isr failed to clear regs\n");
		ret = UNIT_FAIL;
	}

	/* with fecs set and a decode HAL to call */
	g->ops.priv_ring.decode_error_code = mock_decode_error_code;
	nvgpu_posix_io_writel_reg_space(g, timer_pri_timeout_save_0_r(),
					((u32)1 << 31));
	nvgpu_posix_io_writel_reg_space(g, timer_pri_timeout_fecs_errcode_r(),
					fecs_errcode);
	g->ops.ptimer.isr(g);
	if (received_error_code != fecs_errcode) {
		unit_err(m, "ptimer isr failed pass err code to HAL\n");
		ret = UNIT_FAIL;
	}
	val0 = nvgpu_posix_io_readl_reg_space(g, timer_pri_timeout_save_0_r());
	val1 = nvgpu_posix_io_readl_reg_space(g, timer_pri_timeout_save_1_r());
	if ((val0 != 0) || (val1 != 0)) {
		unit_err(m, "ptimer isr failed to clear regs\n");
		ret = UNIT_FAIL;
	}

	/* Set save0 timeout bit to get a branch covered */
	nvgpu_posix_io_writel_reg_space(g, timer_pri_timeout_save_0_r(),
					((u32)1 << 1));
	nvgpu_posix_io_writel_reg_space(g, timer_pri_timeout_fecs_errcode_r(),
					0);
	g->ops.ptimer.isr(g);
	val0 = nvgpu_posix_io_readl_reg_space(g, timer_pri_timeout_save_0_r());
	val1 = nvgpu_posix_io_readl_reg_space(g, timer_pri_timeout_save_1_r());
	if ((val0 != 0) || (val1 != 0)) {
		unit_err(m, "ptimer isr failed to clear regs\n");
		ret = UNIT_FAIL;
	}

	return ret;
}

int test_ptimer_scaling(struct unit_module *m,
			struct gk20a *g, void *args)
{
	int ret = UNIT_SUCCESS;
	int err;
	u32 val;

	/* Initialize ptimer source freq as per gv11b platform freq */
	g->ptimer_src_freq = 31250000U;

	err = nvgpu_ptimer_scale(g, 0U, &val);
	if ((err != 0) || (val != 0U)) {
		unit_err(m, "ptimer scale calculation incorrect, line %u\n", __LINE__);
		ret = UNIT_FAIL;
	}

	err = nvgpu_ptimer_scale(g, 1000U, &val);
	if ((err != 0) || (val != 1000U)) {
		unit_err(m, "ptimer scale calculation incorrect, line %u\n", __LINE__);
		ret = UNIT_FAIL;
	}

	err = nvgpu_ptimer_scale(g, U32_MAX / 10, &val);
	if ((err != 0) || (val !=  (U32_MAX / 10))) {
		unit_err(m, "ptimer scale calculation incorrect, line %u\n", __LINE__);
		ret = UNIT_FAIL;
	}

	err = nvgpu_ptimer_scale(g, (U32_MAX / 10U) + 1, &val);
	if (err == 0) {
		unit_err(m, "unexpected success returned, line %u\n", __LINE__);
		ret = UNIT_FAIL;
	}

	err = nvgpu_ptimer_scale(g, U32_MAX / 5U, &val);
	if (err == 0) {
		unit_err(m, "unexpected success returned, line %u\n", __LINE__);
		ret = UNIT_FAIL;
	}

	err = nvgpu_ptimer_scale(g, U32_MAX, &val);
	if (err == 0) {
		unit_err(m, "unexpected success returned, line %u\n", __LINE__);
		ret = UNIT_FAIL;
	}

	return ret;
}

struct unit_module_test ptimer_tests[] = {
	UNIT_TEST(ptimer_setup_env,	ptimer_test_setup_env,	NULL, 0),
	UNIT_TEST(ptimer_isr,		test_ptimer_isr,	NULL, 0),
	UNIT_TEST(ptimer_scaling,	test_ptimer_scaling,	NULL, 0),
	UNIT_TEST(ptimer_free_env,	ptimer_test_free_env,	NULL, 0),
};

UNIT_MODULE(ptimer, ptimer_tests, UNIT_PRIO_NVGPU_TEST);
