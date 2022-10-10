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
#include <unit/core.h>
#include <nvgpu/io.h>
#include <nvgpu/cic_mon.h>
#include <os/posix/os_posix.h>
#include <nvgpu/posix/posix-fault-injection.h>

#include <nvgpu/gk20a.h>
#include <hal/mc/mc_gp10b.h>
#include <hal/ptimer/ptimer_gk20a.h>
#include <hal/bus/bus_gk20a.h>
#include <hal/bus/bus_gm20b.h>
#include <hal/bus/bus_gp10b.h>
#include <hal/bus/bus_gv11b.h>
#include <hal/cic/mon/cic_ga10b.h>

#include <nvgpu/hw/gv11b/hw_mc_gv11b.h>
#include <nvgpu/hw/gv11b/hw_bus_gv11b.h>

#include "nvgpu-bus.h"

#define assert(cond)    unit_assert(cond, goto done)
u32 read_bind_status_reg = 0U;
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
	/* BAR_1 bind status is indicated by value of
	 * bus_bind_status_bar1_pending = empty(0x0U) and
	 * bus_bind_status_bar1_outstanding = false(0x0U).
	 *
	 * Similarly, for BAR_2 bind status, we check
	 * bus_bind_status_bar2_pending = empty (0x0U)
	 * and bus_bind_status_bar2_outstanding = false(0x0U).
	 *
	 * During bar1/2_bind HAL, the bus_bind_status_r() register is polled to
	 * check if its value changed as described above.
	 * To get complete branch coverage in bus.bar1/2_bind(), after
	 * "read_bind_status_reg" read attempts, the value of
	 * bus_bind_status_r() is read as pending field = empty and
	 * outstanding field = false.
	 * This maps to bind_status = done after "read_cmd_reg" polling
	 * attempts.
	 */
	if (access->addr == bus_bind_status_r()) {
		if (read_bind_status_reg == 3U) {
			access->value =
				(bus_bind_status_bar1_pending_empty_f() |
				bus_bind_status_bar1_outstanding_false_f() |
				bus_bind_status_bar2_pending_empty_f() |
				bus_bind_status_bar2_outstanding_false_f());
			read_bind_status_reg = nvgpu_safe_add_u32(
						read_bind_status_reg, 1U);
			return;
		}
		read_bind_status_reg = nvgpu_safe_add_u32(read_bind_status_reg,
								1U);
	}
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

/* NV_PBUS register space */
#define NV_PBUS_START 0x00001000U
#define NV_PBUS_SIZE  0x00000FFFU

/* NV_PRIV_GPC register space */
#define NV_PMC_START 0x00000000U
#define NV_PMC_SIZE  0x00000FFFU

/* NV_PTIMER register space */
#define NV_PTIMER_START 0x00009000U
#define NV_PTIMER_SIZE  0x00000FFFU

int test_bus_setup(struct unit_module *m, struct gk20a *g, void *args)
{
	/* Init HAL */
	g->ops.bus.init_hw = gk20a_bus_init_hw;
	g->ops.bus.isr = gk20a_bus_isr;
	g->ops.bus.bar1_bind = gm20b_bus_bar1_bind;
	g->ops.bus.bar2_bind = gp10b_bus_bar2_bind;
	g->ops.bus.configure_debug_bus = gv11b_bus_configure_debug_bus;
	g->ops.mc.intr_nonstall_unit_config =
					mc_gp10b_intr_nonstall_unit_config;
	g->ops.ptimer.isr = gk20a_ptimer_isr;
	g->ops.cic_mon.init = ga10b_cic_mon_init;

	/* Map register space NV_PRIV_MASTER */
	if (nvgpu_posix_io_add_reg_space(g, NV_PBUS_START, NV_PBUS_SIZE) != 0) {
		unit_err(m, "%s: failed to register space: NV_PBUS\n",
								__func__);
		return UNIT_FAIL;
	}

	/* Map register space NV_PMC */
	if (nvgpu_posix_io_add_reg_space(g, NV_PMC_START,
						NV_PMC_SIZE) != 0) {
		unit_err(m, "%s: failed to register space: NV_PMC\n",
								__func__);
		return UNIT_FAIL;
	}

	/* Map register space NV_PTIMER */
	if (nvgpu_posix_io_add_reg_space(g, NV_PTIMER_START,
						NV_PTIMER_SIZE) != 0) {
		unit_err(m, "%s: failed to register space: NV_PTIMER\n",
								__func__);
		return UNIT_FAIL;
	}

	(void)nvgpu_posix_register_io(g, &test_reg_callbacks);

	if (nvgpu_cic_mon_setup(g) != 0) {
		unit_err(m, "%s: Failed to initialize CIC\n",
							__func__);
		return UNIT_FAIL;
	}

	if (nvgpu_cic_mon_init_lut(g) != 0) {
		unit_err(m, "%s: Failed to initialize CIC LUT\n",
							__func__);
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

int test_bus_free_reg_space(struct unit_module *m, struct gk20a *g, void *args)
{
	/* Free register space */
	nvgpu_posix_io_delete_reg_space(g, NV_PBUS_START);
	nvgpu_posix_io_delete_reg_space(g, NV_PMC_START);
	nvgpu_posix_io_delete_reg_space(g, NV_PTIMER_START);

	return UNIT_SUCCESS;
}

int test_init_hw(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);

	nvgpu_writel(g, bus_debug_sel_0_r(), 0xFU);
	nvgpu_writel(g, bus_debug_sel_1_r(), 0xFU);
	nvgpu_writel(g, bus_debug_sel_2_r(), 0xFU);
	nvgpu_writel(g, bus_debug_sel_3_r(), 0xFU);

	p->is_fpga = false;
	p->is_silicon = false;
	g->ops.bus.configure_debug_bus = NULL;
	ret = g->ops.bus.init_hw(g);
	assert(nvgpu_readl(g, bus_intr_en_1_r()) == 0U);
	assert(nvgpu_readl(g, bus_debug_sel_0_r()) == 0xFU);
	assert(nvgpu_readl(g, bus_debug_sel_1_r()) == 0xFU);
	assert(nvgpu_readl(g, bus_debug_sel_2_r()) == 0xFU);
	assert(nvgpu_readl(g, bus_debug_sel_3_r()) == 0xFU);

	p->is_silicon = true;
	g->ops.bus.configure_debug_bus = gv11b_bus_configure_debug_bus;
	ret = g->ops.bus.init_hw(g);
	assert(nvgpu_readl(g, bus_intr_en_1_r()) == 0xEU);
	assert(nvgpu_readl(g, bus_debug_sel_0_r()) == 0x0U);
	assert(nvgpu_readl(g, bus_debug_sel_1_r()) == 0x0U);
	assert(nvgpu_readl(g, bus_debug_sel_2_r()) == 0x0U);
	assert(nvgpu_readl(g, bus_debug_sel_3_r()) == 0x0U);

	p->is_fpga = true;
	p->is_silicon = false;
	ret = g->ops.bus.init_hw(g);
	assert(nvgpu_readl(g, bus_intr_en_1_r()) == 0xEU);
	ret = UNIT_SUCCESS;
done:
	return ret;
}

int test_bar_bind(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	struct nvgpu_mem bar_inst = {0};

	/* Initialize cpu_va to a known value */
	bar_inst.cpu_va = (void *) 0xCE418000U;
	bar_inst.aperture = APERTURE_VIDMEM;
	/* Set bus_bind_status_r to 0xF that is both bar1 and bar2 status
	 * pending and outstanding.
	 */
	nvgpu_posix_io_writel_reg_space(g, bus_bind_status_r(), 0xFU);

	/* Call bus.bar1_bind() HAL */
	ret = g->ops.bus.bar1_bind(g, &bar_inst);

	/* Make sure HAL returns success as bind_status is marked as done in
	 * third polling attempt.
	 */
	if (ret != 0U) {
		unit_err(m, "bus.bar1_bind HAL failed.\n");
		ret = UNIT_FAIL;
	}

	/* Send error if bar1_block register is not set as expected:
	 * Bit 27:0 - 4k aligned block pointer = bar_inst.cpu_va >> 12 = 0xCE418
	 * Bit 29:28- Target = Vidmem = (00)b
	 * Bit 30   - Debug CYA = (0)b
	 * Bit 31   - Mode = virtual = (1)b
	 */
	assert(nvgpu_readl(g, bus_bar1_block_r()) == 0x800CE418U);

	/* Call bus.bar1_bind HAL again and except ret != 0 as the bind status
	 * will remain outstanding during this call.
	 */
	nvgpu_posix_io_writel_reg_space(g, bus_bind_status_r(), 0x5U);
	ret = g->ops.bus.bar1_bind(g, &bar_inst);
	/* The HAL should return error this time as timeout is expected to
	 * expire.
	 */
	if (ret != -EINVAL) {
		unit_err(m, "bus.bar1_bind did not fail as expected.\n");
		ret = UNIT_FAIL;
	}

	/* Call bus.bar1_bind HAL again and except ret != 0 as the bind status
	 * will remain pending during this call.
	 */
	nvgpu_posix_io_writel_reg_space(g, bus_bind_status_r(), 0xAU);
	ret = g->ops.bus.bar1_bind(g, &bar_inst);
	/* The HAL should return error this time as timeout is expected to
	 * expire.
	 */
	if (ret != -EINVAL) {
		unit_err(m, "bus.bar1_bind did not fail as expected.\n");
		ret = UNIT_FAIL;
	}

	bar_inst.cpu_va = (void *) 0x2670C000U;
	read_bind_status_reg = 0U;
	ret = g->ops.bus.bar2_bind(g, &bar_inst);
	if (ret != 0U) {
		unit_err(m, "bus.bar2_bind HAL failed.\n");
		ret = UNIT_FAIL;
	}
	assert(nvgpu_readl(g, bus_bar2_block_r()) == 0x8002670CU);

	/* Call bus.bar2_bind HAL again and except ret != 0 as the bind status
	 * will remain outstanding during this call.
	 */
	nvgpu_posix_io_writel_reg_space(g, bus_bind_status_r(), 0x5U);
	ret = g->ops.bus.bar2_bind(g, &bar_inst);
	if (ret != -EINVAL) {
		unit_err(m, "bus.bar2_bind did not fail as expected.\n");
		ret = UNIT_FAIL;
	}

	/* Call bus.bar2_bind HAL again and except ret != 0 as the bind status
	 * will remain pending during this call.
	 */
	nvgpu_posix_io_writel_reg_space(g, bus_bind_status_r(), 0xAU);
	ret = g->ops.bus.bar2_bind(g, &bar_inst);
	if (ret != -EINVAL) {
		unit_err(m, "bus.bar2_bind did not fail as expected.\n");
		ret = UNIT_FAIL;
	}

	ret = UNIT_SUCCESS;
done:
	return ret;
}

int test_bus_isr(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_SUCCESS;

	nvgpu_writel(g, bus_intr_0_r(), bus_intr_0_pri_squash_m());
	g->ops.bus.isr(g);

	nvgpu_writel(g, bus_intr_0_r(), bus_intr_0_pri_fecserr_m());
	g->ops.bus.isr(g);

	nvgpu_writel(g, bus_intr_0_r(), bus_intr_0_pri_timeout_m());
	g->ops.bus.isr(g);

	nvgpu_writel(g, bus_intr_0_r(), 0x10U);
	g->ops.bus.isr(g);

	return ret;
}

struct unit_module_test bus_tests[] = {
	UNIT_TEST(bus_setup,	       test_bus_setup,             NULL, 0),
	UNIT_TEST(bus_init_hw,         test_init_hw,               NULL, 0),
	UNIT_TEST(bus_bar_bind,        test_bar_bind,              NULL, 0),
	UNIT_TEST(bus_isr,             test_bus_isr,               NULL, 0),
	UNIT_TEST(bus_free_reg_space,  test_bus_free_reg_space,    NULL, 0),
};

UNIT_MODULE(bus, bus_tests, UNIT_PRIO_NVGPU_TEST);
