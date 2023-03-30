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
#include <nvgpu/device.h>
#include <nvgpu/ce.h>
#include <nvgpu/cic_mon.h>
#include <hal/ce/ce_gp10b.h>
#include <hal/ce/ce_gv11b.h>
#include <hal/cic/mon/cic_ga10b.h>
#include <nvgpu/hw/gv11b/hw_ce_gv11b.h>

#include "nvgpu-ce.h"

#define assert(cond) unit_assert(cond, goto fail)

#define CE_ADDR_SPACE_START	0x00104000
#define CE_ADDR_SPACE_SIZE	0xfff
#define NUM_INST		2

/*
 * Mock I/O
 */

/*
 * Write callback. Forward the write access to the mock IO framework.
 */
static u32 intr_status_written[NUM_INST];
static void writel_access_reg_fn(struct gk20a *g,
				 struct nvgpu_reg_access *access)
{
	if (access->addr == ce_intr_status_r(0)) {
		intr_status_written[0] |= access->value;
		nvgpu_posix_io_writel_reg_space(g, access->addr,
			nvgpu_posix_io_readl_reg_space(g, access->addr) &
			~access->value);
	} else if (access->addr == ce_intr_status_r(1)) {
		intr_status_written[1] |= access->value;
		nvgpu_posix_io_writel_reg_space(g, access->addr,
			nvgpu_posix_io_readl_reg_space(g, access->addr) &
			~access->value);
	} else {
		nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
	}
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

/*
 * Replacement functions that can be assigned to function pointers
 */
static void mock_void_return(struct gk20a *g)
{
	/* noop */
}

static int mock_mc_enable_units(struct gk20a *g, u32 units, bool enable)
{
	return 0;
}
static int mock_mc_enable_dev(struct gk20a *g, const struct nvgpu_device *dev,
			bool enable)
{
	return 0;
}

static void mock_intr_unit_config(struct gk20a *g, u32 unit, bool enable)
{
	/* noop */
}

int test_ce_setup_env(struct unit_module *m,
			  struct gk20a *g, void *args)
{
	/* Create mc register space */
	if (nvgpu_posix_io_add_reg_space(g, CE_ADDR_SPACE_START,
						CE_ADDR_SPACE_SIZE) != 0) {
		unit_err(m, "%s: failed to create register space\n",
			 __func__);
		return UNIT_FAIL;
	}
	(void)nvgpu_posix_register_io(g, &test_reg_callbacks);

	nvgpu_mutex_init(&g->cg_pg_lock);
	g->blcg_enabled = false;
	nvgpu_spinlock_init(&g->mc.intr_lock);

	g->ops.cic_mon.init = ga10b_cic_mon_init;
	g->ops.ce.get_inst_ptr_from_lce = gv11b_ce_get_inst_ptr_from_lce;

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

int test_ce_free_env(struct unit_module *m, struct gk20a *g, void *args)
{
	/* Free mc register space */
	nvgpu_posix_io_delete_reg_space(g, CE_ADDR_SPACE_START);

	return UNIT_SUCCESS;
}

int test_ce_init_support(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_SUCCESS;
	int err;

	nvgpu_device_init(g);

	g->fifo.num_engines = 0;
	g->ops.ce.set_pce2lce_mapping = mock_void_return;
	g->ops.ce.init_prod_values = mock_void_return;
	g->ops.mc.enable_units = mock_mc_enable_units;
	g->ops.mc.enable_dev = mock_mc_enable_dev;
	g->ops.mc.intr_nonstall_unit_config = mock_intr_unit_config;
	g->ops.mc.intr_stall_unit_config = mock_intr_unit_config;

	/* test default case where all HALs are defined */
	err = nvgpu_ce_init_support(g);
	if (err != 0) {
		ret = UNIT_FAIL;
		unit_err(m, "failed to init ce\n");
		goto done;
	}

	/* test with this HAL set to NULL for branch coverage */
	g->ops.ce.set_pce2lce_mapping = NULL;
	err = nvgpu_ce_init_support(g);
	if (err != 0) {
		ret = UNIT_FAIL;
		unit_err(m, "failed to init ce\n");
		goto done;
	}

	/* test with this HAL set to NULL for branch coverage */
	g->ops.ce.init_prod_values = NULL;
	err = nvgpu_ce_init_support(g);
	if (err != 0) {
		ret = UNIT_FAIL;
		unit_err(m, "failed to init ce\n");
		goto done;
	}

done:
	return ret;
}

int test_ce_stall_isr(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_SUCCESS;
	int inst_id;
	u32 intr_val;

	g->ops.ce.isr_stall = gv11b_ce_stall_isr;
	for (inst_id = 0; inst_id < NUM_INST; inst_id++) {
		intr_status_written[inst_id] = 0;
		/* all intr sources except launcherr as they are not supported on safety */
		intr_val = 0x4;
		nvgpu_posix_io_writel_reg_space(g, ce_intr_status_r(inst_id),
						intr_val);
		nvgpu_ce_stall_isr(g, inst_id, 0);
		if (intr_status_written[inst_id] != (intr_val &
				 ~ce_intr_status_nonblockpipe_pending_f())) {
			ret = UNIT_FAIL;
			unit_err(m, "intr_status not cleared, only 0x%08x\n",
				 intr_status_written[inst_id]);
			goto done;
		}

		intr_status_written[inst_id] = 0;
		intr_val = 0x0;
		nvgpu_posix_io_writel_reg_space(g, ce_intr_status_r(inst_id),
						intr_val);
		nvgpu_ce_stall_isr(g, inst_id, 0);
		if (intr_status_written[inst_id] != intr_val) {
			ret = UNIT_FAIL;
			unit_err(m, "intr_status not cleared, only 0x%08x\n",
				 intr_status_written[inst_id]);
			goto done;
		}
	}

done:
	return ret;
}

int test_get_num_pce(struct unit_module *m, struct gk20a *g, void *args)
{
	u32 pce_map_val; /* 16 bit bitmap */
	u32 val;

	g->ops.ce.get_num_pce = gv11b_ce_get_num_pce;
	for (pce_map_val = 0; pce_map_val <= U16_MAX; pce_map_val++) {
		nvgpu_posix_io_writel_reg_space(g, ce_pce_map_r(),
						pce_map_val);
		val = g->ops.ce.get_num_pce(g);
		if (val != hweight32(pce_map_val)) {
			unit_return_fail(m, "incorrect value %u\n", val);
		}
	}

	return UNIT_SUCCESS;
}

int test_init_prod_values(struct unit_module *m, struct gk20a *g, void *args)
{
	int inst_id;
	u32 val;

	g->ops.ce.init_prod_values = gv11b_ce_init_prod_values;
	for (inst_id = 0; inst_id < NUM_INST; inst_id++) {
		/* init reg to known state */
		nvgpu_posix_io_writel_reg_space(g, ce_lce_opt_r(inst_id), 0U);
	}
	g->ops.ce.init_prod_values(g);
	for (inst_id = 0; inst_id < NUM_INST; inst_id++) {
		/* verify written correctly */
		val = nvgpu_posix_io_readl_reg_space(g, ce_lce_opt_r(inst_id));
		if (val != ce_lce_opt_force_barriers_npl__prod_f()) {
			unit_return_fail(m, "value incorrect 0x%08x\n", val);
		}
	}

	return UNIT_SUCCESS;
}

struct unit_module_test ce_tests[] = {
	UNIT_TEST(ce_setup_env,				test_ce_setup_env,		NULL, 0),
	UNIT_TEST(ce_init_support,			test_ce_init_support,	NULL, 0),
	UNIT_TEST(ce_stall_isr,				test_ce_stall_isr,	NULL, 0),
	UNIT_TEST(ce_get_num_pce,			test_get_num_pce,	NULL, 0),
	UNIT_TEST(ce_init_prod_values,			test_init_prod_values,	NULL, 0),
	UNIT_TEST(ce_free_env,				test_ce_free_env,		NULL, 0),
};

UNIT_MODULE(ce, ce_tests, UNIT_PRIO_NVGPU_TEST);
