/*
 * GM20B Master Control
 *
 * Copyright (c) 2014-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/timers.h>
#include <nvgpu/atomic.h>
#include <nvgpu/io.h>
#include <nvgpu/mc.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/ltc.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/bug.h>
#include <nvgpu/engines.h>
#include <nvgpu/device.h>
#include <nvgpu/power_features/pg.h>
#include <nvgpu/ce.h>

#include "mc_gm20b.h"

#include <nvgpu/hw/gm20b/hw_mc_gm20b.h>

void gm20b_mc_isr_stall(struct gk20a *g)
{
	u32 mc_intr_0;
	u32 i;
	const struct nvgpu_device *dev;

	mc_intr_0 = g->ops.mc.intr_stall(g);

	nvgpu_log(g, gpu_dbg_intr, "stall intr %08x", mc_intr_0);

	for (i = 0U; i < g->fifo.num_engines; i++) {
		dev = g->fifo.active_engines[i];

		if ((mc_intr_0 & BIT32(dev->intr_id)) == 0U) {
			continue;
		}

		/* GR Engine */
		if (nvgpu_device_is_graphics(g, dev)) {
			nvgpu_pg_elpg_protected_call(g,
						g->ops.gr.intr.stall_isr(g));
		}

		/* CE Engine */
		if (nvgpu_device_is_ce(g, dev)) {
			nvgpu_ce_stall_isr(g, dev->inst_id, dev->pri_base);
		}
	}

	if ((mc_intr_0 & mc_intr_pfifo_pending_f()) != 0U) {
		g->ops.fifo.intr_0_isr(g);
	}
#ifdef CONFIG_NVGPU_LS_PMU
	if ((mc_intr_0 & mc_intr_pmu_pending_f()) != 0U) {
		g->ops.pmu.pmu_isr(g);
	}
#endif
	if ((mc_intr_0 & mc_intr_priv_ring_pending_f()) != 0U) {
		g->ops.priv_ring.isr(g);
	}
	if ((mc_intr_0 & mc_intr_ltc_pending_f()) != 0U) {
		g->ops.mc.ltc_isr(g);
	}
}

void gm20b_mc_intr_mask(struct gk20a *g)
{
	nvgpu_writel(g, mc_intr_en_0_r(),
		mc_intr_en_0_inta_disabled_f());
	nvgpu_writel(g, mc_intr_en_1_r(),
		mc_intr_en_1_inta_disabled_f());
	nvgpu_writel(g, mc_intr_mask_0_r(), 0);
	nvgpu_writel(g, mc_intr_mask_1_r(), 0);
}

void gm20b_mc_intr_enable(struct gk20a *g)
{
	nvgpu_writel(g, mc_intr_en_1_r(),
		mc_intr_en_1_inta_hardware_f());

	nvgpu_writel(g, mc_intr_en_0_r(),
		mc_intr_en_0_inta_hardware_f());
}

static u32 gm20b_mc_intr_pending_f(struct gk20a *g, u32 unit)
{
	u32 intr_pending_f = 0;

	switch (unit) {
	case NVGPU_CIC_INTR_UNIT_BUS:
		intr_pending_f = mc_intr_pbus_pending_f();
		break;
	case NVGPU_CIC_INTR_UNIT_PRIV_RING:
		intr_pending_f = mc_intr_priv_ring_pending_f();
		break;
	case NVGPU_CIC_INTR_UNIT_FIFO:
		intr_pending_f = mc_intr_pfifo_pending_f();
		break;
	case NVGPU_CIC_INTR_UNIT_LTC:
		intr_pending_f = mc_intr_ltc_pending_f();
		break;
	case NVGPU_CIC_INTR_UNIT_GR:
		intr_pending_f = nvgpu_gr_engine_interrupt_mask(g);
		break;
	case NVGPU_CIC_INTR_UNIT_PMU:
		intr_pending_f = mc_intr_mask_0_pmu_enabled_f();
		break;
	case NVGPU_CIC_INTR_UNIT_CE:
		intr_pending_f = nvgpu_ce_engine_interrupt_mask(g);
		break;
	default:
		nvgpu_err(g, "Invalid MC interrupt unit specified !!!");
		break;
	}

	return intr_pending_f;
}

void gm20b_mc_intr_stall_unit_config(struct gk20a *g, u32 unit, bool enable)
{
	u32 unit_pending_f = gm20b_mc_intr_pending_f(g, unit);

	if (enable) {
		nvgpu_writel(g, mc_intr_mask_0_r(),
			nvgpu_readl(g, mc_intr_mask_0_r()) |
			unit_pending_f);
	} else {
		nvgpu_writel(g, mc_intr_mask_0_r(),
			nvgpu_readl(g, mc_intr_mask_0_r()) &
			~unit_pending_f);
	}
}

void gm20b_mc_intr_nonstall_unit_config(struct gk20a *g, u32 unit, bool enable)
{
	u32 unit_pending_f = gm20b_mc_intr_pending_f(g, unit);

	if (enable) {
		nvgpu_writel(g, mc_intr_mask_1_r(),
			nvgpu_readl(g, mc_intr_mask_1_r()) |
			unit_pending_f);
	} else {
		nvgpu_writel(g, mc_intr_mask_1_r(),
			nvgpu_readl(g, mc_intr_mask_1_r()) &
			~unit_pending_f);
	}
}

void gm20b_mc_intr_stall_pause(struct gk20a *g)
{
	nvgpu_writel(g, mc_intr_en_0_r(),
		mc_intr_en_0_inta_disabled_f());

	/* flush previous write */
	(void) nvgpu_readl(g, mc_intr_en_0_r());
}

void gm20b_mc_intr_stall_resume(struct gk20a *g)
{
	nvgpu_writel(g, mc_intr_en_0_r(),
		mc_intr_en_0_inta_hardware_f());

	/* flush previous write */
	(void) nvgpu_readl(g, mc_intr_en_0_r());
}

void gm20b_mc_intr_nonstall_pause(struct gk20a *g)
{
	nvgpu_writel(g, mc_intr_en_1_r(),
		mc_intr_en_0_inta_disabled_f());

	/* flush previous write */
	(void) nvgpu_readl(g, mc_intr_en_1_r());
}

void gm20b_mc_intr_nonstall_resume(struct gk20a *g)
{
	nvgpu_writel(g, mc_intr_en_1_r(),
		mc_intr_en_0_inta_hardware_f());

	/* flush previous write */
	(void) nvgpu_readl(g, mc_intr_en_1_r());
}

u32 gm20b_mc_intr_stall(struct gk20a *g)
{
	return nvgpu_readl(g, mc_intr_r(NVGPU_CIC_INTR_STALLING));
}

u32 gm20b_mc_intr_nonstall(struct gk20a *g)
{
	return nvgpu_readl(g, mc_intr_r(NVGPU_CIC_INTR_NONSTALLING));
}

bool gm20b_mc_is_intr1_pending(struct gk20a *g, u32 unit, u32 mc_intr_1)
{
	u32 mask;
	bool is_pending;

	switch (unit) {
	case NVGPU_UNIT_FIFO:
		mask = mc_intr_pfifo_pending_f();
		break;
	default:
		mask = 0U;
		break;
	}

	if (mask == 0U) {
		nvgpu_err(g, "unknown unit %d", unit);
		is_pending = false;
	} else {
		is_pending = ((mc_intr_1 & mask) != 0U);
	}

	return is_pending;
}

void gm20b_mc_log_pending_intrs(struct gk20a *g)
{
	u32 mc_intr_0;
	u32 mc_intr_1;

	mc_intr_0 = g->ops.mc.intr_stall(g);
	if (mc_intr_0 != 0U) {
		if ((mc_intr_0 & mc_intr_priv_ring_pending_f()) != 0U) {
			/* clear priv ring interrupts */
			g->ops.priv_ring.isr(g);
		}
		mc_intr_0 = g->ops.mc.intr_stall(g);
		if (mc_intr_0 != 0U) {
			nvgpu_info(g, "Pending stall intr0=0x%08x", mc_intr_0);
		}
	}

	mc_intr_1 = g->ops.mc.intr_nonstall(g);
	if (mc_intr_1 != 0U) {
		nvgpu_info(g, "Pending nonstall intr1=0x%08x", mc_intr_1);
	}
}

void gm20b_mc_fb_reset(struct gk20a *g)
{
	u32 val;

	nvgpu_log_info(g, "reset gk20a fb");

	val = nvgpu_readl(g, mc_elpg_enable_r());
	val |= mc_elpg_enable_xbar_enabled_f()
		| mc_elpg_enable_pfb_enabled_f()
		| mc_elpg_enable_hub_enabled_f();
	nvgpu_writel(g, mc_elpg_enable_r(), val);
}

void gm20b_mc_ltc_isr(struct gk20a *g)
{
	u32 mc_intr;
	u32 ltc;

	mc_intr = nvgpu_readl(g, mc_intr_ltc_r());
	nvgpu_log(g, gpu_dbg_intr, "mc_ltc_intr: %08x", mc_intr);
	for (ltc = 0; ltc < nvgpu_ltc_get_ltc_count(g); ltc++) {
		if ((mc_intr & BIT32(ltc)) == 0U) {
			continue;
		}
		g->ops.ltc.intr.isr(g, ltc);
	}
}

bool gm20b_mc_is_mmu_fault_pending(struct gk20a *g)
{
	return g->ops.fifo.is_mmu_fault_pending(g);
}
