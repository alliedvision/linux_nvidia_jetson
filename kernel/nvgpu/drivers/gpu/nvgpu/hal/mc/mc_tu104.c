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
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/mc.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/ltc.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/engines.h>
#include <nvgpu/device.h>
#include <nvgpu/power_features/pg.h>

#include "hal/mc/mc_gp10b.h"
#include "mc_tu104.h"

#include "nvgpu/hw/tu104/hw_mc_tu104.h"
#include "nvgpu/hw/tu104/hw_func_tu104.h"
#include "nvgpu/hw/tu104/hw_ctrl_tu104.h"

/* helper to set leaf_reg_bit in LEAF_EN_SET(leaf_reg_index) register */
void intr_tu104_leaf_en_set(struct gk20a *g, u32 leaf_reg_index,
	u32 leaf_reg_bit)
{
	u32 val;

	val = nvgpu_func_readl(g,
		func_priv_cpu_intr_leaf_en_set_r(leaf_reg_index));

	val |= BIT32(leaf_reg_bit);
	nvgpu_func_writel(g,
		func_priv_cpu_intr_leaf_en_set_r(leaf_reg_index),
		val);
}

/* helper to set leaf_reg_bit in LEAF_EN_CLEAR(leaf_reg_index) register */
void intr_tu104_leaf_en_clear(struct gk20a *g, u32 leaf_reg_index,
	u32 leaf_reg_bit)
{
	u32 val;

	val = nvgpu_func_readl(g,
		func_priv_cpu_intr_leaf_en_clear_r(leaf_reg_index));

	val |= BIT32(leaf_reg_bit);
	nvgpu_func_writel(g,
		func_priv_cpu_intr_leaf_en_clear_r(leaf_reg_index),
		val);
}

/* helper to set leaf_reg_bit in LEAF(leaf_reg_index) register */
static void intr_tu104_leaf_clear(struct gk20a *g, u32 leaf_reg_index,
	u32 leaf_reg_bit)
{
	nvgpu_func_writel(g,
		func_priv_cpu_intr_leaf_r(leaf_reg_index),
		BIT32(leaf_reg_bit));
}

/* helper to set top_reg_bit in TOP_EN_SET(top_reg_index) register */
void intr_tu104_top_en_set(struct gk20a *g, u32 top_reg_index,
	u32 top_reg_bit)
{
	u32 val;

	val = nvgpu_func_readl(g,
		func_priv_cpu_intr_top_en_set_r(top_reg_index));

	val |= BIT32(top_reg_bit);
	nvgpu_func_writel(g,
		func_priv_cpu_intr_top_en_set_r(top_reg_index),
		val);
}

/* helper to enable interrupt vector in both LEAF and TOP registers */
void intr_tu104_vector_en_set(struct gk20a *g, u32 intr_vector)
{
	intr_tu104_leaf_en_set(g,
		NV_CPU_INTR_GPU_VECTOR_TO_LEAF_REG(intr_vector),
		NV_CPU_INTR_GPU_VECTOR_TO_LEAF_BIT(intr_vector));

	intr_tu104_top_en_set(g,
		NV_CPU_INTR_SUBTREE_TO_TOP_IDX(
			NV_CPU_INTR_GPU_VECTOR_TO_SUBTREE(intr_vector)),
		(NV_CPU_INTR_SUBTREE_TO_TOP_BIT(
			NV_CPU_INTR_GPU_VECTOR_TO_SUBTREE(intr_vector))));
}

/* helper to disable interrupt vector in LEAF register */
void intr_tu104_vector_en_clear(struct gk20a *g, u32 intr_vector)
{
	intr_tu104_leaf_en_clear(g,
		NV_CPU_INTR_GPU_VECTOR_TO_LEAF_REG(intr_vector),
		NV_CPU_INTR_GPU_VECTOR_TO_LEAF_BIT(intr_vector));
}

/* helper to clear an interrupt vector in LEAF register */
void intr_tu104_intr_clear_leaf_vector(struct gk20a *g, u32 intr_vector)
{
	intr_tu104_leaf_clear(g,
		NV_CPU_INTR_GPU_VECTOR_TO_LEAF_REG(intr_vector),
		NV_CPU_INTR_GPU_VECTOR_TO_LEAF_BIT(intr_vector));
}

/* helper to check if interrupt is pending for interrupt vector */
bool intr_tu104_vector_intr_pending(struct gk20a *g, u32 intr_vector)
{
	u32 leaf_val;

	leaf_val = nvgpu_func_readl(g,
		func_priv_cpu_intr_leaf_r(
			NV_CPU_INTR_GPU_VECTOR_TO_LEAF_REG(intr_vector)));

	return ((leaf_val &
		BIT32(NV_CPU_INTR_GPU_VECTOR_TO_LEAF_BIT(intr_vector))) != 0U);
}

static void intr_tu104_nonstall_enable(struct gk20a *g)
{
	u32 i;
	u32 nonstall_intr_base = 0;
	u64 nonstall_intr_mask = 0;
	u32 intr_mask;

	/* Keep NV_PMC_INTR(1) disabled */
	nvgpu_writel(g, mc_intr_en_clear_r(NVGPU_CIC_INTR_NONSTALLING), U32_MAX);

	/*
	 * Enable nonstall interrupts in TOP
	 * Enable all engine specific non-stall interrupts in LEAF
	 *
	 * We need to read and add
	 * ctrl_legacy_engine_nonstall_intr_base_vectorid_r()
	 * to get correct interrupt id in NV_CTRL tree
	 */
	nonstall_intr_base = nvgpu_readl(g,
		ctrl_legacy_engine_nonstall_intr_base_vectorid_r());
	/*
	 * FIXME: ctrl_legacy_engine_nonstall_intr_base_vectorid_r() has a range of 0-4095,
	 *        but bit shifting is currently using u64 variables (after typecast).
	 */
	if (nonstall_intr_base > 63U) {
		nvgpu_err(g, "Invalid nostall_intr_base, %u", nonstall_intr_base);
		return;
	}

	for (i = 0; i < g->fifo.num_engines; i++) {
		const struct nvgpu_device *dev = g->fifo.active_engines[i];

		intr_mask = BIT32(dev->intr_id);

		nonstall_intr_mask |= U64(intr_mask) << U64(nonstall_intr_base);
	}

	nvgpu_func_writel(g,
		func_priv_cpu_intr_top_en_set_r(
			NV_CPU_INTR_SUBTREE_TO_TOP_IDX(
				NV_CPU_INTR_TOP_NONSTALL_SUBTREE)),
		BIT32(NV_CPU_INTR_SUBTREE_TO_TOP_BIT(
			NV_CPU_INTR_TOP_NONSTALL_SUBTREE)));

	nvgpu_func_writel(g,
		func_priv_cpu_intr_leaf_en_set_r(
			NV_CPU_INTR_SUBTREE_TO_LEAF_REG0(
				NV_CPU_INTR_TOP_NONSTALL_SUBTREE)),
		u64_lo32(nonstall_intr_mask));
	nvgpu_func_writel(g,
		func_priv_cpu_intr_leaf_en_set_r(
			NV_CPU_INTR_SUBTREE_TO_LEAF_REG1(
				NV_CPU_INTR_TOP_NONSTALL_SUBTREE)),
		u64_hi32(nonstall_intr_mask));
}

static u32 intr_tu104_intr_pending_f(struct gk20a *g, u32 unit)
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
		intr_pending_f = mc_intr_pmu_pending_f();
		break;
	case NVGPU_CIC_INTR_UNIT_CE:
		intr_pending_f = nvgpu_ce_engine_interrupt_mask(g);
		break;
	case NVGPU_CIC_INTR_UNIT_NVLINK:
		intr_pending_f = mc_intr_nvlink_pending_f();
		break;
	case NVGPU_CIC_INTR_UNIT_FBPA:
		intr_pending_f = mc_intr_pfb_pending_f();
		break;
	default:
		nvgpu_err(g, "Invalid MC interrupt unit specified !!!");
		break;
	}

	return intr_pending_f;
}

void intr_tu104_stall_unit_config(struct gk20a *g, u32 unit, bool enable)
{
	u32 unit_pending_f = intr_tu104_intr_pending_f(g, unit);
	u32 reg = 0U;

	if (enable) {
		reg = mc_intr_en_set_r(NVGPU_CIC_INTR_STALLING);
		g->mc.intr_mask_restore[NVGPU_CIC_INTR_STALLING] |=
			unit_pending_f;
		nvgpu_writel(g, reg, unit_pending_f);
	} else {
		reg = mc_intr_en_clear_r(NVGPU_CIC_INTR_STALLING);
		g->mc.intr_mask_restore[NVGPU_CIC_INTR_STALLING] &=
			~unit_pending_f;
		nvgpu_writel(g, reg, unit_pending_f);
	}
}

void intr_tu104_nonstall_unit_config(struct gk20a *g, u32 unit, bool enable)
{
	(void)unit;
	(void)enable;
	intr_tu104_nonstall_enable(g);
}

void intr_tu104_mask(struct gk20a *g)
{
	u32 size, reg, i;

	nvgpu_writel(g, mc_intr_en_clear_r(NVGPU_CIC_INTR_STALLING), U32_MAX);
	g->mc.intr_mask_restore[NVGPU_CIC_INTR_STALLING] = 0;

	nvgpu_writel(g, mc_intr_en_clear_r(NVGPU_CIC_INTR_NONSTALLING), U32_MAX);
	g->mc.intr_mask_restore[NVGPU_CIC_INTR_NONSTALLING] = 0;

	size = func_priv_cpu_intr_top_en_clear__size_1_v();
	for (i = 0U; i < size; i++) {
		reg = func_priv_cpu_intr_top_en_clear_r(i);
		nvgpu_func_writel(g, reg, U32_MAX);
	}
}

/* Return non-zero if nonstall interrupts are pending */
u32 intr_tu104_nonstall(struct gk20a *g)
{
	u32 nonstall_intr_status;
	u32 nonstall_intr_set_mask;

	nonstall_intr_status =
		nvgpu_func_readl(g, func_priv_cpu_intr_top_r(
			NV_CPU_INTR_SUBTREE_TO_TOP_IDX(
				NV_CPU_INTR_TOP_NONSTALL_SUBTREE)));

	nonstall_intr_set_mask = BIT32(
			NV_CPU_INTR_SUBTREE_TO_TOP_BIT(
				NV_CPU_INTR_TOP_NONSTALL_SUBTREE));

	return nonstall_intr_status & nonstall_intr_set_mask;
}

/* pause all nonstall interrupts */
void intr_tu104_nonstall_pause(struct gk20a *g)
{
	nvgpu_func_writel(g,
		func_priv_cpu_intr_top_en_clear_r(
			NV_CPU_INTR_SUBTREE_TO_TOP_IDX(
				NV_CPU_INTR_TOP_NONSTALL_SUBTREE)),
		BIT32(NV_CPU_INTR_SUBTREE_TO_TOP_BIT(
			NV_CPU_INTR_TOP_NONSTALL_SUBTREE)));
}

/* resume all nonstall interrupts */
void intr_tu104_nonstall_resume(struct gk20a *g)
{
	nvgpu_func_writel(g,
		func_priv_cpu_intr_top_en_set_r(
			NV_CPU_INTR_SUBTREE_TO_TOP_IDX(
				NV_CPU_INTR_TOP_NONSTALL_SUBTREE)),
		BIT32(NV_CPU_INTR_SUBTREE_TO_TOP_BIT(
			NV_CPU_INTR_TOP_NONSTALL_SUBTREE)));
}

/* Handle and clear all nonstall interrupts */
u32 intr_tu104_isr_nonstall(struct gk20a *g)
{
	u32 i;
	u32 nonstall_intr_base = 0U;
	u64 nonstall_intr_mask = 0U;
	u32 nonstall_intr_mask_lo, nonstall_intr_mask_hi;
	u32 intr_leaf_reg0, intr_leaf_reg1;
	u32 intr_mask;
	u32 ops = 0U;

	intr_leaf_reg0 = nvgpu_func_readl(g,
			func_priv_cpu_intr_leaf_r(
				NV_CPU_INTR_SUBTREE_TO_LEAF_REG0(
					NV_CPU_INTR_TOP_NONSTALL_SUBTREE)));

	intr_leaf_reg1 = nvgpu_func_readl(g,
			func_priv_cpu_intr_leaf_r(
				NV_CPU_INTR_SUBTREE_TO_LEAF_REG1(
					NV_CPU_INTR_TOP_NONSTALL_SUBTREE)));
	/*
	 * FIXME: ctrl_legacy_engine_nonstall_intr_base_vectorid_r() has a range of 0-4095,
	 *        but bit shifting is currently using u64 variables (after typecast).
	 */
	nonstall_intr_base = nvgpu_readl(g,
		ctrl_legacy_engine_nonstall_intr_base_vectorid_r());

	if (nonstall_intr_base > 63U) {
		nvgpu_err(g, "Invalid nostall_intr_base, %u", nonstall_intr_base);
		return ops;
	}

	for (i = 0U; i < g->fifo.num_engines; i++) {
		const struct nvgpu_device *dev = g->fifo.active_engines[i];

		intr_mask = BIT32(dev->intr_id);

		nonstall_intr_mask = U64(intr_mask) << U64(nonstall_intr_base);
		nonstall_intr_mask_lo = u64_lo32(nonstall_intr_mask);
		nonstall_intr_mask_hi = u64_hi32(nonstall_intr_mask);

		if ((nonstall_intr_mask_lo & intr_leaf_reg0) != 0U ||
		    (nonstall_intr_mask_hi & intr_leaf_reg1) != 0U) {
			nvgpu_log(g, gpu_dbg_intr,
				"nonstall intr from engine %d",
				dev->engine_id);

			nvgpu_func_writel(g,
				func_priv_cpu_intr_leaf_r(
					NV_CPU_INTR_SUBTREE_TO_LEAF_REG0(
					     NV_CPU_INTR_TOP_NONSTALL_SUBTREE)),
				nonstall_intr_mask_lo);

			nvgpu_func_writel(g,
				func_priv_cpu_intr_leaf_r(
					NV_CPU_INTR_SUBTREE_TO_LEAF_REG1(
					     NV_CPU_INTR_TOP_NONSTALL_SUBTREE)),
				nonstall_intr_mask_hi);

			ops |= (NVGPU_CIC_NONSTALL_OPS_WAKEUP_SEMAPHORE |
				NVGPU_CIC_NONSTALL_OPS_POST_EVENTS);
		}
	}

	return ops;
}

/* Return non-zero if stall interrupts are pending */
u32 intr_tu104_stall(struct gk20a *g)
{
	u32 mc_intr_0;

	mc_intr_0 = mc_gp10b_intr_stall(g);
	if (mc_intr_0 != 0U) {
		return mc_intr_0;
	}

	if (g->ops.mc.is_intr_hub_pending != NULL) {
		if (g->ops.mc.is_intr_hub_pending(g, 0) == false) {
			return 0U;
		} else {
			return 1U;
		}
	}

	return 0U;
}

/* Return true if HUB interrupt is pending */
bool intr_tu104_is_intr_hub_pending(struct gk20a *g, u32 mc_intr_0)
{
	(void)mc_intr_0;
	return g->ops.mc.is_mmu_fault_pending(g);
}

/* pause all stall interrupts */
void intr_tu104_stall_pause(struct gk20a *g)
{
	mc_gp10b_intr_stall_pause(g);

	g->ops.fb.intr.disable(g);
}

/* resume all stall interrupts */
void intr_tu104_stall_resume(struct gk20a *g)
{
	mc_gp10b_intr_stall_resume(g);

	g->ops.fb.intr.enable(g);
}

void intr_tu104_log_pending_intrs(struct gk20a *g)
{
	bool pending;
	u32 intr, i, size;

	intr = intr_tu104_nonstall(g);
	if (intr != 0U) {
		nvgpu_info(g, "Pending nonstall intr=0x%08x", intr);
	}

	intr = mc_gp10b_intr_stall(g);
	if (intr != 0U) {
		nvgpu_info(g, "Pending stall intr=0x%08x", intr);
	}

	if (g->ops.mc.is_intr_hub_pending != NULL) {
		pending = g->ops.mc.is_intr_hub_pending(g, 0);
		if (pending) {
			nvgpu_info(g, "Pending hub intr");
		}
	}

	size = func_priv_cpu_intr_top__size_1_v();
	for (i = 0U; i < size; i++) {
		intr = nvgpu_func_readl(g,
			func_priv_cpu_intr_top_r(i));
		if (intr == 0U) {
			continue;
		}
		nvgpu_info(g, "Pending TOP%d intr=0x%08x", i, intr);
	}
}

void mc_tu104_fbpa_isr(struct gk20a *g)
{
	u32 intr_fbpa, fbpas;
	u32 i, num_fbpas;

	intr_fbpa = nvgpu_readl(g, mc_intr_fbpa_r());
	fbpas = mc_intr_fbpa_part_mask_v(intr_fbpa);
	num_fbpas = nvgpu_get_litter_value(g, GPU_LIT_NUM_FBPAS);

	for (i = 0U; i < num_fbpas; i++) {
		if ((fbpas & BIT32(i)) == 0U) {
			continue;
		}
		g->ops.fb.handle_fbpa_intr(g, i);
	}
}


void mc_tu104_ltc_isr(struct gk20a *g)
{
	u32 ltc;

	/* Go through all the LTCs explicitly */
	for (ltc = 0; ltc < nvgpu_ltc_get_ltc_count(g); ltc++) {
		g->ops.ltc.intr.isr(g, ltc);
	}
}

static void mc_tu104_isr_stall_primary(struct gk20a *g, u32 mc_intr_0)
{
	/*
	 * In Turing, mc_intr_1 is deprecated and pbus intr is routed to
	 * mc_intr_0. This is different than legacy chips pbus interrupt.
	 */
	if ((mc_intr_0 & mc_intr_pbus_pending_f()) != 0U) {
		g->ops.bus.isr(g);
	}

	if ((mc_intr_0 & mc_intr_priv_ring_pending_f()) != 0U) {
		g->ops.priv_ring.isr(g);
	}
}

void mc_tu104_isr_stall(struct gk20a *g)
{
	u32 mc_intr_0;
	u32 i;
	const struct nvgpu_device *dev;

	mc_intr_0 = nvgpu_readl(g, mc_intr_r(NVGPU_CIC_INTR_STALLING));

	nvgpu_log(g, gpu_dbg_intr, "stall intr 0x%08x", mc_intr_0);

	mc_tu104_isr_stall_primary(g, mc_intr_0);

	for (i = 0U; i < g->fifo.num_engines; i++) {
		dev = g->fifo.active_engines[i];

		if ((mc_intr_0 & BIT32(dev->intr_id)) == 0U) {
			continue;
		}

		mc_gp10b_isr_stall_engine(g, dev);
	}

	mc_gp10b_isr_stall_secondary_0(g, mc_intr_0);
	mc_gp10b_isr_stall_secondary_1(g, mc_intr_0);
	nvgpu_log(g, gpu_dbg_intr, "stall intr done 0x%08x", mc_intr_0);

}
