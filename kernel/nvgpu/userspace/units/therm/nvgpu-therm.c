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
#include <nvgpu/device.h>
#include <nvgpu/engines.h>
#include <hal/therm/therm_gv11b.h>

#include "nvgpu-therm.h"

#define THERM_ADDR_SPACE_START	0x00020000
#define THERM_ADDR_SPACE_SIZE	0xfff

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

/*
 * Read callback. Get the register value from the mock IO framework.
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

static int mock_hal_fail(struct gk20a *g)
{
	return -1;
}

int test_setup_env(struct unit_module *m,
			  struct gk20a *g, void *args)
{
	/* Create therm register space */
	if (nvgpu_posix_io_add_reg_space(g, THERM_ADDR_SPACE_START,
						THERM_ADDR_SPACE_SIZE) != 0) {
		unit_err(m, "%s: failed to create register space\n",
			 __func__);
		return UNIT_FAIL;
	}
	(void)nvgpu_posix_register_io(g, &test_reg_callbacks);

	return UNIT_SUCCESS;
}

int test_free_env(struct unit_module *m, struct gk20a *g, void *args)
{
	nvgpu_posix_io_delete_reg_space(g, THERM_ADDR_SPACE_START);

	return UNIT_SUCCESS;
}

int test_therm_init_support(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	int err ;
	int (*save_hal)(struct gk20a *g);

	g->fifo.g = g;
	nvgpu_device_init(g);
	nvgpu_engine_setup_sw(g);

	save_hal = g->ops.therm.init_therm_setup_hw;

	/* default case */
	err = g->ops.therm.init_therm_support(g);
	unit_assert(err == 0, goto done);

	/* set this HAL to NULL for branch coverage */
	g->ops.therm.init_therm_setup_hw = NULL;
	err = g->ops.therm.init_therm_support(g);
	unit_assert(err == 0, goto done);

	/* make this HAL return error */
	g->ops.therm.init_therm_setup_hw = mock_hal_fail;
	err = g->ops.therm.init_therm_support(g);
	unit_assert(err != 0, goto done);

	ret = UNIT_SUCCESS;

done:
	g->ops.therm.init_therm_setup_hw = save_hal;

	return ret;
}

struct unit_module_test therm_tests[] = {
	UNIT_TEST(therm_setup_env,		test_setup_env,				NULL, 0),
	UNIT_TEST(therm_init_support,		test_therm_init_support,		NULL, 0),
	UNIT_TEST(gv11b_therm_init_elcg_mode,	test_therm_init_elcg_mode,		NULL, 0),
	UNIT_TEST(gv11b_elcg_init_idle_filters,	test_elcg_init_idle_filters,		NULL, 0),
	UNIT_TEST(therm_free_env,		test_free_env,				NULL, 0),
};

UNIT_MODULE(therm, therm_tests, UNIT_PRIO_NVGPU_TEST);
