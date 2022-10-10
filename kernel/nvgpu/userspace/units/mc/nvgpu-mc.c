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
#include <nvgpu/mc.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/cic_rm.h>
#include <nvgpu/hal_init.h>
#include <nvgpu/device.h>
#include <nvgpu/engines.h>
#include <nvgpu/ltc.h>
#include <nvgpu/hw/gp10b/hw_mc_gp10b.h>

#include <nvgpu/posix/cond.h>
#include <nvgpu/posix/posix-fault-injection.h>

#include "nvgpu-mc.h"

#define MC_ADDR_SPACE_START	0x00000000
#define MC_ADDR_SPACE_SIZE	0xfff

/* value for GV11B */
#define MC_BOOT_0_GV11B (NVGPU_GPUID_GV11B << 20)
/* to set the security fuses */
#define GP10B_FUSE_REG_BASE		0x00021000U
#define GP10B_FUSE_OPT_PRIV_SEC_EN	(GP10B_FUSE_REG_BASE+0x434U)
#define ACTIVE_GR_ID 1
#define ACTIVE_CE_ID 2

#define STALL_EN_REG			mc_intr_en_r(NVGPU_CIC_INTR_STALLING)
#define NONSTALL_EN_REG			mc_intr_en_r(NVGPU_CIC_INTR_NONSTALLING)

#define STALL_EN_SET_REG		mc_intr_en_set_r(NVGPU_CIC_INTR_STALLING)
#define STALL_EN_CLEAR_REG		mc_intr_en_clear_r(NVGPU_CIC_INTR_STALLING)
#define NONSTALL_EN_SET_REG		mc_intr_en_set_r(NVGPU_CIC_INTR_NONSTALLING)
#define NONSTALL_EN_CLEAR_REG		mc_intr_en_clear_r(NVGPU_CIC_INTR_NONSTALLING)

#define STALL_PENDING_REG		mc_intr_r(NVGPU_CIC_INTR_STALLING)
#define NONSTALL_PENDING_REG		mc_intr_r(NVGPU_CIC_INTR_NONSTALLING)

struct mc_unit {
	u32 num;
	u32 bit;
};
static struct mc_unit mc_units[] = {
	{ NVGPU_CIC_INTR_UNIT_BUS, mc_intr_pbus_pending_f() },
	{ NVGPU_CIC_INTR_UNIT_PRIV_RING, mc_intr_priv_ring_pending_f() },
	{ NVGPU_CIC_INTR_UNIT_FIFO, mc_intr_pfifo_pending_f() },
	{ NVGPU_CIC_INTR_UNIT_LTC, mc_intr_ltc_pending_f() },
	{ NVGPU_CIC_INTR_UNIT_HUB, mc_intr_replayable_fault_pending_f() },
	{ NVGPU_CIC_INTR_UNIT_GR, (1 << ACTIVE_GR_ID) /* nvgpu_gr_engine_interrupt_mask()  */},
	{ NVGPU_CIC_INTR_UNIT_PMU, mc_intr_pmu_pending_f() },
	{ NVGPU_CIC_INTR_UNIT_CE, (1 << ACTIVE_CE_ID) /* nvgpu_ce_engine_interrupt_mask() */ },
};
#define NUM_MC_UNITS ARRAY_SIZE(mc_units)

#define INVALID_UNIT 100
/*
 * Mock I/O
 */

/*
 * Write callback. Forward the write access to the mock IO framework.
 */
static void writel_access_reg_fn(struct gk20a *g,
				 struct nvgpu_reg_access *access)
{
	u32 value;

	if (access->addr == STALL_EN_SET_REG) {
		value = nvgpu_posix_io_readl_reg_space(g, STALL_EN_REG) | access->value;
		nvgpu_posix_io_writel_reg_space(g, STALL_EN_REG, value);
	}

	if (access->addr == NONSTALL_EN_SET_REG) {
		value = nvgpu_posix_io_readl_reg_space(g, NONSTALL_EN_REG) | access->value;
		nvgpu_posix_io_writel_reg_space(g, NONSTALL_EN_REG, value);
	}


	if (access->addr == STALL_EN_CLEAR_REG) {
		value = nvgpu_posix_io_readl_reg_space(g, STALL_EN_REG) & ~access->value;
		nvgpu_posix_io_writel_reg_space(g, STALL_EN_REG, value);
	}

	if (access->addr == NONSTALL_EN_CLEAR_REG) {
		value = nvgpu_posix_io_readl_reg_space(g, NONSTALL_EN_REG) & ~access->value;
		nvgpu_posix_io_writel_reg_space(g, NONSTALL_EN_REG, value);
	}

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

struct unit_ctx {
	bool bus_isr;
	bool ce_isr;
	bool fb_isr;
	bool fifo_isr;
	bool gr_isr;
	bool ltc_isr;
	bool pmu_isr;
	bool priv_ring_isr;

	u32 ce_isr_return;
	u32 fifo_isr_return;
	int gr_isr_return;
};

static struct unit_ctx u;

static void reset_ctx(void)
{
	u.bus_isr = false;
	u.ce_isr = false;
	u.fb_isr = false;
	u.fifo_isr = false;
	u.gr_isr = false;
	u.gr_isr_return = 0;
	u.ltc_isr = false;
	u.pmu_isr = false;
	u.priv_ring_isr = false;
}

static void mock_bus_isr(struct gk20a *g)
{
	u.bus_isr = true;
}

static void mock_ce_stall_isr(struct gk20a *g, u32 inst_id, u32 pri_base,
				bool *needs_rc, bool *needs_quiesce)
{
	u.ce_isr = true;
}

#ifdef CONFIG_NVGPU_NONSTALL_INTR
static u32 mock_ce_nonstall_isr(struct gk20a *g, u32 inst_id, u32 pri_base)
{
	u.ce_isr = true;
	return u.ce_isr_return;
}
#endif

static void mock_fb_isr(struct gk20a *g, u32 intr_unit_bitmask)
{
	u.fb_isr = true;
}

static void mock_fifo_stall_isr(struct gk20a *g)
{
	u.fifo_isr = true;
}

static u32 mock_fifo_nonstall_isr(struct gk20a *g)
{
	u.fifo_isr = true;
	return u.fifo_isr_return;
}

static u32 mock_gr_nonstall_isr(struct gk20a *g)
{
	u.gr_isr = true;
	return (u32)u.gr_isr_return;
}

static int mock_gr_stall_isr(struct gk20a *g)
{
	u.gr_isr = true;
	return u.gr_isr_return;
}

static void mock_ltc_isr(struct gk20a *g, u32 ltc)
{
	u.ltc_isr = true;
}

static void mock_pmu_isr(struct gk20a *g)
{
	u.pmu_isr = true;
}

static void mock_priv_ring_isr(struct gk20a *g)
{
	u.priv_ring_isr = true;
}

int test_mc_setup_env(struct unit_module *m,
			  struct gk20a *g, void *args)
{
	/* Create mc register space */
	if (nvgpu_posix_io_add_reg_space(g, MC_ADDR_SPACE_START,
						MC_ADDR_SPACE_SIZE) != 0) {
		unit_err(m, "%s: failed to create register space\n",
			 __func__);
		return UNIT_FAIL;
	}
	/* Create fuse register space */
	if (nvgpu_posix_io_add_reg_space(g, GP10B_FUSE_REG_BASE, 0xfff) != 0) {
		unit_err(m, "%s: failed to create register space\n",
			 __func__);
		return UNIT_FAIL;
	}
	(void)nvgpu_posix_register_io(g, &test_reg_callbacks);

	nvgpu_posix_io_writel_reg_space(g, mc_boot_0_r(), MC_BOOT_0_GV11B);
	nvgpu_posix_io_writel_reg_space(g, GP10B_FUSE_OPT_PRIV_SEC_EN, 0x0);

	if (nvgpu_detect_chip(g) != 0) {
		unit_err(m, "%s: failed to init HAL\n", __func__);
		return UNIT_FAIL;
	}

	/* override HALs */
	g->ops.bus.isr = mock_bus_isr;
	g->ops.ce.isr_stall = mock_ce_stall_isr;
#ifdef CONFIG_NVGPU_NONSTALL_INTR
	g->ops.ce.isr_nonstall = mock_ce_nonstall_isr;
#endif
	g->ops.fb.intr.isr = mock_fb_isr;
	g->ops.fifo.intr_0_isr = mock_fifo_stall_isr;
	g->ops.fifo.intr_1_isr = mock_fifo_nonstall_isr;
	g->ops.gr.intr.stall_isr = mock_gr_stall_isr;
	g->ops.gr.intr.nonstall_isr = mock_gr_nonstall_isr;
	g->ops.ltc.intr.isr = mock_ltc_isr;
	g->ops.pmu.pmu_isr = mock_pmu_isr;
	g->ops.priv_ring.isr = mock_priv_ring_isr;

	nvgpu_device_init(g);

	/* setup engines for getting interrupt info */
	g->fifo.g = g;
	if (nvgpu_engine_setup_sw(g) != 0) {
		unit_return_fail(m, "failed to setup engines\n");
	}

	/* setup LTC just enough */
	g->ltc = nvgpu_kzalloc(g, sizeof(struct nvgpu_ltc));
	if (g->ltc == NULL) {
		unit_return_fail(m, "failed to alloc\n");
	}
	g->ltc->ltc_count = 1;

	return UNIT_SUCCESS;
}

int test_mc_free_env(struct unit_module *m, struct gk20a *g, void *args)
{
	/* Free mc register space */
	nvgpu_posix_io_delete_reg_space(g, MC_ADDR_SPACE_START);
	nvgpu_posix_io_delete_reg_space(g, GP10B_FUSE_REG_BASE);

	nvgpu_engine_cleanup_sw(g);

	nvgpu_kfree(g, g->ltc);

	return UNIT_SUCCESS;
}

int test_unit_config(struct unit_module *m, struct gk20a *g, void *args)
{
	u32 invalid_units[] = {NUM_MC_UNITS, INVALID_UNIT, U32_MAX};
	u32 i;
	u32 unit;
	u32 val;

	/* clear regs */
	nvgpu_posix_io_writel_reg_space(g, STALL_EN_REG, 0x0);
	nvgpu_posix_io_writel_reg_space(g, NONSTALL_EN_REG, 0x0);

	nvgpu_posix_io_writel_reg_space(g, STALL_EN_SET_REG, 0x0);
	nvgpu_posix_io_writel_reg_space(g, STALL_EN_CLEAR_REG, 0x0);
	nvgpu_posix_io_writel_reg_space(g, NONSTALL_EN_SET_REG, 0x0);
	nvgpu_posix_io_writel_reg_space(g, NONSTALL_EN_CLEAR_REG, 0x0);

	for (i = 0; i < NUM_MC_UNITS; i++) {
		unit = mc_units[i].num;

		/* enable stall intr */
		nvgpu_cic_mon_intr_stall_unit_config(g, unit, true);
		val = nvgpu_posix_io_readl_reg_space(g, STALL_EN_REG);
		if ((val & mc_units[i].bit) == 0) {
			unit_return_fail(m, "failed to enable stall intr for unit %u val=0x%08x\n",
					unit, val);
		}

		/* disable stall intr */
		nvgpu_cic_mon_intr_stall_unit_config(g, unit, false);
		val = nvgpu_posix_io_readl_reg_space(g, STALL_EN_REG);
		if ((val & mc_units[i].bit) != 0) {
			unit_return_fail(m, "failed to disable stall intr for unit %u val=0x%08x\n",
					unit, val);
		}
#ifdef CONFIG_NVGPU_NONSTALL_INTR
		/* enable nonstall intr */
		nvgpu_cic_mon_intr_nonstall_unit_config(g, unit, true);
		val = nvgpu_posix_io_readl_reg_space(g, NONSTALL_EN_REG);
		if ((val & mc_units[i].bit) == 0) {
			unit_return_fail(m, "failed to enable nonstall intr for unit %u val=0x%08x\n",
					unit, val);
		}

		/* disable stall intr */
		nvgpu_cic_mon_intr_nonstall_unit_config(g, unit, false);
		val = nvgpu_posix_io_readl_reg_space(g, NONSTALL_EN_REG);
		if ((val & mc_units[i].bit) != 0) {
			unit_return_fail(m, "failed to disable nonstall intr for unit %u val=0x%08x\n",
					unit, val);
		}
#endif
	}

	for (i = 0; i < ARRAY_SIZE(invalid_units); i++) {
		/* negative testing - invalid unit enable set - stall */
		nvgpu_posix_io_writel_reg_space(g, STALL_EN_REG, 0x0); /* clear en reg */
		nvgpu_cic_mon_intr_stall_unit_config(g, invalid_units[i], true);
		val = nvgpu_posix_io_readl_reg_space(g, STALL_EN_REG);
		if (val != 0U) {
			unit_return_fail(m, "Incorrectly enabled stall interrupt for invalid unit, val=0x%08x\n",
					 val);
		}

		/* negative testing - invalid unit enable clear - stall */
		nvgpu_posix_io_writel_reg_space(g, STALL_EN_REG, U32_MAX); /* set en reg */
		nvgpu_cic_mon_intr_stall_unit_config(g, invalid_units[i], false);
		val = nvgpu_posix_io_readl_reg_space(g, STALL_EN_REG);
		if (val != U32_MAX) {
			unit_return_fail(m, "Incorrectly disabled stall interrupt for invalid unit, val=0x%08x\n",
					 val);
		}

#ifdef CONFIG_NVGPU_NONSTALL_INTR
		/* negative testing - invalid unit enable set - nonstall */
		nvgpu_posix_io_writel_reg_space(g, NONSTALL_EN_REG, 0x0); /* clear en reg */
		nvgpu_cic_mon_intr_nonstall_unit_config(g, invalid_units[i], true);
		val = nvgpu_posix_io_readl_reg_space(g, NONSTALL_EN_REG);
		if (val != 0U) {
			unit_return_fail(m, "Incorrectly enabled non-stall interrupt for invalid unit, val=0x%08x\n",
					 val);
		}

		/* negative testing - invalid unit enable clear - nonstall */
		nvgpu_posix_io_writel_reg_space(g, NONSTALL_EN_REG, U32_MAX); /* set en reg */
		nvgpu_cic_mon_intr_nonstall_unit_config(g, invalid_units[i], false);
		val = nvgpu_posix_io_readl_reg_space(g, NONSTALL_EN_REG);
		if (val != U32_MAX) {
			unit_return_fail(m, "Incorrectly enabled non-stall interrupt for invalid unit, val=0x%08x\n",
					 val);
		}
#endif
	}

	return UNIT_SUCCESS;
}

int test_pause_resume_mask(struct unit_module *m, struct gk20a *g, void *args)
{
	u32 val;
	u32 expected_stall_val = mc_intr_priv_ring_pending_f();
#ifdef CONFIG_NVGPU_NONSTALL_INTR
	u32 expected_nonstall_val = mc_intr_pbus_pending_f();
#endif
	void (*save_func)(struct gk20a *g);

	/* clear regs */
	nvgpu_posix_io_writel_reg_space(g, STALL_EN_REG, 0x0);
	nvgpu_posix_io_writel_reg_space(g, NONSTALL_EN_REG, 0x0);

	nvgpu_posix_io_writel_reg_space(g, STALL_EN_SET_REG, 0x0);
	nvgpu_posix_io_writel_reg_space(g, STALL_EN_CLEAR_REG, 0x0);
	nvgpu_posix_io_writel_reg_space(g, NONSTALL_EN_SET_REG, 0x0);
	nvgpu_posix_io_writel_reg_space(g, NONSTALL_EN_CLEAR_REG, 0x0);

	/* cleanup anything from previous tests */
	g->mc.intr_mask_restore[0] = 0U;
	g->mc.intr_mask_restore[1] = 0U;

	/* enable something to pause and resume */
	nvgpu_cic_mon_intr_stall_unit_config(g, NVGPU_CIC_INTR_UNIT_PRIV_RING, true);
#ifdef CONFIG_NVGPU_NONSTALL_INTR
	nvgpu_cic_mon_intr_nonstall_unit_config(g, NVGPU_CIC_INTR_UNIT_BUS, true);
#endif

	/* pause stall */
	nvgpu_cic_mon_intr_stall_pause(g);
	val = nvgpu_posix_io_readl_reg_space(g, STALL_EN_REG);
	if (val != 0) {
		unit_return_fail(m, "failed to pause stall intr\n");
	}

#ifdef CONFIG_NVGPU_NONSTALL_INTR
	/* pause nonstall */
	nvgpu_cic_mon_intr_nonstall_pause(g);
	val = nvgpu_posix_io_readl_reg_space(g, NONSTALL_EN_REG);
	if (val != 0) {
		unit_return_fail(m, "failed to pause nonstall intr\n");
	}
#endif

	/* resume stall */
	nvgpu_posix_io_writel_reg_space(g, STALL_EN_SET_REG, 0x0);
	nvgpu_cic_mon_intr_stall_resume(g);
	val = nvgpu_posix_io_readl_reg_space(g, STALL_EN_REG);
	if (val != expected_stall_val) {
		unit_return_fail(m, "failed to resume stall intr\n");
	}

#ifdef CONFIG_NVGPU_NONSTALL_INTR
	/* resume nonstall */
	nvgpu_posix_io_writel_reg_space(g, NONSTALL_EN_SET_REG, 0x0);
	nvgpu_cic_mon_intr_nonstall_resume(g);
	val = nvgpu_posix_io_readl_reg_space(g, NONSTALL_EN_REG);
	if (val != expected_nonstall_val) {
		unit_return_fail(m, "failed to resume nonstall intr\n");
	}
#endif

	/* clear regs */
	nvgpu_posix_io_writel_reg_space(g, STALL_EN_CLEAR_REG, 0x0);
#ifdef CONFIG_NVGPU_NONSTALL_INTR
	nvgpu_posix_io_writel_reg_space(g, NONSTALL_EN_CLEAR_REG, 0x0);
#endif

	/* mask all */
	nvgpu_cic_mon_intr_mask(g);
	val = nvgpu_posix_io_readl_reg_space(g, STALL_EN_REG);
	if (val != 0) {
		unit_return_fail(m, "failed to mask stall intr\n");
	}
	val = nvgpu_posix_io_readl_reg_space(g, NONSTALL_EN_REG);
	if (val != 0) {
		unit_return_fail(m, "failed to mask nonstall intr\n");
	}

	/* make this HAL NULL for branch coverage */
	save_func = g->ops.mc.intr_mask;
	g->ops.mc.intr_mask = NULL;
	nvgpu_cic_mon_intr_mask(g);
	g->ops.mc.intr_mask = save_func;

	return UNIT_SUCCESS;
}

int test_intr_stall(struct unit_module *m, struct gk20a *g, void *args)
{
	u32 i, pend, val;

	for (i = 0; i < 32; i++) {
		pend = (1 << i);
		nvgpu_posix_io_writel_reg_space(g, STALL_PENDING_REG, pend);
		val = g->ops.mc.intr_stall(g);
		if (val != pend) {
			unit_return_fail(m, "incorrect stall value returned\n");
		}
	}

	return UNIT_SUCCESS;
}

int test_is_stall_and_eng_intr_pending(struct unit_module *m, struct gk20a *g,
					void *args)
{
	u32 act_eng_id = 0; /* GR engine */
	u32 eng_intr_pending = 0;
	u32 intrs_pending = 0;
	u32 expected_eng_intr_pending = 0;
	bool result;
	unsigned int i;

	/* test with nothing pending */
	nvgpu_posix_io_writel_reg_space(g, STALL_PENDING_REG, 0);
	result = g->ops.mc.is_stall_and_eng_intr_pending(g, act_eng_id,
							&eng_intr_pending);
	if (result) {
		unit_return_fail(m, "incorrect value returned\n");
	}

	/* test with everything pending */
	for (i = 0U; i < NUM_MC_UNITS; i++) {
		intrs_pending |= mc_units[i].bit;
		if (mc_units[i].num == NVGPU_CIC_INTR_UNIT_GR) {
			expected_eng_intr_pending = mc_units[i].bit;
		}
	}
	nvgpu_posix_io_writel_reg_space(g, STALL_PENDING_REG, intrs_pending);
	result = g->ops.mc.is_stall_and_eng_intr_pending(g, act_eng_id,
							&eng_intr_pending);
	if (!result || (eng_intr_pending != expected_eng_intr_pending)) {
		unit_return_fail(m, "incorrect value returned\n");
	}

	return UNIT_SUCCESS;
}

int test_isr_stall(struct unit_module *m, struct gk20a *g, void *args)
{
	u32 intrs_pending = 0;
	u32 i;
	bool (*save_intr_hub_pending)(struct gk20a *g, u32 intr);

	/* for branch coverage, test with nothing pending */
	nvgpu_posix_io_writel_reg_space(g, STALL_PENDING_REG, 0);
	reset_ctx();
	g->ops.mc.isr_stall(g);
	if (u.bus_isr || u.ce_isr || u.fb_isr || u.fifo_isr || u.gr_isr ||
	    u.pmu_isr || u.priv_ring_isr) {
		unit_return_fail(m, "unexpected ISR called\n");
	}

	/* setup regs for basic test with all units intr pending */
	for (i = 0; i < NUM_MC_UNITS; i++) {
		intrs_pending |= mc_units[i].bit;
	}
	nvgpu_posix_io_writel_reg_space(g, STALL_PENDING_REG, intrs_pending);
	nvgpu_posix_io_writel_reg_space(g, mc_intr_ltc_r(), 1U);
	reset_ctx();
	g->ops.mc.isr_stall(g);
	if (u.bus_isr) {
		unit_return_fail(m, "BUS ISR called from Stall\n");
	}
	if (!u.ce_isr || !u.fb_isr || !u.fifo_isr || !u.gr_isr ||
	    !u.pmu_isr || !u.priv_ring_isr) {
		unit_return_fail(m, "not all ISRs called\n");
	}

	/* for branch coverage set this HAL to NULL */
	save_intr_hub_pending = g->ops.mc.is_intr_hub_pending;
	g->ops.mc.is_intr_hub_pending = NULL;
	for (i = 0; i < NUM_MC_UNITS; i++) {
		intrs_pending |= mc_units[i].bit;
	}
	nvgpu_posix_io_writel_reg_space(g, STALL_PENDING_REG, intrs_pending);
	reset_ctx();
	g->ops.mc.isr_stall(g);
	if (u.fb_isr) {
		unit_return_fail(m, "unexpected ISR called\n");
	}
	g->ops.mc.is_intr_hub_pending = save_intr_hub_pending;

	/* for branch coverage return error from GR ISR */
	for (i = 0; i < NUM_MC_UNITS; i++) {
		intrs_pending |= mc_units[i].bit;
	}
	nvgpu_posix_io_writel_reg_space(g, STALL_PENDING_REG, intrs_pending);
	reset_ctx();
	u.gr_isr_return = -1;
	g->ops.mc.isr_stall(g);

	/* for branch coverage set this HAL to NULL */
	g->ops.ce.isr_stall = NULL;
	for (i = 0; i < NUM_MC_UNITS; i++) {
		intrs_pending |= mc_units[i].bit;
	}
	nvgpu_posix_io_writel_reg_space(g, STALL_PENDING_REG, intrs_pending);
	reset_ctx();
	g->ops.mc.isr_stall(g);
	g->ops.ce.isr_stall = mock_ce_stall_isr;

	/*
	 * for branch coverage set LTC intr in main intr reg, but not ltc
	 * intr reg
	 */
	for (i = 0; i < NUM_MC_UNITS; i++) {
		intrs_pending |= mc_units[i].bit;
	}
	nvgpu_posix_io_writel_reg_space(g, STALL_PENDING_REG, intrs_pending);
	nvgpu_posix_io_writel_reg_space(g, mc_intr_ltc_r(), 0U);
	reset_ctx();
	g->ops.mc.isr_stall(g);
	if (u.ltc_isr) {
		unit_return_fail(m, "unexpected ISR called\n");
	}

	return UNIT_SUCCESS;
}

int test_is_intr1_pending(struct unit_module *m, struct gk20a *g, void *args)
{
	struct match_struct {
		u32 unit;
		u32 mask;
		bool expect;
	};
	const struct match_struct match_table[] = {
		{ NVGPU_UNIT_FIFO, ~mc_enable_pfifo_enabled_f(), false },
		{ NVGPU_UNIT_FIFO, mc_enable_pfifo_enabled_f(), true },
		{ INVALID_UNIT, 0x0, false },
	};
	unsigned int i;
	bool val;

	for (i = 0; i < ARRAY_SIZE(match_table); i++) {
		val = g->ops.mc.is_intr1_pending(g, match_table[i].unit,
						match_table[i].mask);
		if (val != match_table[i].expect) {
			unit_return_fail(m, "incorrect stall value returned\n");
		}
	}

	return UNIT_SUCCESS;
}

int test_isr_nonstall(struct unit_module *m, struct gk20a *g, void *args)
{
	u32 intrs_pending = 0;
	u32 i;
	u32 val;

	/* for branch coverage, test with nothing pending */
	nvgpu_posix_io_writel_reg_space(g, NONSTALL_PENDING_REG, 0);
	reset_ctx();
	val = g->ops.mc.isr_nonstall(g);
	if (u.bus_isr || u.ce_isr || u.fb_isr || u.fifo_isr || u.gr_isr ||
	    u.priv_ring_isr) {
		unit_return_fail(m, "unexpected ISR called\n");
	}

	/* setup regs for basic test with all units intr pending */
	for (i = 0; i < NUM_MC_UNITS; i++) {
		intrs_pending |= mc_units[i].bit;
	}
	nvgpu_posix_io_writel_reg_space(g, NONSTALL_PENDING_REG, intrs_pending);
	reset_ctx();
	u.ce_isr_return = 0x1;
	u.fifo_isr_return = 0x2;
	u.gr_isr_return = 0x4;
	val = g->ops.mc.isr_nonstall(g);
	if (!u.bus_isr || !u.ce_isr || !u.fifo_isr || !u.gr_isr) {
		unit_return_fail(m, "not all ISRs called\n");
	}
	if (val != 0x7) {
		unit_return_fail(m, "incorrect ops returned 0x%08x\n", val);
	}

	/* for branch coverage set this HAL to NULL */
#ifdef CONFIG_NVGPU_NONSTALL_INTR
	g->ops.ce.isr_nonstall = NULL;
#endif
	for (i = 0; i < NUM_MC_UNITS; i++) {
		intrs_pending |= mc_units[i].bit;
	}
	nvgpu_posix_io_writel_reg_space(g, NONSTALL_PENDING_REG, intrs_pending);
	reset_ctx();
	g->ops.mc.isr_nonstall(g);
#ifdef CONFIG_NVGPU_NONSTALL_INTR
	g->ops.ce.isr_nonstall = mock_ce_nonstall_isr;
#endif
	return UNIT_SUCCESS;
}

int test_enable_disable_reset(struct unit_module *m, struct gk20a *g, void *args)
{
	u32 units = (mc_enable_pfifo_enabled_f() |
		     mc_enable_pgraph_enabled_f() |
		     mc_enable_blg_enabled_f() | mc_enable_ce2_enabled_f());
	u32 val;

	/* test enable */
	nvgpu_posix_io_writel_reg_space(g, mc_enable_r(), 0);
	g->ops.mc.enable_units(g, (NVGPU_UNIT_FIFO | NVGPU_UNIT_GRAPH |
		     NVGPU_UNIT_BLG | NVGPU_UNIT_CE2), true);
	val = nvgpu_posix_io_readl_reg_space(g, mc_enable_r());
	if (val != units) {
		unit_return_fail(m, "failed to reset units val=0x%08x\n", val);
	}

	/* test disable */
	g->ops.mc.enable_units(g, (NVGPU_UNIT_FIFO | NVGPU_UNIT_GRAPH |
		     NVGPU_UNIT_BLG | NVGPU_UNIT_CE2), false);
	val = nvgpu_posix_io_readl_reg_space(g, mc_enable_r());
	if (val != 0U) {
		unit_return_fail(m, "failed to reset units val=0x%08x\n", val);
	}

	/* test reset */
	nvgpu_posix_io_writel_reg_space(g, mc_enable_r(), units);
	nvgpu_mc_reset_units(g, (NVGPU_UNIT_FIFO | NVGPU_UNIT_GRAPH |
		     NVGPU_UNIT_BLG | NVGPU_UNIT_CE2));
	val = nvgpu_posix_io_readl_reg_space(g, mc_enable_r());
	if (val != units) {
		unit_return_fail(m, "failed to reset units val=0x%08x\n", val);
	}

	return UNIT_SUCCESS;
}

struct unit_module_test mc_tests[] = {
	UNIT_TEST(mc_setup_env,			test_mc_setup_env,			NULL, 0),
	UNIT_TEST(unit_config,			test_unit_config,			NULL, 2),
	UNIT_TEST(pause_resume_mask,		test_pause_resume_mask,			NULL, 0),
	UNIT_TEST(intr_stall,			test_intr_stall,			NULL, 0),
	UNIT_TEST(intr_is_stall_and_eng_intr_pending,
						test_is_stall_and_eng_intr_pending,	NULL, 2),
	UNIT_TEST(isr_stall,			test_isr_stall,				NULL, 2),
	UNIT_TEST(isr_nonstall,			test_isr_nonstall,			NULL, 2),
	UNIT_TEST(is_intr1_pending,		test_is_intr1_pending,			NULL, 0),
	UNIT_TEST(enable_disable_reset,		test_enable_disable_reset,		NULL, 0),
	UNIT_TEST(mc_free_env,			test_mc_free_env,			NULL, 0),
};

UNIT_MODULE(mc, mc_tests, UNIT_PRIO_NVGPU_TEST);
