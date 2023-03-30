/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/gk20a.h>
#include <nvgpu/engines.h>
#include <nvgpu/device.h>
#include <nvgpu/mc.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/power_features/pg.h>
#include <nvgpu/gr/gr_instances.h>
#include <nvgpu/ce.h>
#ifdef CONFIG_NVGPU_GSP_SCHEDULER
#include <nvgpu/gsp.h>
#endif

#include "mc_intr_ga10b.h"

#include <nvgpu/hw/ga10b/hw_func_ga10b.h>
#include <nvgpu/hw/ga10b/hw_ctrl_ga10b.h>

static bool ga10b_intr_is_unit_pending(struct gk20a *g,
	u32 intr_unit, u32 intr_leaf0, u32 intr_leaf1, u64 *unit_subtree_mask)
{
	u64 subtree_mask;
	u32 subtree_mask_lo, subtree_mask_hi;
	u32 intr_unit_pending = false;
	struct nvgpu_intr_unit_info *intr_unit_info;

	intr_unit_info = g->mc.intr_unit_info;

	subtree_mask = intr_unit_info[intr_unit].subtree_mask;
	subtree_mask_lo = u64_lo32(subtree_mask);
	subtree_mask_hi = u64_hi32(subtree_mask);

	if ((subtree_mask_lo & intr_leaf0) != 0U ||
	    (subtree_mask_hi & intr_leaf1) != 0U) {
		intr_unit_pending = true;
		*unit_subtree_mask = subtree_mask;
		nvgpu_log(g, gpu_dbg_intr, "pending intr_unit: %u", intr_unit);
	}

	return intr_unit_pending;
}

static void ga10b_intr_subtree_leaf0_leaf1_status(struct gk20a *g, u32 subtree,
		u32 *intr_leaf0, u32 *intr_leaf1)
{
	/**
	 * Get intr_leaf status for the subtree
	 * top bit 0 -> subtree 0 -> leaf0, leaf1 -> leaf 0, 1
	 * top bit 1 -> subtree 1 -> leaf0, leaf1 -> leaf 2, 3
	 * top bit 2 -> subtree 2 -> leaf0, leaf1 -> leaf 4, 5
	 * top bit 3 -> subtree 3 -> leaf0, leaf1 -> leaf 6, 7
	 */

	*intr_leaf0 = nvgpu_func_readl(g,
			func_priv_cpu_intr_leaf_r(
				HOST2SOC_SUBTREE_TO_LEAF0(subtree)));

	*intr_leaf1 = nvgpu_func_readl(g,
			func_priv_cpu_intr_leaf_r(
				HOST2SOC_SUBTREE_TO_LEAF1(subtree)));

	nvgpu_log(g, gpu_dbg_intr,
		"%d_subtree: intr_leaf0: 0x%08x intr_leaf1: 0x%08x",
			subtree, *intr_leaf0, *intr_leaf1);
}

static void ga10b_intr_subtree_clear(struct gk20a *g, u32 subtree,
		u64 subtree_mask)
{
	/**
	 * Clear interrupts in Leaf registers for the subtree.
	 * top bit 0 -> subtree 0 -> leaf0, leaf1 -> leaf 0, 1
	 * top bit 1 -> subtree 1 -> leaf0, leaf1 -> leaf 2, 3
	 * top bit 2 -> subtree 2 -> leaf0, leaf1 -> leaf 4, 5
	 * top bit 3 -> subtree 3 -> leaf0, leaf1 -> leaf 6, 7
	 */

	nvgpu_func_writel(g, func_priv_cpu_intr_leaf_r(
				HOST2SOC_SUBTREE_TO_LEAF0(subtree)),
				u64_lo32(subtree_mask));

	nvgpu_func_writel(g, func_priv_cpu_intr_leaf_r(
				HOST2SOC_SUBTREE_TO_LEAF1(subtree)),
				u64_hi32(subtree_mask));

	nvgpu_log(g, gpu_dbg_intr, "clear %d_subtree_mask: 0x%llx",
			subtree, subtree_mask);
}

static void ga10b_intr_unit_enable(struct gk20a *g, u32 subtree,
		u64 subtree_mask)
{
	/**
	 * Enable interrupts in Top & Leaf registers for the subtree.
	 * top bit 0 -> subtree 0 -> leaf0, leaf1 -> leaf 0, 1
	 * top bit 1 -> subtree 1 -> leaf0, leaf1 -> leaf 2, 3
	 * top bit 2 -> subtree 2 -> leaf0, leaf1 -> leaf 4, 5
	 * top bit 3 -> subtree 3 -> leaf0, leaf1 -> leaf 6, 7
	 */
	//TODO top_en manipulation needs to be decoupled from leaf_en enablement
	//process.
	nvgpu_func_writel(g,
		func_priv_cpu_intr_top_en_set_r(
			HOST2SOC_SUBTREE_TO_TOP_IDX(subtree)),
			BIT32(HOST2SOC_SUBTREE_TO_TOP_BIT(subtree)));
	nvgpu_func_writel(g,
		func_priv_cpu_intr_leaf_en_set_r(
			HOST2SOC_SUBTREE_TO_LEAF0(subtree)),
		u64_lo32(subtree_mask));
	nvgpu_func_writel(g,
		func_priv_cpu_intr_leaf_en_set_r(
			HOST2SOC_SUBTREE_TO_LEAF1(subtree)),
		u64_hi32(subtree_mask));

	nvgpu_log(g, gpu_dbg_intr, "%d_subtree_mask: 0x%llx",
			subtree, subtree_mask);
}

static void ga10b_intr_unit_disable(struct gk20a *g, u32 subtree,
		u64 subtree_mask)
{
	/**
	 * Disable unit specific Leaf interrupt registers for the subtree.
	 * top bit 0 -> subtree 0 -> leaf0, leaf1 -> leaf 0, 1
	 * top bit 1 -> subtree 1 -> leaf0, leaf1 -> leaf 2, 3
	 * top bit 2 -> subtree 2 -> leaf0, leaf1 -> leaf 4, 5
	 * top bit 3 -> subtree 3 -> leaf0, leaf1 -> leaf 6, 7
	 */
	nvgpu_func_writel(g,
		func_priv_cpu_intr_leaf_en_clear_r(
			HOST2SOC_SUBTREE_TO_LEAF0(subtree)),
		u64_lo32(subtree_mask));
	nvgpu_func_writel(g,
		func_priv_cpu_intr_leaf_en_clear_r(
			HOST2SOC_SUBTREE_TO_LEAF1(subtree)),
		u64_hi32(subtree_mask));

	nvgpu_log(g, gpu_dbg_intr, "%d_subtree_mask: 0x%llx",
			subtree, subtree_mask);
}

static void ga10b_intr_config(struct gk20a *g, bool enable, u32 subtree,
		u64 subtree_mask)
{
	if (enable) {
		g->mc.subtree_mask_restore[subtree] |=
			subtree_mask;
		ga10b_intr_unit_enable(g, subtree, subtree_mask);
	} else {
		g->mc.subtree_mask_restore[subtree] &=
			~(subtree_mask);
		ga10b_intr_unit_disable(g, subtree, subtree_mask);
	}
}

static void ga10b_intr_subtree_clear_unhandled(struct gk20a *g,
	u32 subtree, u32 intr_leaf0, u32 intr_leaf1, u64 handled_subtree_mask)
{
	if (((u64_lo32(handled_subtree_mask)) != intr_leaf0) &&
	    ((u64_hi32(handled_subtree_mask)) != intr_leaf1)) {
		u64 unhandled_intr_leaf0 = intr_leaf0 &
			~(u64_lo32(handled_subtree_mask));
		u64 unhandled_intr_leaf1 = intr_leaf1 &
			~(u64_hi32(handled_subtree_mask));
		nvgpu_err(g, "unhandled host2soc_%d intr handled: 0x%llx"
				"intr_leaf0 0x%08x intr_leaf1 0x%08x",
			subtree, handled_subtree_mask, intr_leaf0, intr_leaf1);
		ga10b_intr_subtree_clear(g, subtree,
			hi32_lo32_to_u64((u32)unhandled_intr_leaf1,
				(u32)unhandled_intr_leaf0));
	}
}

void ga10b_intr_host2soc_0_unit_config(struct gk20a *g, u32 unit, bool enable)
{
	u32 subtree = 0U;
	u64 subtree_mask = 0ULL;

	if (nvgpu_cic_mon_intr_get_unit_info(g, unit, &subtree, &subtree_mask)
			== false) {
		return;
	}
	ga10b_intr_config(g, enable, HOST2SOC_0_SUBTREE, subtree_mask);
}

/** return non-zero if 0_subtree interrupts are pending */
u32 ga10b_intr_host2soc_0(struct gk20a *g)
{
	u32 intr_status;
	u32 intr_mask;

	intr_status = nvgpu_func_readl(g, func_priv_cpu_intr_top_r(
			HOST2SOC_SUBTREE_TO_TOP_IDX(HOST2SOC_0_SUBTREE)));

	nvgpu_log(g, gpu_dbg_intr, "0_subtree intr top status: 0x%08x",
				intr_status);

	intr_mask = BIT32(HOST2SOC_SUBTREE_TO_TOP_BIT(HOST2SOC_0_SUBTREE));

	return intr_status & intr_mask;
}

/** pause all 0_subtree interrupts */
void ga10b_intr_host2soc_0_pause(struct gk20a *g)
{
	nvgpu_func_writel(g,
		func_priv_cpu_intr_top_en_clear_r(
			HOST2SOC_SUBTREE_TO_TOP_IDX(
				HOST2SOC_0_SUBTREE)),
		BIT32(HOST2SOC_SUBTREE_TO_TOP_BIT(
			HOST2SOC_0_SUBTREE)));
}

/* resume all 0_subtree interrupts */
void ga10b_intr_host2soc_0_resume(struct gk20a *g)
{
	nvgpu_func_writel(g,
		func_priv_cpu_intr_top_en_set_r(
			HOST2SOC_SUBTREE_TO_TOP_IDX(
				HOST2SOC_0_SUBTREE)),
		BIT32(HOST2SOC_SUBTREE_TO_TOP_BIT(
			HOST2SOC_0_SUBTREE)));
}

/* Handle and clear 0_subtree interrupts */
u32 ga10b_intr_isr_host2soc_0(struct gk20a *g)
{
	u32 intr_leaf0, intr_leaf1;
	u64 unit_subtree_mask;
	u64 handled_subtree_mask = 0ULL;
	u32 subtree;
	u32 ops = 0U;

	/**
	 * Engine Non-stall interrupts
	 * Leaf 0 is for engine non stall interrupts used to notify.
	 * Leaf 5 is for future use.
	 */

	subtree = HOST2SOC_0_SUBTREE;
	ga10b_intr_subtree_leaf0_leaf1_status(g, subtree,
		&intr_leaf0, &intr_leaf1);

	if (ga10b_intr_is_unit_pending(g, NVGPU_CIC_INTR_UNIT_GR,
			intr_leaf0, intr_leaf1, &unit_subtree_mask) == true) {
		ga10b_intr_subtree_clear(g, subtree, unit_subtree_mask);
		ops |= (NVGPU_CIC_NONSTALL_OPS_WAKEUP_SEMAPHORE |
				NVGPU_CIC_NONSTALL_OPS_POST_EVENTS);
		handled_subtree_mask |= unit_subtree_mask;
	}

#ifdef CONFIG_NVGPU_NONSTALL_INTR
	if (ga10b_intr_is_unit_pending(g, NVGPU_CIC_INTR_UNIT_CE,
			intr_leaf0, intr_leaf1, &unit_subtree_mask) == true) {
		ga10b_intr_subtree_clear(g, subtree, unit_subtree_mask);
		ops |= (NVGPU_CIC_NONSTALL_OPS_WAKEUP_SEMAPHORE |
				NVGPU_CIC_NONSTALL_OPS_POST_EVENTS);
		handled_subtree_mask |= unit_subtree_mask;
	}
#endif
	ga10b_intr_subtree_clear_unhandled(g, subtree, intr_leaf0, intr_leaf1,
				handled_subtree_mask);
	return ops;
}

#ifdef CONFIG_NVGPU_NON_FUSA
void ga10b_intr_log_pending_intrs(struct gk20a *g)
{
	u32 intr_top, intr_leaf, i, j;

	for (i = 0U; i < func_priv_cpu_intr_top__size_1_v(); i++) {
		intr_top = nvgpu_func_readl(g,
			func_priv_cpu_intr_top_r(i));

		/* Each top reg contains intr status for leaf__size */
		for (j = 0U; j < func_priv_cpu_intr_leaf__size_1_v(); j++) {
			intr_leaf = nvgpu_func_readl(g,
					func_priv_cpu_intr_leaf_r(j));
			if (intr_leaf == 0U) {
				continue;
			}
			nvgpu_err(g,
				"Pending TOP[%d]: 0x%08x, LEAF[%d]: 0x%08x",
				i, intr_top, j, intr_leaf);
		}
	}
}
#endif

void ga10b_intr_mask_top(struct gk20a *g)
{
	u32 i;

	/* mask interrupts at the top level. leafs are not touched */
	for (i = 0U; i < func_priv_cpu_intr_top_en_clear__size_1_v(); i++) {
		nvgpu_func_writel(g, func_priv_cpu_intr_top_en_clear_r(i),
				U32_MAX);
	}
}

bool ga10b_mc_intr_get_unit_info(struct gk20a *g, u32 unit)
{
	u32 vectorid, reg_val, i;
	struct nvgpu_intr_unit_info *intr_unit_info;
	u64 tmp_subtree_mask = 0ULL;

	intr_unit_info = &g->mc.intr_unit_info[unit];

	switch (unit) {
	case NVGPU_CIC_INTR_UNIT_BUS:
		intr_unit_info->vectorid[0] =
			func_priv_cpu_intr_pbus_vector_v();
		intr_unit_info->vectorid_size = NVGPU_CIC_INTR_VECTORID_SIZE_ONE;
		break;
	case NVGPU_CIC_INTR_UNIT_PRIV_RING:
		intr_unit_info->vectorid[0] =
			func_priv_cpu_intr_priv_ring_vector_v();
		intr_unit_info->vectorid_size = NVGPU_CIC_INTR_VECTORID_SIZE_ONE;
		break;
	case NVGPU_CIC_INTR_UNIT_LTC:
		intr_unit_info->vectorid[0] =
			func_priv_cpu_intr_ltc_all_vector_v();
		intr_unit_info->vectorid_size = NVGPU_CIC_INTR_VECTORID_SIZE_ONE;
		break;
	case NVGPU_CIC_INTR_UNIT_PMU:
		intr_unit_info->vectorid[0] =
			func_priv_cpu_intr_pmu_vector_v();
		intr_unit_info->vectorid_size = NVGPU_CIC_INTR_VECTORID_SIZE_ONE;
		break;
	case NVGPU_CIC_INTR_UNIT_FBPA:
		intr_unit_info->vectorid[0] =
			func_priv_cpu_intr_pfb_vector_v();
		intr_unit_info->vectorid_size = NVGPU_CIC_INTR_VECTORID_SIZE_ONE;
		break;
	case NVGPU_CIC_INTR_UNIT_MMU_FAULT_ECC_ERROR:
	case NVGPU_CIC_INTR_UNIT_MMU_NON_REPLAYABLE_FAULT:
	case NVGPU_CIC_INTR_UNIT_MMU_NON_REPLAYABLE_FAULT_ERROR:
	case NVGPU_CIC_INTR_UNIT_MMU_INFO_FAULT:
	case NVGPU_CIC_INTR_UNIT_MMU_REPLAYABLE_FAULT:
	case NVGPU_CIC_INTR_UNIT_MMU_REPLAYABLE_FAULT_ERROR:
		/* vectorids are set up in fb.init_hw */
		nvgpu_log(g, gpu_dbg_intr, "MMU interrupts: %d", unit);
		break;
	/* GR NONSTALL interrupts */
	case NVGPU_CIC_INTR_UNIT_GR:
		/*
		 * Even though each engine has separate vector id and each intr_unit
		 * supports multiple engines, vectorid_size is set to one. This
		 * is because engine interrupt mask is being used to configure
		 * interrupts. Base vector is read from ctrl reg.
		 */
		reg_val = nvgpu_readl(g,
			    ctrl_legacy_engine_nonstall_intr_base_vectorid_r());
		vectorid =
		  ctrl_legacy_engine_nonstall_intr_base_vectorid_vector_v(reg_val);
		intr_unit_info->vectorid[0] = vectorid;
		intr_unit_info->vectorid_size = NVGPU_CIC_INTR_VECTORID_SIZE_ONE;

		intr_unit_info->subtree = GPU_VECTOR_TO_SUBTREE(vectorid);

		tmp_subtree_mask = ((u64)nvgpu_gr_engine_interrupt_mask(g));
		tmp_subtree_mask <<= GPU_VECTOR_TO_LEAF_SHIFT(vectorid);
		intr_unit_info->subtree_mask = tmp_subtree_mask;

		nvgpu_log(g, gpu_dbg_intr, "GR NONSTALL %d_subtree_mask: 0x%llx",
			intr_unit_info->subtree, intr_unit_info->subtree_mask);
		intr_unit_info->valid = true;
		return true;
	/* CE NONSTALL interrupts */
#ifdef CONFIG_NVGPU_NONSTALL_INTR
	case NVGPU_CIC_INTR_UNIT_CE:
		/* vectorids are setup in ce.init_hw */
		nvgpu_log(g, gpu_dbg_intr, "CE NONSTALL interrupt");
		break;
#endif
	case NVGPU_CIC_INTR_UNIT_GR_STALL:
		reg_val = nvgpu_readl(g,
			    ctrl_legacy_engine_stall_intr_base_vectorid_r());
		vectorid =
		  ctrl_legacy_engine_stall_intr_base_vectorid_vector_v(reg_val);
		intr_unit_info->vectorid[0] = vectorid;
		intr_unit_info->vectorid_size = NVGPU_CIC_INTR_VECTORID_SIZE_ONE;

		intr_unit_info->subtree = GPU_VECTOR_TO_SUBTREE(vectorid);

		tmp_subtree_mask = ((u64)nvgpu_gr_engine_interrupt_mask(g));
		tmp_subtree_mask <<= GPU_VECTOR_TO_LEAF_SHIFT(vectorid);
		intr_unit_info->subtree_mask = tmp_subtree_mask;

		nvgpu_log(g, gpu_dbg_intr, "GR STALL %d_subtree_mask: 0x%llx",
			intr_unit_info->subtree, intr_unit_info->subtree_mask);
		intr_unit_info->valid = true;
		return true;

	case NVGPU_CIC_INTR_UNIT_CE_STALL:
		reg_val = nvgpu_readl(g,
			    ctrl_legacy_engine_stall_intr_base_vectorid_r());
		vectorid =
		  ctrl_legacy_engine_stall_intr_base_vectorid_vector_v(reg_val);
		intr_unit_info->vectorid[0] = vectorid;
		intr_unit_info->vectorid_size = NVGPU_CIC_INTR_VECTORID_SIZE_ONE;

		intr_unit_info->subtree = GPU_VECTOR_TO_SUBTREE(vectorid);

		tmp_subtree_mask = ((u64)nvgpu_ce_engine_interrupt_mask(g));
		tmp_subtree_mask <<= GPU_VECTOR_TO_LEAF_SHIFT(vectorid);
		intr_unit_info->subtree_mask = tmp_subtree_mask;

		nvgpu_log(g, gpu_dbg_intr, "CE STALL %d_subtree_mask: 0x%llx",
			intr_unit_info->subtree, intr_unit_info->subtree_mask);
		intr_unit_info->valid = true;
		return true;

	case NVGPU_CIC_INTR_UNIT_RUNLIST_TREE_0:
	case NVGPU_CIC_INTR_UNIT_RUNLIST_TREE_1:
		nvgpu_log(g, gpu_dbg_intr, "RUNLIST interrupts");
		break;

#ifdef CONFIG_NVGPU_GSP_SCHEDULER
	case NVGPU_CIC_INTR_UNIT_GSP:
		intr_unit_info->vectorid[0] =
			func_priv_cpu_intr_gsp_vector_v();
		intr_unit_info->vectorid_size = NVGPU_CIC_INTR_VECTORID_SIZE_ONE;
		break;
#endif /* CONFIG_NVGPU_GSP_SCHEDULER */

	default:
		nvgpu_err(g, "non supported intr unit");
		return false;
	}

	for (i = 0U; i < intr_unit_info->vectorid_size; i++) {
		u32 tmp_subtree;
		vectorid = intr_unit_info->vectorid[i];
		nvgpu_log(g, gpu_dbg_intr, "unit: %d vectorid: %d",
				unit, vectorid);

		/**
		 * Assumption is intr unit supporting more vectorids
		 * reside in same subtree.
		 */
		tmp_subtree = GPU_VECTOR_TO_SUBTREE(vectorid);
		if (i != 0U && tmp_subtree != intr_unit_info->subtree) {
			nvgpu_err(g,
				"unit: %d, vectorid(%d) is outside subtree(%d)",
				unit, vectorid, intr_unit_info->subtree);
			return false;
		}
		intr_unit_info->subtree = tmp_subtree;

		tmp_subtree_mask = GPU_VECTOR_TO_LEAF_MASK(vectorid);
		tmp_subtree_mask <<= GPU_VECTOR_TO_LEAF_SHIFT(vectorid);
		intr_unit_info->subtree_mask |= tmp_subtree_mask;
	}

	intr_unit_info->valid = true;

	nvgpu_log(g, gpu_dbg_intr, "%d_subtree_mask: 0x%llx",
			intr_unit_info->subtree, intr_unit_info->subtree_mask);
	return true;
}

static u32 ga10b_intr_map_mc_stall_unit_to_intr_unit(struct gk20a *g,
		u32 mc_intr_unit)
{
	u32 intr_unit = mc_intr_unit;

	(void)g;

	/**
	 * Different indices are used to store unit info for
	 * gr/ce stall/nostall intr.
	 */
	if (mc_intr_unit == NVGPU_CIC_INTR_UNIT_GR) {
		intr_unit = NVGPU_CIC_INTR_UNIT_GR_STALL;
	} else if (mc_intr_unit == NVGPU_CIC_INTR_UNIT_CE) {
		intr_unit = NVGPU_CIC_INTR_UNIT_CE_STALL;
	}
	return intr_unit;
}

void ga10b_intr_stall_unit_config(struct gk20a *g, u32 unit, bool enable)
{
	u64 subtree_mask = 0ULL;
	u32 subtree = 0U;

	unit = ga10b_intr_map_mc_stall_unit_to_intr_unit(g, unit);

	if (nvgpu_cic_mon_intr_get_unit_info(g, unit, &subtree, &subtree_mask)
			== false) {
		return;
	}

	ga10b_intr_config(g, enable, subtree, subtree_mask);
}

/** return non-zero if subtree 1, 2, 3 interrupts are pending */
u32 ga10b_intr_stall(struct gk20a *g)
{
	u32 intr_status;
	u32 intr_mask;

	intr_status = nvgpu_func_readl(g, func_priv_cpu_intr_top_r(
			STALL_SUBTREE_TOP_IDX));

	nvgpu_log(g, gpu_dbg_intr, "intr top status: 0x%08x", intr_status);

	intr_mask = STALL_SUBTREE_TOP_BITS;

	return intr_status & intr_mask;
}

/** pause all stall interrupts i.e. from subtree 1, 2 and 3 */
void ga10b_intr_stall_pause(struct gk20a *g)
{
	nvgpu_func_writel(g, func_priv_cpu_intr_top_en_clear_r(
			STALL_SUBTREE_TOP_IDX), STALL_SUBTREE_TOP_BITS);
}

/* resume all interrupts for subtree 1, 2 and 3 */
void ga10b_intr_stall_resume(struct gk20a *g)
{
	nvgpu_func_writel(g, func_priv_cpu_intr_top_en_set_r(
			STALL_SUBTREE_TOP_IDX), STALL_SUBTREE_TOP_BITS);
}

static u32 ga10b_intr_is_pending_2_subtree_mmu_fault(struct gk20a *g,
		u32 intr_leaf0, u32 intr_leaf1, u64 *unit_subtree_mask)
{
	u32 intr_unit_bitmask = 0U;

	if (ga10b_intr_is_unit_pending(g,
		NVGPU_CIC_INTR_UNIT_MMU_FAULT_ECC_ERROR,
		intr_leaf0, intr_leaf1, unit_subtree_mask) == true) {
		intr_unit_bitmask |=
			BIT32(NVGPU_CIC_INTR_UNIT_MMU_FAULT_ECC_ERROR);
	}

	if (ga10b_intr_is_unit_pending(g,
		NVGPU_CIC_INTR_UNIT_MMU_REPLAYABLE_FAULT_ERROR,
		intr_leaf0, intr_leaf1, unit_subtree_mask) == true) {
		intr_unit_bitmask |=
			BIT32(NVGPU_CIC_INTR_UNIT_MMU_REPLAYABLE_FAULT_ERROR);
	}

	if (ga10b_intr_is_unit_pending(g,
		NVGPU_CIC_INTR_UNIT_MMU_NON_REPLAYABLE_FAULT,
		intr_leaf0, intr_leaf1, unit_subtree_mask) == true) {
		intr_unit_bitmask |=
			BIT32(NVGPU_CIC_INTR_UNIT_MMU_NON_REPLAYABLE_FAULT);
	}

	if (ga10b_intr_is_unit_pending(g,
		NVGPU_CIC_INTR_UNIT_MMU_NON_REPLAYABLE_FAULT_ERROR,
		intr_leaf0, intr_leaf1, unit_subtree_mask) == true) {
		intr_unit_bitmask |=
			BIT32(NVGPU_CIC_INTR_UNIT_MMU_NON_REPLAYABLE_FAULT_ERROR);
	}

	if (ga10b_intr_is_unit_pending(g,
		NVGPU_CIC_INTR_UNIT_MMU_INFO_FAULT,
		intr_leaf0, intr_leaf1, unit_subtree_mask) == true) {
		intr_unit_bitmask |=
			BIT32(NVGPU_CIC_INTR_UNIT_MMU_INFO_FAULT);
	}

	if (intr_unit_bitmask != 0U) {
		nvgpu_log(g, gpu_dbg_intr, "mmu_fault_pending: 0x%llx"
			, *unit_subtree_mask);
	}

	return intr_unit_bitmask;
}

static void ga10b_intr_isr_stall_host2soc_1(struct gk20a *g)
{
	u32 intr_leaf0, intr_leaf1;
	u64 unit_subtree_mask;
	u64 handled_subtree_mask = 0ULL;
	u32 subtree;

	/**
	 * New interrupt line
	 * HOST2SOC_1_INTR_ID: 68: 1_subtree: leaf0, leaf1 (leaf 2, 3)
	 * Leaf 2 is for mmu_replayable fault and hub_access_cntr
	 * Leaf 3 is empty
	 */

	subtree = HOST2SOC_1_SUBTREE;
	ga10b_intr_subtree_leaf0_leaf1_status(g, subtree,
		&intr_leaf0, &intr_leaf1);

	if (ga10b_intr_is_unit_pending(g, NVGPU_CIC_INTR_UNIT_MMU_REPLAYABLE_FAULT,
			intr_leaf0, intr_leaf1,
			&unit_subtree_mask) == true) {
		ga10b_intr_subtree_clear(g, subtree, unit_subtree_mask);
		handled_subtree_mask |= unit_subtree_mask;
		g->ops.fb.intr.isr(g, BIT32(NVGPU_CIC_INTR_UNIT_MMU_REPLAYABLE_FAULT));
	}

	ga10b_intr_subtree_clear_unhandled(g, subtree, intr_leaf0, intr_leaf1,
				handled_subtree_mask);
}

static void ga10b_intr_isr_stall_host2soc_2(struct gk20a *g)
{
	u32 intr_leaf0, intr_leaf1;
	u64 unit_subtree_mask;
	u64 handled_subtree_mask = 0ULL;
	u32 subtree;
	u32 intr_unit_bitmask = 0U;

	/**
	 * Legacy stall
	 * HOST2SOC_2_INTR_ID: 70: 2_subtree: leaf0, leaf1 (leaf 4, 5)
	 * Leaf 4 is for mmu_*, pbus, priv, ltc etc.
	 * Leaf 5 is for runlist_tree0
	 */

	subtree = HOST2SOC_2_SUBTREE;
	ga10b_intr_subtree_leaf0_leaf1_status(g, subtree,
		&intr_leaf0, &intr_leaf1);

	if (ga10b_intr_is_unit_pending(g, NVGPU_CIC_INTR_UNIT_BUS, intr_leaf0, intr_leaf1,
				&unit_subtree_mask) == true) {
		handled_subtree_mask |= unit_subtree_mask;
		ga10b_intr_subtree_clear(g, subtree, unit_subtree_mask);
		g->ops.bus.isr(g);
	}

	if (ga10b_intr_is_unit_pending(g, NVGPU_CIC_INTR_UNIT_PRIV_RING,
		intr_leaf0, intr_leaf1, &unit_subtree_mask) == true) {
		handled_subtree_mask |= unit_subtree_mask;
		ga10b_intr_subtree_clear(g, subtree, unit_subtree_mask);
		g->ops.priv_ring.isr(g);
	}

	if (ga10b_intr_is_unit_pending(g, NVGPU_CIC_INTR_UNIT_FBPA,
		intr_leaf0, intr_leaf1, &unit_subtree_mask) == true) {
		handled_subtree_mask |= unit_subtree_mask;
		ga10b_intr_subtree_clear(g, subtree, unit_subtree_mask);
		g->ops.mc.fbpa_isr(g);
	}

	if (ga10b_intr_is_unit_pending(g, NVGPU_CIC_INTR_UNIT_LTC, intr_leaf0, intr_leaf1,
				&unit_subtree_mask) == true) {
		handled_subtree_mask |= unit_subtree_mask;
		ga10b_intr_subtree_clear(g, subtree, unit_subtree_mask);
		g->ops.mc.ltc_isr(g);
	}

	intr_unit_bitmask = ga10b_intr_is_pending_2_subtree_mmu_fault(g,
		intr_leaf0, intr_leaf1, &unit_subtree_mask);
	if (intr_unit_bitmask != 0U) {
		handled_subtree_mask |= unit_subtree_mask;
		ga10b_intr_subtree_clear(g, subtree, unit_subtree_mask);
		g->ops.fb.intr.isr(g, intr_unit_bitmask);
	}

	if (ga10b_intr_is_unit_pending(g, NVGPU_CIC_INTR_UNIT_RUNLIST_TREE_0,
				intr_leaf0, intr_leaf1,
				&unit_subtree_mask) == true) {
		handled_subtree_mask |= unit_subtree_mask;
		ga10b_intr_subtree_clear(g, subtree, unit_subtree_mask);
		g->ops.fifo.intr_0_isr(g);
		g->ops.fifo.runlist_intr_retrigger(g, RUNLIST_INTR_TREE_0);
	}

	if (ga10b_intr_is_unit_pending(g, NVGPU_CIC_INTR_UNIT_PMU, intr_leaf0, intr_leaf1,
				&unit_subtree_mask) == true) {
		handled_subtree_mask |= unit_subtree_mask;
		ga10b_intr_subtree_clear(g, subtree, unit_subtree_mask);
		g->ops.pmu.pmu_isr(g);
	}

#ifdef CONFIG_NVGPU_GSP_SCHEDULER
	if (ga10b_intr_is_unit_pending(g, NVGPU_CIC_INTR_UNIT_GSP, intr_leaf0, intr_leaf1,
				&unit_subtree_mask) == true) {
		handled_subtree_mask |= unit_subtree_mask;
		ga10b_intr_subtree_clear(g, subtree, unit_subtree_mask);
		nvgpu_gsp_isr(g);
	}
#endif /* CONFIG_NVGPU_GSP_SCHEDULER */

	ga10b_intr_subtree_clear_unhandled(g, subtree, intr_leaf0, intr_leaf1,
				handled_subtree_mask);
}

static int ga10b_intr_gr_stall_isr(struct gk20a *g)
{
	int err = 0;

#ifdef CONFIG_NVGPU_POWER_PG
	/* Disable ELPG before handling stall isr */
	err = nvgpu_pg_elpg_disable(g);
	if (err != 0) {
		nvgpu_err(g, "ELPG disable failed."
			"Going ahead with stall_isr handling");
	}
#endif
	/* handle stall isr */
	err = g->ops.gr.intr.stall_isr(g);
	if (err != 0) {
		nvgpu_err(g, "GR intr stall_isr failed");
		return err;
	}

	err = g->ops.gr.intr.retrigger(g);
	if (err != 0) {
		nvgpu_err(g, "GR intr retrigger failed");
		return err;
	}

#ifdef CONFIG_NVGPU_POWER_PG
	/* enable elpg again */
	err = nvgpu_pg_elpg_enable(g);
	if (err != 0) {
		 nvgpu_err(g, "ELPG enable failed.");
	}
#endif
	return err;
}

static void ga10b_intr_gr_stall_interrupt_handling(struct gk20a *g,
		u64 unit_subtree_mask)
{
	int err;
	u64 engine_intr_mask;
	u32 vectorid;
	u32 gr_instance_id;
	const struct nvgpu_device *dev;
	struct nvgpu_intr_unit_info *intr_unit_info =
		&g->mc.intr_unit_info[NVGPU_CIC_INTR_UNIT_GR_STALL];

	vectorid = intr_unit_info->vectorid[0];

	nvgpu_device_for_each(g, dev, NVGPU_DEVTYPE_GRAPHICS) {
		engine_intr_mask = BIT32(dev->intr_id);
		engine_intr_mask <<= GPU_VECTOR_TO_LEAF_SHIFT(vectorid);

		if ((unit_subtree_mask & engine_intr_mask) == 0ULL) {
			continue;
		}

		gr_instance_id =
			nvgpu_grmgr_get_gr_instance_id_for_syspipe(
				g, dev->inst_id);

		err = nvgpu_gr_exec_with_err_for_instance(g,
			gr_instance_id, ga10b_intr_gr_stall_isr(g));

		if (err != 0) {
			nvgpu_err(g,
				"Unable to handle GR STALL interrupt "
					"inst_id : %u Vectorid : 0x%08x "
					"intr_id : 0x%08x "
					"gr_instance_id : %u "
					"engine_intr_mask : 0x%llx "
					"unit_subtree_mask : 0x%llx",
				dev->inst_id, vectorid, dev->intr_id,
				gr_instance_id, engine_intr_mask,
				unit_subtree_mask);
		} else {
			nvgpu_log(g, gpu_dbg_mig,
				"GR STALL interrupt handled "
					"inst_id : %u Vectorid : 0x%08x "
					"intr_id : 0x%08x "
					"gr_instance_id : %u "
					"engine_intr_mask : 0x%llx "
					"unit_subtree_mask : 0x%llx",
				dev->inst_id, vectorid, dev->intr_id,
				gr_instance_id, engine_intr_mask,
				unit_subtree_mask);
		}
	}
}

static void ga10b_intr_isr_stall_host2soc_3(struct gk20a *g)
{
	u32 intr_leaf0, intr_leaf1;
	u64 unit_subtree_mask;
	u64 handled_subtree_mask = 0ULL;
	u32 subtree;

	/**
	 * New interrupt line
	 * HOST2SOC_3_INTR_ID: 71: 3_subtree: leaf0, leaf1 (leaf 6, 7)
	 * Leaf 6 is for engine stall interrupts
	 * Leaf 7 is for runlist_tree_1
	 */

	subtree = HOST2SOC_3_SUBTREE;
	ga10b_intr_subtree_leaf0_leaf1_status(g, subtree,
		&intr_leaf0, &intr_leaf1);

	if (ga10b_intr_is_unit_pending(g, NVGPU_CIC_INTR_UNIT_GR_STALL,
				intr_leaf0, intr_leaf1,
				&unit_subtree_mask) == true) {

		handled_subtree_mask |= unit_subtree_mask;
		ga10b_intr_subtree_clear(g, subtree, unit_subtree_mask);

		ga10b_intr_gr_stall_interrupt_handling(g, unit_subtree_mask);
	}
	if (ga10b_intr_is_unit_pending(g, NVGPU_CIC_INTR_UNIT_CE_STALL,
				intr_leaf0, intr_leaf1,
				&unit_subtree_mask) == true) {
		u32 i;
		u64 engine_intr_mask;
		u32 vectorid;
		const struct nvgpu_device *dev;
#ifdef CONFIG_NVGPU_POWER_PG
		int err;
#endif

		vectorid =
		   g->mc.intr_unit_info[NVGPU_CIC_INTR_UNIT_CE_STALL].vectorid[0];

		handled_subtree_mask |= unit_subtree_mask;
		ga10b_intr_subtree_clear(g, subtree, unit_subtree_mask);

#ifdef CONFIG_NVGPU_POWER_PG
		/* disable elpg before accessing CE registers */
		err = nvgpu_pg_elpg_disable(g);
		if (err != 0) {
			nvgpu_err(g, "ELPG disable failed");
			/* enable ELPG again so that PG SM is in known state*/
			(void) nvgpu_pg_elpg_enable(g);
			goto exit;
		}
#endif

		for (i = 0U; i < g->fifo.num_engines; i++) {
			dev = g->fifo.active_engines[i];

			engine_intr_mask = BIT32(dev->intr_id);
			engine_intr_mask <<= GPU_VECTOR_TO_LEAF_SHIFT(vectorid);
			if ((unit_subtree_mask & engine_intr_mask) == 0ULL) {
				continue;
			}

			nvgpu_ce_stall_isr(g, dev->inst_id, dev->pri_base);
			g->ops.ce.intr_retrigger(g, dev->inst_id);

		}

#ifdef CONFIG_NVGPU_POWER_PG
		/* enable elpg again */
		(void) nvgpu_pg_elpg_enable(g);
#endif
	}
#ifdef CONFIG_NVGPU_POWER_PG
exit:
#endif
	ga10b_intr_subtree_clear_unhandled(g, subtree, intr_leaf0, intr_leaf1,
				handled_subtree_mask);
}

/* Handle and clear interrupt for subtree 1, 2 and 3 */
void ga10b_intr_isr_stall(struct gk20a *g)
{
	u32 top_pending;

	top_pending = g->ops.mc.intr_stall(g);
	if (top_pending == 0U) {
		nvgpu_log(g, gpu_dbg_intr, "stall intr already handled");
		return;
	}
	/**
	 * Legacy nonstall
	 * HOST2SOC_0_INTR_ID: 67: 0_subtree: leaf0, leaf1 (leaf 0, 1)
	 * Leaf 0 is used for engine nonstall interrupts
	 * Leaf 1 is empty
	 *
	 * New interrupt line
	 * HOST2SOC_1_INTR_ID: 68: 1_subtree: leaf0, leaf1 (leaf 2, 3)
	 * Leaf 2 is for mmu_replayable fault and hub_access_cntr
	 * Leaf 3 is empty
	 *
	 * Legacy stall
	 * HOST2SOC_2_INTR_ID: 70: 2_subtree: leaf0, leaf1 (leaf 4, 5)
	 * Leaf 4 is for mmu_*, pbus, priv, ltc etc.
	 * Leaf 5 is for runlist_tree0
	 *
	 * New interrupt line
	 * HOST2SOC_3_INTR_ID: 71: 3_subtree: leaf0, leaf1 (leaf 6, 7)
	 * Leaf 6 is for engine stall interrupts
	 * Leaf 7 is for runlist_tree_1
	 */

	/*
	 * The cpu leaf bit in each interrupt subtree is handled as follows:
	 * - Each bit in the leaf register represents an interrupt vector.
	 * - Each vector is mapped to a unit. A unit may have multiple
	 *   vectors mapped to it.
	 * - Attempt to map pending vectors in the CPU leaf register to a
	 *   specific unit, this is accomplished using a unit level bitmask.
	 *   - If match is found:
	 *     - Clear the corresponding bits in the CPU leaf registers of the
	 *       subtree.
	 *     - Call unit level interrupt handler.
	 *     - Call interrupt retrigger if unit implements one.
	 *   - Not found:
	 *     - Clear the CPU leaf register anyways.
	 *
	 * Interrupt Retriggering:
	 *
	 * In ga10b the interrupt tree is composed of two 32bit top level
	 * registers cpu_top_0/1. The lower 4 bits of cpu_top_0 are connected
	 * to 4 interrupt lines, while the other bits are left unused,
	 * unconnected.
	 *
	 * Each bit in the cpu_top_0/1 is rolled up from a pair of registers
	 * cpu_leaf_0/1. Similarly each bit in cpu_leaf_0/1 is latched to the
	 * interrupt signals from respective hw units at +ve edges.
	 * A hardware unit may further implement its own intermediate interrupt
	 * tree, comprising of serveral status registers. The unit level
	 * interrupt status is rolled up to the top level tree via a interrupt
	 * output signal.
	 *
	 * However, the edge latching at the cpu_leaf register introduces a
	 * possible race condition for hw units which performs level based
	 * roll up of interrupt signal i.e. a race might happen between sw
	 * reading the interrupt status and hw setting bits within the same
	 * register. In such a scenario, the unhandled, pending bits in the
	 * hardware unit will remain high. However an interrupt will not be
	 * generated once the sw handles the seen interrupts and clears the
	 * corresponding cpu_leaf register bit. This is on account of the edge
	 * latching at the cpu_leaf registers, which sets bits only when there
	 * is a +ve edge detected on the interrupt signal from the hw unit.
	 *
	 * In order to mitigate this race condition ga10b introduces a
	 * *_INTR_RETRIGGER register for engines which generate level rolled up
	 * interrupt signals. The *_INTR_RETRIGGER register is normally wired
	 * to 1 and is logically ANDed with interrupt output of the hw unit,
	 * which then is edge latched to the leaf register bits. Once sw
	 * services a unit interrupt, it writes to its *_INTR_RETRIGGER
	 * register, this causes it to be pulled down to 0 for a short time and
	 * back to 1. This ensures unhandled hw unit interrupts are seen as a
	 * +ve by the cpu_leaf register and would interrupt alert the CPU.
	 */

	/* Handle interrupts for 3_subtree */
	if (top_pending &
		BIT32(HOST2SOC_SUBTREE_TO_TOP_BIT(HOST2SOC_3_SUBTREE))) {

		ga10b_intr_isr_stall_host2soc_3(g);
	}

	/* Handle interrupts for 2_subtree */
	if (top_pending &
		BIT32(HOST2SOC_SUBTREE_TO_TOP_BIT(HOST2SOC_2_SUBTREE))) {

		ga10b_intr_isr_stall_host2soc_2(g);
	}

	/* Handle interrupts for 1_subtree */
	if (top_pending &
		BIT32(HOST2SOC_SUBTREE_TO_TOP_BIT(HOST2SOC_1_SUBTREE))) {

		ga10b_intr_isr_stall_host2soc_1(g);
	}

	return;
}

bool ga10b_intr_is_mmu_fault_pending(struct gk20a *g)
{
	u64 subtree_mask = 0ULL;
	u32 intr_leaf0, intr_leaf1;
	bool mmu_fault_pending = false;

	ga10b_intr_subtree_leaf0_leaf1_status(g, HOST2SOC_2_SUBTREE,
		&intr_leaf0, &intr_leaf1);

	if (ga10b_intr_is_unit_pending(g,
		NVGPU_CIC_INTR_UNIT_MMU_FAULT_ECC_ERROR,
		intr_leaf0, intr_leaf1, &subtree_mask) == true) {
		mmu_fault_pending = true;
	}

	if (ga10b_intr_is_unit_pending(g,
		NVGPU_CIC_INTR_UNIT_MMU_REPLAYABLE_FAULT_ERROR,
		intr_leaf0, intr_leaf1, &subtree_mask) == true) {
		mmu_fault_pending = true;
	}
	if (ga10b_intr_is_unit_pending(g,
		NVGPU_CIC_INTR_UNIT_MMU_NON_REPLAYABLE_FAULT,
		intr_leaf0, intr_leaf1, &subtree_mask) == true) {
		mmu_fault_pending = true;
	}
	if (ga10b_intr_is_unit_pending(g,
		NVGPU_CIC_INTR_UNIT_MMU_NON_REPLAYABLE_FAULT_ERROR,
		intr_leaf0, intr_leaf1, &subtree_mask) == true) {
		mmu_fault_pending = true;
	}
	if (ga10b_intr_is_unit_pending(g,
		NVGPU_CIC_INTR_UNIT_MMU_INFO_FAULT,
		intr_leaf0, intr_leaf1, &subtree_mask) == true) {
		mmu_fault_pending = true;
	}

	if (mmu_fault_pending == true) {
		nvgpu_log(g, gpu_dbg_intr, "2_subtree mmu_fault_pending: 0x%llx"
			, subtree_mask);
	}

	ga10b_intr_subtree_leaf0_leaf1_status(g, HOST2SOC_1_SUBTREE,
		&intr_leaf0, &intr_leaf1);
	if (ga10b_intr_is_unit_pending(g,
		NVGPU_CIC_INTR_UNIT_MMU_REPLAYABLE_FAULT,
		intr_leaf0, intr_leaf1, &subtree_mask) == true) {
		mmu_fault_pending = true;
		nvgpu_log(g, gpu_dbg_intr, "1_subtree mmu_fault_pending: 0x%llx"
			, subtree_mask);
	}

	return mmu_fault_pending;
}

static bool ga10b_intr_is_eng_stall_pending(struct gk20a *g, u32 engine_id)
{
	u64 eng_subtree_mask = 0ULL;
	u64 subtree_mask = 0ULL;
	u32 intr_leaf0, intr_leaf1;
	u32 reg_val, vectorid;
	bool eng_stall_pending = false;

	reg_val = nvgpu_readl(g,
		    ctrl_legacy_engine_stall_intr_base_vectorid_r());
	vectorid =
		  ctrl_legacy_engine_stall_intr_base_vectorid_vector_v(reg_val);

	eng_subtree_mask = ((u64)nvgpu_engine_act_interrupt_mask(g, engine_id));
	eng_subtree_mask <<= GPU_VECTOR_TO_LEAF_SHIFT(vectorid);

	ga10b_intr_subtree_leaf0_leaf1_status(g, HOST2SOC_3_SUBTREE,
		&intr_leaf0, &intr_leaf1);

	if (ga10b_intr_is_unit_pending(g,
		NVGPU_CIC_INTR_UNIT_GR_STALL,
		intr_leaf0, intr_leaf1, &subtree_mask) == true) {
		if (subtree_mask & eng_subtree_mask) {
			eng_stall_pending = true;
		}
	}

	if (ga10b_intr_is_unit_pending(g,
		NVGPU_CIC_INTR_UNIT_CE_STALL,
		intr_leaf0, intr_leaf1, &subtree_mask) == true) {
		if (subtree_mask & eng_subtree_mask) {
			eng_stall_pending = true;
		}
	}

	return eng_stall_pending;
}

bool ga10b_intr_is_stall_and_eng_intr_pending(struct gk20a *g, u32 engine_id,
			u32 *eng_intr_pending)
{
	u32 stall_intr;

	*eng_intr_pending = ga10b_intr_is_eng_stall_pending(g, engine_id);

	stall_intr = ga10b_intr_stall(g);

	nvgpu_log(g, gpu_dbg_info | gpu_dbg_intr,
		"intr_top = 0x%08x, eng_intr_pending = 0x%08x",
		stall_intr, *eng_intr_pending);

	return (stall_intr != 0U);
}
