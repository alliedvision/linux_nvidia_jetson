/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/log.h>
#include <nvgpu/io.h>
#include <nvgpu/runlist.h>
#include <nvgpu/mc.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/rc.h>

#include "fifo_utils_ga10b.h"
#include "fifo_intr_ga10b.h"
#include "ctxsw_timeout_ga10b.h"
#include "pbdma_ga10b.h"

#include <nvgpu/hw/ga10b/hw_runlist_ga10b.h>

/**
 *     [runlist's tree 0 bit] <---------. .---------> [runlist's tree 1 bit]
 *                                       Y
 *                                       |
 *                                       |
 *     [runlist intr tree 0]             ^             [runlist intr tree 1]
 *                       ______________/   \______________
 *                      /                                  |
 *     NV_RUNLIST_INTR_VECTORID(0) msg       NV_RUNLIST_INTR_VECTORID(1) msg
 *                     |                                   |
 *               ______^______                       ______^______
 *              /             \                     /             \
 *             '_______________'                   '_______________'
 *              |||||||       |                     |       |||||||
 *            other tree0     |                     |     other tree1
 *          ANDed intr bits   ^                     ^   ANDed intr bits
 *                           AND                   AND
 *                           | |                   | |
 *                    _______. .______      _______. .________
 *                   /                 \   /                  \
 *RUNLIST_INTR_0_EN_SET_TREE(0)_intr_bit Y RUNLIST_INTR_0_EN_SET_TREE(1)_intr_bit
 *                                       |
 *                           NV_RUNLIST_INTR_0_intr_bit
 */

static u32 runlist_intr_0_mask(void)
{
	u32 mask = (runlist_intr_0_en_set_tree_ctxsw_timeout_eng0_enabled_f() |
		runlist_intr_0_en_set_tree_ctxsw_timeout_eng1_enabled_f() |
		runlist_intr_0_en_set_tree_ctxsw_timeout_eng2_enabled_f() |
		runlist_intr_0_en_set_tree_pbdma0_intr_tree_0_enabled_f() |
		runlist_intr_0_en_set_tree_pbdma1_intr_tree_0_enabled_f() |
		runlist_intr_0_en_set_tree_bad_tsg_enabled_f());

	return mask;
}

static u32 runlist_intr_0_recover(void)
{
	u32 mask = runlist_intr_0_en_clear_tree_ctxsw_timeout_eng0_enabled_f() |
		runlist_intr_0_en_clear_tree_ctxsw_timeout_eng1_enabled_f() |
		runlist_intr_0_en_clear_tree_ctxsw_timeout_eng2_enabled_f();

	return mask;
}

static u32 runlist_intr_0_recover_unmask(void)
{
	u32 mask = runlist_intr_0_en_set_tree_ctxsw_timeout_eng0_enabled_f() |
		runlist_intr_0_en_set_tree_ctxsw_timeout_eng1_enabled_f() |
		runlist_intr_0_en_set_tree_ctxsw_timeout_eng2_enabled_f();

	return mask;
}

static u32 runlist_intr_0_ctxsw_timeout_mask(void)
{
	u32 mask = runlist_intr_0_en_clear_tree_ctxsw_timeout_eng0_enabled_f() |
		runlist_intr_0_en_clear_tree_ctxsw_timeout_eng1_enabled_f() |
		runlist_intr_0_en_clear_tree_ctxsw_timeout_eng2_enabled_f();

	return mask;
}

static const char *const ga10b_bad_tsg_error_str[] = {
	"no_error",
	"zero_length_tsg",
	"max_length_exceeded",
	"runlist_overflow",
	"expected_a_chid_entry",
	"expected_a_tsg_header",
	"invalid_runqueue",
};

void ga10b_fifo_runlist_intr_vectorid_init(struct gk20a *g)
{
	u32 i, intr_tree, reg_val, intr_unit;
	u32 vectorid_tree[NVGPU_CIC_INTR_VECTORID_SIZE_MAX];
	u32 num_vectorid;
	struct nvgpu_runlist *runlist;

	for (intr_tree = 0U; intr_tree < runlist_intr_vectorid__size_1_v();
			intr_tree++) {

		intr_unit = NVGPU_CIC_INTR_UNIT_RUNLIST_TREE_0 + intr_tree;

		if (nvgpu_cic_mon_intr_is_unit_info_valid(g, intr_unit) == true) {
			/* intr_unit_info is already set by s/w */
			continue;
		}
		num_vectorid = 0U;
		for (i = 0U; i < g->fifo.num_runlists; i++) {

			runlist = &g->fifo.active_runlists[i];

			reg_val = nvgpu_runlist_readl(g, runlist,
				runlist_intr_vectorid_r(intr_tree));
			vectorid_tree[i] =
				runlist_intr_vectorid_vector_v(reg_val);

			num_vectorid ++;

			nvgpu_log_info(g,
				"init runlist: %u intr_tree_%d vectorid",
				i, intr_tree);
		}
		nvgpu_cic_mon_intr_unit_vectorid_init(g, intr_unit,
			vectorid_tree, num_vectorid);
	}

}

void ga10b_fifo_intr_top_enable(struct gk20a *g, bool enable)
{
	if (enable) {
		nvgpu_cic_mon_intr_stall_unit_config(g,
			NVGPU_CIC_INTR_UNIT_RUNLIST_TREE_0, NVGPU_CIC_INTR_ENABLE);

		/**
		 * RUNLIST_TREE_1 interrupts are not enabled as all runlist
		 * interrupts are routed to runlist_tree_0
		 */
		nvgpu_cic_mon_intr_stall_unit_config(g,
			NVGPU_CIC_INTR_UNIT_RUNLIST_TREE_1, NVGPU_CIC_INTR_DISABLE);
	} else {
		nvgpu_cic_mon_intr_stall_unit_config(g,
				NVGPU_CIC_INTR_UNIT_RUNLIST_TREE_0, NVGPU_CIC_INTR_DISABLE);
	}
}

static void ga10b_fifo_runlist_intr_disable(struct gk20a *g)
{
	u32 i, intr_tree, reg_val;
	struct nvgpu_runlist *runlist;

	/** Disable raising interrupt for both runlist trees to CPU and GSP */
	for (i = 0U; i < g->fifo.num_runlists; i++) {
		runlist = &g->fifo.active_runlists[i];
		for (intr_tree = 0U;
			intr_tree < runlist_intr_vectorid__size_1_v();
			intr_tree++) {

			reg_val = nvgpu_runlist_readl(g, runlist,
				runlist_intr_vectorid_r(intr_tree));
			reg_val &= ~(runlist_intr_vectorid_cpu_enable_f() |
					runlist_intr_vectorid_gsp_enable_f());
			nvgpu_runlist_writel(g, runlist,
				runlist_intr_vectorid_r(intr_tree), reg_val);
			nvgpu_log_info(g, "intr_vectorid_r[tree_%u]: 0x%08x",
				i + intr_tree, reg_val);
		}
		/** Clear interrupts */
		reg_val = nvgpu_runlist_readl(g, runlist, runlist_intr_0_r());

		nvgpu_runlist_writel(g, runlist, runlist_intr_0_r(), reg_val);
	}
}

static void ga10b_fifo_runlist_intr_enable(struct gk20a *g)
{
	u32 i, intr_tree_0, intr_tree_1, reg_val;
	u32 intr0_en_mask;
	struct nvgpu_runlist *runlist;

	intr_tree_0 = 0U;
	intr_tree_1 = 1U;
	intr0_en_mask = runlist_intr_0_mask();

	for (i = 0U; i < g->fifo.num_runlists; i++) {
		runlist = &g->fifo.active_runlists[i];
		/**
		 * runlist_intr_0 interrupts can be routed to either
		 * tree0 or tree1 vector using runlist_intr_0_en_set_tree(0) or
		 * runlist_intr_0_en_set_tree(1) register. For now route all
		 * interrupts to tree0.
		 */
		/** Clear interrupts */
		reg_val = nvgpu_runlist_readl(g, runlist, runlist_intr_0_r());

		nvgpu_runlist_writel(g, runlist, runlist_intr_0_r(), reg_val);

		/** Enable interrupts in tree(0) */
		nvgpu_runlist_writel(g, runlist,
			runlist_intr_0_en_set_tree_r(intr_tree_0),
			intr0_en_mask);
		/** Disable all interrupts in tree(1) */
		nvgpu_runlist_writel(g, runlist,
			runlist_intr_0_en_clear_tree_r(intr_tree_1),
			U32_MAX);

		reg_val = nvgpu_runlist_readl(g, runlist,
			runlist_intr_vectorid_r(intr_tree_0));
		/** disable raising interrupt to gsp */
		reg_val &= ~(runlist_intr_vectorid_gsp_enable_f());

		/** enable raising interrupt to cpu */
		reg_val |= runlist_intr_vectorid_cpu_enable_f();

		/** enable runlist tree 0 interrupts at runlist level */
		nvgpu_runlist_writel(g, runlist,
				runlist_intr_vectorid_r(intr_tree_0), reg_val);

		reg_val = nvgpu_runlist_readl(g, runlist,
			runlist_intr_vectorid_r(intr_tree_1));

		/** disable raising interrupt to gsp */
		reg_val &= ~(runlist_intr_vectorid_gsp_enable_f());

		/** disable raising interrupt to cpu */
		reg_val &= ~(runlist_intr_vectorid_cpu_enable_f());

		/** Disable runlist tree 1 interrupts at runlist level */
		nvgpu_runlist_writel(g, runlist,
				runlist_intr_vectorid_r(intr_tree_1), reg_val);
	}
}
void ga10b_fifo_intr_0_enable(struct gk20a *g, bool enable)
{

	ga10b_fifo_runlist_intr_disable(g);

	if (!enable) {
		g->ops.fifo.ctxsw_timeout_enable(g, false);
		g->ops.pbdma.intr_enable(g, false);
		return;
	}

	/* Enable interrupts */
	g->ops.fifo.ctxsw_timeout_enable(g, true);
	g->ops.pbdma.intr_enable(g, true);

	ga10b_fifo_runlist_intr_enable(g);

}

void ga10b_fifo_intr_1_enable(struct gk20a *g, bool enable)
{
	(void)g;
	(void)enable;
	return;
}

static void ga10b_fifo_handle_bad_tsg(struct gk20a *g,
			struct nvgpu_runlist *runlist)
{
	u32 bad_tsg;
	u32 bad_tsg_code;

	bad_tsg = nvgpu_runlist_readl(g, runlist, runlist_intr_bad_tsg_r());
	bad_tsg_code = runlist_intr_bad_tsg_code_v(bad_tsg);

	if (bad_tsg_code < ARRAY_SIZE(ga10b_bad_tsg_error_str)) {
		nvgpu_err(g, "runlist bad tsg error: %s",
			ga10b_bad_tsg_error_str[bad_tsg_code]);
	} else {
		nvgpu_err(g, "runlist bad tsg error code not supported");
	}

	nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_HOST,
			GPU_HOST_PFIFO_SCHED_ERROR);

	/* id is unknown, preempt all runlists and do recovery */
	/* TBD: nvgpu_rc_sched_error_bad_tsg(g); */
}

static void ga10b_fifo_runlist_intr_clear(struct gk20a *g)
{
	u32 i, intr_0;
	struct nvgpu_runlist *runlist;

	for (i = 0U; i < g->fifo.num_runlists; i++) {
		runlist = &g->fifo.active_runlists[i];

		intr_0 = nvgpu_runlist_readl(g, runlist, runlist_intr_0_r());

		nvgpu_err(g, "unhandled runlist(%d) intr_0: 0x%08x", i, intr_0);

		nvgpu_runlist_writel(g, runlist, runlist_intr_0_r(), intr_0);
	}
}

void ga10b_fifo_intr_0_isr(struct gk20a *g)
{
	u32 i, intr_0, handled_intr_0 = 0U;
	u32 intr_0_en_mask = 0U;
	u32 pbdma_idx = 0U;
	u32 intr_tree_0 = 0U, intr_tree_1 = 1U;
	struct nvgpu_runlist *runlist;

	/* TODO: sw_ready is needed only for recovery part */
	if (!g->fifo.sw_ready) {
		ga10b_fifo_runlist_intr_clear(g);
		return;
	}
	/* note we're not actually in an "isr", but rather
	 * in a threaded interrupt context... */
	nvgpu_mutex_acquire(&g->fifo.intr.isr.mutex);

	for (i = 0U; i < g->fifo.num_runlists; i++) {
		runlist = &g->fifo.active_runlists[i];

		intr_0 = nvgpu_runlist_readl(g, runlist, runlist_intr_0_r());

		if (intr_0 & runlist_intr_0_bad_tsg_pending_f()) {
			ga10b_fifo_handle_bad_tsg(g, runlist);
			handled_intr_0 |= runlist_intr_0_bad_tsg_pending_f();
		}

		for (pbdma_idx = 0U;
			pbdma_idx < runlist_intr_0_pbdmai_intr_tree_j__size_1_v();
			pbdma_idx++) {
			if (intr_0 &
				runlist_intr_0_pbdmai_intr_tree_j_pending_f(pbdma_idx, intr_tree_0)) {
				ga10b_fifo_pbdma_isr(g, runlist, pbdma_idx);
				handled_intr_0 |= runlist_intr_0_pbdmai_intr_tree_j_pending_f(pbdma_idx, intr_tree_0);
			}
		}

		if ((intr_0 & runlist_intr_0_ctxsw_timeout_mask()) != 0U) {
			ga10b_fifo_ctxsw_timeout_isr(g, runlist);
			handled_intr_0 |=
				(runlist_intr_0_ctxsw_timeout_mask() & intr_0);
		}

		/*
		 * The runlist_intr_0_r register can have bits set for which
		 * interrupts are not enabled by the SW. Hence, create a mask
		 * of all the runlist interrupts enabled on both runlist
		 * tree0,1 and consider only these bits when detecting
		 * unhandled interrupts.
		 */
		intr_0_en_mask = nvgpu_runlist_readl(g, runlist,
				runlist_intr_0_en_set_tree_r(intr_tree_0));
		intr_0_en_mask |= nvgpu_runlist_readl(g, runlist,
				runlist_intr_0_en_set_tree_r(intr_tree_1));

		if (handled_intr_0 != (intr_0 & intr_0_en_mask)) {
			nvgpu_err(g,
				"unhandled runlist(%d) intr_0: 0x%08x "
				"handled: 0x%08x",
				i, intr_0 & intr_0_en_mask, handled_intr_0);
		}

		handled_intr_0 = 0U;
		/** Clear interrupts */
		nvgpu_runlist_writel(g, runlist, runlist_intr_0_r(), intr_0);
	}
	nvgpu_mutex_release(&g->fifo.intr.isr.mutex);

}

void ga10b_fifo_intr_set_recover_mask(struct gk20a *g)
{

	u32 i, intr_tree_0;
	struct nvgpu_runlist *runlist;

	/*
	 * ctxsw timeout error prevents recovery, and ctxsw error will retrigger
	 * every 100ms. Disable ctxsw timeout error to allow recovery.
	 */

	intr_tree_0 = 0U;

	for (i = 0U; i < g->fifo.num_runlists; i++) {
		runlist = &g->fifo.active_runlists[i];
		/**
		 * runlist_intr_0 interrupts can be routed to either
		 * tree0 or tree1 vector using runlist_intr_0_en_set_tree(0) or
		 * runlist_intr_0_en_set_tree(1) register. For now route all
		 * interrupts are routed to tree0.
		 */

		/*
		 * Disable ctxsw interrupts in tree(0) using en_clear_tree_r(0).
		 * Writes of 1 disables reporting of corresponding interrupt,
		 * whereas writes with 0 are ignored. Read returns enabled
		 * interrupts instead of the previous write value.
		 */
		nvgpu_runlist_writel(g, runlist,
			runlist_intr_0_en_clear_tree_r(intr_tree_0),
			runlist_intr_0_recover());
	}
}

void ga10b_fifo_intr_unset_recover_mask(struct gk20a *g)
{
	u32 i, intr_tree_0;
	struct nvgpu_runlist *runlist;

	/*
	 * ctxsw timeout error prevents recovery, and ctxsw error will retrigger
	 * every 100ms. To allow recovery, ctxsw timeout is disabled. Enable
	 * the same after recovery is done.
	 */

	intr_tree_0 = 0U;

	for (i = 0U; i < g->fifo.num_runlists; i++) {
		runlist = &g->fifo.active_runlists[i];
		/**
		 * runlist_intr_0 interrupts can be routed to either
		 * tree0 or tree1 vector using runlist_intr_0_en_set_tree(0) or
		 * runlist_intr_0_en_set_tree(1) register. For now route all
		 * interrupts are routed to tree0.
		 */

		/*
		 * Enable ctxsw interrupts in tree(0) using en_set_tree_r(0).
		 * Writes of 1 enables reporting of corresponding interrupt,
		 * whereas writes with 0 are ignored. Read returns enabled
		 * interrupts instead of the previous write value.
		 */
		nvgpu_runlist_writel(g, runlist,
			runlist_intr_0_en_set_tree_r(intr_tree_0),
			runlist_intr_0_recover_unmask());
	}

}


void ga10b_fifo_pbdma_isr(struct gk20a *g, struct nvgpu_runlist *runlist, u32 pbdma_idx)
{
	u32 pbdma_id;
	const struct nvgpu_pbdma_info *pbdma_info;

	if (pbdma_idx >= PBDMA_PER_RUNLIST_SIZE) {
		nvgpu_err(g, "pbdma_idx(%d) >= max_pbdmas_per_runlist(%d)",
				pbdma_idx, PBDMA_PER_RUNLIST_SIZE);
		return;
	}
	pbdma_info = runlist->pbdma_info;
	pbdma_id = pbdma_info->pbdma_id[pbdma_idx];
	if (pbdma_id == PBDMA_ID_INVALID) {
		nvgpu_err(g, "runlist_id(%d), pbdma_idx(%d): invalid PBDMA",
				runlist->id, pbdma_idx);
		return;
	}
	g->ops.pbdma.handle_intr(g, pbdma_id, true);
}

void ga10b_fifo_runlist_intr_retrigger(struct gk20a *g, u32 intr_tree)
{
	u32 i = 0U;
	struct nvgpu_runlist *runlist;

	for (i = 0U; i < g->fifo.num_runlists; i++) {
		runlist = &g->fifo.active_runlists[i];

		nvgpu_runlist_writel(g, runlist,
				runlist_intr_retrigger_r(intr_tree),
				runlist_intr_retrigger_trigger_true_f());
	}

}
