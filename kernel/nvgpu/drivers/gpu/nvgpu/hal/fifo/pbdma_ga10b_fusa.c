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

#include <nvgpu/log.h>
#include <nvgpu/log2.h>
#include <nvgpu/utils.h>
#include <nvgpu/debug.h>
#include <nvgpu/io.h>
#include <nvgpu/bitops.h>
#include <nvgpu/error_notifier.h>
#include <nvgpu/fifo.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/pbdma_status.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/runlist.h>
#include <nvgpu/rc.h>
#include <nvgpu/device.h>

#include "pbdma_ga10b.h"

#include <nvgpu/hw/ga10b/hw_pbdma_ga10b.h>

static u32 pbdma_intr_0_en_set_tree_mask(void)
{
	u32 mask = pbdma_intr_0_en_set_tree_gpfifo_enabled_f()    |
			pbdma_intr_0_en_set_tree_gpptr_enabled_f()     |
			pbdma_intr_0_en_set_tree_gpentry_enabled_f()   |
			pbdma_intr_0_en_set_tree_gpcrc_enabled_f()     |
			pbdma_intr_0_en_set_tree_pbptr_enabled_f()     |
			pbdma_intr_0_en_set_tree_pbentry_enabled_f()   |
			pbdma_intr_0_en_set_tree_pbcrc_enabled_f()     |
			pbdma_intr_0_en_set_tree_method_enabled_f()    |
			pbdma_intr_0_en_set_tree_device_enabled_f()    |
			pbdma_intr_0_en_set_tree_eng_reset_enabled_f() |
			pbdma_intr_0_en_set_tree_semaphore_enabled_f() |
			pbdma_intr_0_en_set_tree_acquire_enabled_f()   |
			pbdma_intr_0_en_set_tree_pri_enabled_f()       |
			pbdma_intr_0_en_set_tree_pbseg_enabled_f()     |
			pbdma_intr_0_en_set_tree_signature_enabled_f();

	return mask;
}

static u32 pbdma_intr_0_en_clear_tree_mask(void)
{
	u32 mask = pbdma_intr_0_en_clear_tree_gpfifo_enabled_f()    |
			pbdma_intr_0_en_clear_tree_gpptr_enabled_f()     |
			pbdma_intr_0_en_clear_tree_gpentry_enabled_f()   |
			pbdma_intr_0_en_clear_tree_gpcrc_enabled_f()     |
			pbdma_intr_0_en_clear_tree_pbptr_enabled_f()     |
			pbdma_intr_0_en_clear_tree_pbentry_enabled_f()   |
			pbdma_intr_0_en_clear_tree_pbcrc_enabled_f()     |
			pbdma_intr_0_en_clear_tree_method_enabled_f()    |
			pbdma_intr_0_en_clear_tree_device_enabled_f()    |
			pbdma_intr_0_en_clear_tree_eng_reset_enabled_f() |
			pbdma_intr_0_en_clear_tree_semaphore_enabled_f() |
			pbdma_intr_0_en_clear_tree_acquire_enabled_f()   |
			pbdma_intr_0_en_clear_tree_pri_enabled_f()       |
			pbdma_intr_0_en_clear_tree_pbseg_enabled_f()     |
			pbdma_intr_0_en_clear_tree_signature_enabled_f();

	return mask;
}

static u32 pbdma_intr_1_en_set_tree_mask(void)
{	u32 mask = pbdma_intr_1_en_set_tree_hce_re_illegal_op_enabled_f() |
			pbdma_intr_1_en_set_tree_hce_re_alignb_enabled_f()     |
			pbdma_intr_1_en_set_tree_hce_priv_enabled_f()          |
			pbdma_intr_1_en_set_tree_hce_illegal_mthd_enabled_f()  |
			pbdma_intr_1_en_set_tree_hce_illegal_class_enabled_f() |
			pbdma_intr_1_en_set_tree_ctxnotvalid_enabled_f();

	return mask;
}

static u32 pbdma_intr_1_en_clear_tree_mask(void)
{
	u32 mask = pbdma_intr_1_en_clear_tree_hce_re_illegal_op_enabled_f() |
			pbdma_intr_1_en_clear_tree_hce_re_alignb_enabled_f()     |
			pbdma_intr_1_en_clear_tree_hce_priv_enabled_f()          |
			pbdma_intr_1_en_clear_tree_hce_illegal_mthd_enabled_f()  |
			pbdma_intr_1_en_clear_tree_hce_illegal_class_enabled_f() |
			pbdma_intr_1_en_clear_tree_ctxnotvalid_enabled_f();

	return mask;
}
/**
 * nvgpu will route all pbdma intr to tree_0
 * The interrupt registers NV_PPBDMA_INTR_* contain and control the interrupt
 * state for each PBDMA. Interrupts are set by events and are cleared by software
 * running on the CPU or GSP.
 *
 * Interrupts in the PBDMA are divided into two interrupt trees:
 * RUNLIST_INTR_0_PBDMAn_INTR_TREE_0   RUNLIST_INTR_0_PBDMAn_INTR_TREE_1
 *                      |                                   |
 *                ______^______                       ______^______
 *               /             \                     /             \
 *              |      OR       |                   |      OR       |
 *              '_______________'                   '_______________'
 *               |||||||       |                     |       |||||||
 *             other tree0     |                     |     other tree1
 *           ANDed intr bits   ^                     ^   ANDed intr bits
 *                            AND                   AND
 *                            | |                   | |
 *                     _______. .______      _______. .________
 *                    /                 \   /                  \
 *                   |                   \ /                    |
 * PPBDMA_INTR_0/1_EN_SET_TREE(p,0)_intr  Y  PPBDMA_INTR_0/1_EN_SET_TREE(p,1)_intr
 *                                        |
 *                           NV_PPBDMA_INTR_0/1_intr_bit
 */


/* TBD: NVGPU-4516: Update fault_type_desc */
static const char *const pbdma_intr_fault_type_desc[] = {
	"MEMREQ timeout", "MEMACK_TIMEOUT", "MEMACK_EXTRA acks",
	"MEMDAT_TIMEOUT", "MEMDAT_EXTRA acks", "MEMFLUSH noack",
	"MEMOP noack", "LBCONNECT noack", "NONE - was LBREQ",
	"LBACK_TIMEOUT", "LBACK_EXTRA acks", "LBDAT_TIMEOUT",
	"LBDAT_EXTRA acks", "GPFIFO won't fit", "GPPTR invalid",
	"GPENTRY invalid", "GPCRC mismatch", "PBPTR get>put",
	"PBENTRY invld", "PBCRC mismatch", "NONE - was XBARC",
	"METHOD invld", "METHODCRC mismat", "DEVICE sw method",
	"[ENGINE]", "SEMAPHORE invlid", "ACQUIRE timeout",
	"PRI forbidden", "ILLEGAL SYNCPT", "[NO_CTXSW_SEG]",
	"PBSEG badsplit", "SIGNATURE bad"
};

static bool ga10b_pbdma_is_sw_method_subch(struct gk20a *g, u32 pbdma_id,
						u32 pbdma_method_index)
{
	u32 pbdma_method_stride;
	u32 pbdma_method_reg, pbdma_method_subch;

	pbdma_method_stride = nvgpu_safe_sub_u32(pbdma_method1_r(pbdma_id),
				pbdma_method0_r(pbdma_id));

	pbdma_method_reg = nvgpu_safe_add_u32(pbdma_method0_r(pbdma_id),
				nvgpu_safe_mult_u32(pbdma_method_index,
					pbdma_method_stride));

	pbdma_method_subch = pbdma_method0_subch_v(
			nvgpu_readl(g, pbdma_method_reg));

	if ((pbdma_method_subch == 5U) ||
		(pbdma_method_subch == 6U) ||
		(pbdma_method_subch == 7U)) {
		return true;
	}

	return false;
}

u32 ga10b_pbdma_set_clear_intr_offsets(struct gk20a *g,
			u32 set_clear_size)
{
	u32 ret = 0U;
	switch(set_clear_size) {
		case INTR_SIZE:
			ret = pbdma_intr_0__size_1_v();
			break;
		case INTR_SET_SIZE:
			ret = pbdma_intr_0_en_set_tree__size_1_v();
			break;
		case INTR_CLEAR_SIZE:
			ret = pbdma_intr_0_en_clear_tree__size_1_v();
			break;
		default:
			nvgpu_err(g, "Invalid input for set_clear_intr_offset");
			break;
	}

	return ret;
}

static void ga10b_pbdma_disable_all_intr(struct gk20a *g)
{
	u32 pbdma_id = 0U;
	u32 tree = 0U;
	u32 pbdma_id_max =
			g->ops.pbdma.set_clear_intr_offsets(g, INTR_CLEAR_SIZE);

	for (pbdma_id = 0U; pbdma_id < pbdma_id_max; pbdma_id++) {
		for (tree = 0U; tree < pbdma_intr_0_en_clear_tree__size_2_v();
			tree++) {
			nvgpu_writel(g, pbdma_intr_0_en_clear_tree_r(pbdma_id,
				tree), pbdma_intr_0_en_clear_tree_mask());
			nvgpu_writel(g, pbdma_intr_1_en_clear_tree_r(pbdma_id,
				tree), pbdma_intr_1_en_clear_tree_mask());
		}
	}
}

void ga10b_pbdma_clear_all_intr(struct gk20a *g, u32 pbdma_id)
{
	nvgpu_writel(g, pbdma_intr_0_r(pbdma_id), U32_MAX);
	nvgpu_writel(g, pbdma_intr_1_r(pbdma_id), U32_MAX);
}

void ga10b_pbdma_disable_and_clear_all_intr(struct gk20a *g)
{
	u32 pbdma_id = 0U;
	u32 pbdma_id_max =
		g->ops.pbdma.set_clear_intr_offsets(g, INTR_SIZE);

	ga10b_pbdma_disable_all_intr(g);

	for (pbdma_id = 0U; pbdma_id < pbdma_id_max; pbdma_id++) {
		ga10b_pbdma_clear_all_intr(g, pbdma_id);
	}
}

static void ga10b_pbdma_dump_intr_0(struct gk20a *g, u32 pbdma_id,
				u32 pbdma_intr_0)
{
	u32 header = nvgpu_readl(g, pbdma_pb_header_r(pbdma_id));
	u32 data = g->ops.pbdma.read_data(g, pbdma_id);
	u32 shadow_0 = nvgpu_readl(g, pbdma_gp_shadow_0_r(pbdma_id));
	u32 shadow_1 = nvgpu_readl(g, pbdma_gp_shadow_1_r(pbdma_id));
	u32 method0 = nvgpu_readl(g, pbdma_method0_r(pbdma_id));
	u32 method1 = nvgpu_readl(g, pbdma_method1_r(pbdma_id));
	u32 method2 = nvgpu_readl(g, pbdma_method2_r(pbdma_id));
	u32 method3 = nvgpu_readl(g, pbdma_method3_r(pbdma_id));

	nvgpu_err(g,
		"pbdma_intr_0(%d):0x%08x PBH: %08x "
		"SHADOW: %08x gp shadow0: %08x gp shadow1: %08x"
		"M0: %08x %08x %08x %08x ",
		pbdma_id, pbdma_intr_0, header, data,
		shadow_0, shadow_1, method0, method1, method2, method3);
}

/* Copied static function */
static u32 pbdma_get_intr_descs(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;
	u32 intr_descs = (f->intr.pbdma.device_fatal_0 |
				f->intr.pbdma.channel_fatal_0 |
				f->intr.pbdma.restartable_0);

	return intr_descs;
}

void ga10b_pbdma_reset_header(struct gk20a *g, u32 pbdma_id)
{
	nvgpu_writel(g, pbdma_pb_header_r(pbdma_id),
			pbdma_pb_header_first_true_f() |
			pbdma_pb_header_type_non_inc_f());
}

void ga10b_pbdma_reset_method(struct gk20a *g, u32 pbdma_id,
			u32 pbdma_method_index)
{
	u32 pbdma_method_stride;
	u32 pbdma_method_reg;

	pbdma_method_stride = nvgpu_safe_sub_u32(pbdma_method1_r(pbdma_id),
				pbdma_method0_r(pbdma_id));

	pbdma_method_reg = nvgpu_safe_add_u32(pbdma_method0_r(pbdma_id),
				nvgpu_safe_mult_u32(pbdma_method_index,
					pbdma_method_stride));

	nvgpu_writel(g, pbdma_method_reg,
			pbdma_method0_valid_true_f() |
			pbdma_method0_first_true_f() |
			pbdma_method0_addr_f(
			     U32(pbdma_udma_nop_r()) >> 2U));
}

u32 ga10b_pbdma_read_data(struct gk20a *g, u32 pbdma_id)
{
	return nvgpu_readl(g, pbdma_hdr_shadow_r(pbdma_id));
}

static void report_pbdma_error(struct gk20a *g, u32 pbdma_id,
		u32 pbdma_intr_0)
{
	u32 err_type = GPU_HOST_INVALID_ERROR;

	/*
	 * Multiple errors have been grouped as part of a single
	 * top-level error.
	 */
	if ((pbdma_intr_0 & (
		pbdma_intr_0_gpfifo_pending_f() |
		pbdma_intr_0_gpptr_pending_f() |
		pbdma_intr_0_gpentry_pending_f() |
		pbdma_intr_0_gpcrc_pending_f() |
		pbdma_intr_0_pbptr_pending_f() |
		pbdma_intr_0_pbentry_pending_f() |
		pbdma_intr_0_pbcrc_pending_f())) != 0U) {
			err_type = GPU_HOST_PBDMA_GPFIFO_PB_ERROR;
	}
	if ((pbdma_intr_0 & (
		pbdma_intr_0_method_pending_f() |
		pbdma_intr_0_device_pending_f() |
		pbdma_intr_0_eng_reset_pending_f() |
		pbdma_intr_0_semaphore_pending_f() |
		pbdma_intr_0_acquire_pending_f() |
		pbdma_intr_0_pri_pending_f() |
		pbdma_intr_0_pbseg_pending_f())) != 0U) {
			err_type = GPU_HOST_PBDMA_METHOD_ERROR;
	}
	if ((pbdma_intr_0 &
		pbdma_intr_0_signature_pending_f()) != 0U) {
			err_type = GPU_HOST_PBDMA_SIGNATURE_ERROR;
	}
	if (err_type != GPU_HOST_INVALID_ERROR) {
		nvgpu_err(g, "pbdma_intr_0(%d)= 0x%08x ",
				pbdma_id, pbdma_intr_0);
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_HOST, err_type);
	}
	return;
}

void ga10b_pbdma_intr_enable(struct gk20a *g, bool enable)
{
	u32 pbdma_id = 0U;
	u32 tree = 0U;
	u32 pbdma_id_max =
		g->ops.pbdma.set_clear_intr_offsets(g, INTR_SET_SIZE);

	if (!enable) {
		ga10b_pbdma_disable_and_clear_all_intr(g);
		return;
	}

	for (pbdma_id = 0U; pbdma_id < pbdma_id_max; pbdma_id++) {

		ga10b_pbdma_clear_all_intr(g, pbdma_id);

		/* enable pbdma interrupts and route to tree_0 */
		nvgpu_writel(g, pbdma_intr_0_en_set_tree_r(pbdma_id,
			tree), pbdma_intr_0_en_set_tree_mask());
		nvgpu_writel(g, pbdma_intr_1_en_set_tree_r(pbdma_id,
			tree), pbdma_intr_1_en_set_tree_mask());
	}
}


void ga10b_pbdma_handle_intr(struct gk20a *g, u32 pbdma_id, bool recover)
{
	struct nvgpu_pbdma_status_info pbdma_status;
	u32 intr_error_notifier = NVGPU_ERR_NOTIFIER_PBDMA_ERROR;

	u32 pbdma_intr_0 = nvgpu_readl(g, pbdma_intr_0_r(pbdma_id));
	u32 pbdma_intr_1 = nvgpu_readl(g, pbdma_intr_1_r(pbdma_id));

	if (pbdma_intr_0 != 0U) {
		nvgpu_log(g, gpu_dbg_info | gpu_dbg_intr,
			"pbdma id %d intr_0 0x%08x pending",
			pbdma_id, pbdma_intr_0);

		if (g->ops.pbdma.handle_intr_0(g, pbdma_id, pbdma_intr_0,
			&intr_error_notifier)) {
			g->ops.pbdma_status.read_pbdma_status_info(g,
				pbdma_id, &pbdma_status);
			if (recover) {
				nvgpu_rc_pbdma_fault(g, pbdma_id,
						intr_error_notifier,
						&pbdma_status);
			}
		}
		nvgpu_writel(g, pbdma_intr_0_r(pbdma_id), pbdma_intr_0);
	}

	if (pbdma_intr_1 != 0U) {
		nvgpu_log(g, gpu_dbg_info | gpu_dbg_intr,
			"pbdma id %d intr_1 0x%08x pending",
			pbdma_id, pbdma_intr_1);

		if (g->ops.pbdma.handle_intr_1(g, pbdma_id, pbdma_intr_1,
			&intr_error_notifier)) {
			g->ops.pbdma_status.read_pbdma_status_info(g,
				pbdma_id, &pbdma_status);
			if (recover) {
				nvgpu_rc_pbdma_fault(g, pbdma_id,
						intr_error_notifier,
						&pbdma_status);
			}
		}
		nvgpu_writel(g, pbdma_intr_1_r(pbdma_id), pbdma_intr_1);
	}
}

static bool ga10b_pbdma_handle_intr_0_legacy(struct gk20a *g, u32 pbdma_id,
			u32 pbdma_intr_0, u32 *error_notifier)
{

	bool recover = false;
	u32 i;
	unsigned long pbdma_intr_err;
	unsigned long bit;
	u32 intr_descs = pbdma_get_intr_descs(g);

	if ((intr_descs & pbdma_intr_0) != 0U) {

		pbdma_intr_err = (unsigned long)pbdma_intr_0;
		for_each_set_bit(bit, &pbdma_intr_err, 32U) {
			nvgpu_err(g, "PBDMA intr %s Error",
				pbdma_intr_fault_type_desc[bit]);
		}

		ga10b_pbdma_dump_intr_0(g, pbdma_id, pbdma_intr_0);

		recover = true;
	}

	if ((pbdma_intr_0 & pbdma_intr_0_acquire_pending_f()) != 0U) {
		u32 val = nvgpu_readl(g, pbdma_acquire_r(pbdma_id));

		val &= ~pbdma_acquire_timeout_en_enable_f();
		nvgpu_writel(g, pbdma_acquire_r(pbdma_id), val);
		if (nvgpu_is_timeouts_enabled(g)) {
			recover = true;
			nvgpu_err(g, "semaphore acquire timeout!");

			gk20a_debug_dump(g);

			/*
			 * Note: the error_notifier can be overwritten if
			 * semaphore_timeout is triggered with pbcrc_pending
			 * interrupt below
			 */
			*error_notifier =
				NVGPU_ERR_NOTIFIER_GR_SEMAPHORE_TIMEOUT;
		}
	}

	if ((pbdma_intr_0 & pbdma_intr_0_pbentry_pending_f()) != 0U) {
		g->ops.pbdma.reset_header(g, pbdma_id);
		ga10b_pbdma_reset_method(g, pbdma_id, 0);
		recover = true;
	}

	if ((pbdma_intr_0 & pbdma_intr_0_method_pending_f()) != 0U) {
		ga10b_pbdma_reset_method(g, pbdma_id, 0);
		recover = true;
	}

	if ((pbdma_intr_0 & pbdma_intr_0_pbcrc_pending_f()) != 0U) {
		*error_notifier =
			NVGPU_ERR_NOTIFIER_PBDMA_PUSHBUFFER_CRC_MISMATCH;
		recover = true;
	}

	if ((pbdma_intr_0 & pbdma_intr_0_device_pending_f()) != 0U) {
		g->ops.pbdma.reset_header(g, pbdma_id);

		for (i = 0U; i < 4U; i++) {
			if (ga10b_pbdma_is_sw_method_subch(g,
					pbdma_id, i)) {
				ga10b_pbdma_reset_method(g,
						pbdma_id, i);
			}
		}
		recover = true;
	}

	return recover;
}


bool ga10b_pbdma_handle_intr_0(struct gk20a *g, u32 pbdma_id, u32 pbdma_intr_0,
			u32 *error_notifier)
{
	bool recover = ga10b_pbdma_handle_intr_0_legacy(g, pbdma_id,
			 pbdma_intr_0, error_notifier);

	if ((pbdma_intr_0 & pbdma_intr_0_eng_reset_pending_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr, "eng reset intr on pbdma id %d",
				 pbdma_id);
		recover = true;
	}
	report_pbdma_error(g, pbdma_id, pbdma_intr_0);
	return recover;
}

/*
 * Pbdma which encountered the ctxnotvalid interrupt will stall and
 * prevent the channel which was loaded at the time the interrupt fired
 * from being swapped out until the interrupt is cleared.
 * CTXNOTVALID pbdma interrupt indicates error conditions related
 * to the *_CTX_VALID fields for a channel.  The following
 * conditions trigger the interrupt:
 * * CTX_VALID bit for the targeted engine is FALSE
 * * At channel start/resume, all preemptible eng have CTX_VALID FALSE but:
 *       - CTX_RELOAD is set in CCSR_CHANNEL_STATUS,
 *       - PBDMA_TARGET_SHOULD_SEND_HOST_TSG_EVENT is TRUE, or
 *       - PBDMA_TARGET_NEEDS_HOST_TSG_EVENT is TRUE
 * The field is left NOT_PENDING and the interrupt is not raised if the PBDMA is
 * currently halted.  This allows SW to unblock the PBDMA and recover.
 * SW may read METHOD0, CHANNEL_STATUS and TARGET to determine whether the
 * interrupt was due to an engine method, CTX_RELOAD, SHOULD_SEND_HOST_TSG_EVENT
 * or NEEDS_HOST_TSG_EVENT.  If METHOD0 VALID is TRUE, lazy context creation
 * can be used or the TSG may be destroyed.
 * If METHOD0 VALID is FALSE, the error is likely a bug in SW, and the TSG
 * will have to be destroyed.
 */

bool ga10b_pbdma_handle_intr_1(struct gk20a *g, u32 pbdma_id, u32 pbdma_intr_1,
			u32 *error_notifier)
{
	bool recover = false;

	u32 pbdma_intr_1_current = nvgpu_readl(g, pbdma_intr_1_r(pbdma_id));

	(void)error_notifier;

	/* minimize race with the gpu clearing the pending interrupt */
	if ((pbdma_intr_1_current &
	     pbdma_intr_1_ctxnotvalid_pending_f()) == 0U) {
		pbdma_intr_1 &= ~pbdma_intr_1_ctxnotvalid_pending_f();
	}

	if (pbdma_intr_1 == 0U) {
		return recover;
	}

	recover = true;

	nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_HOST,
			GPU_HOST_PBDMA_HCE_ERROR);

	if ((pbdma_intr_1 & pbdma_intr_1_ctxnotvalid_pending_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr, "ctxnotvalid intr on pbdma id %d",
				 pbdma_id);
		nvgpu_err(g, "pbdma_intr_1(%d)= 0x%08x ",
				pbdma_id, pbdma_intr_1);
	} else{
		/*
		 * rest of the interrupts in _intr_1 are "host copy engine"
		 * related, which is not supported. For now just make them
		 * channel fatal.
		 */
		nvgpu_err(g, "hce err: pbdma_intr_1(%d):0x%08x",
			pbdma_id, pbdma_intr_1);
	}

	return recover;
}

u32 ga10b_pbdma_channel_fatal_0_intr_descs(void)
{
	/*
	 * These are data parsing, framing errors or others which can be
	 * recovered from with intervention... or just resetting the
	 * channel
	 */
	u32 channel_fatal_0_intr_descs =
		pbdma_intr_0_gpfifo_pending_f() |
		pbdma_intr_0_gpptr_pending_f() |
		pbdma_intr_0_gpentry_pending_f() |
		pbdma_intr_0_gpcrc_pending_f() |
		pbdma_intr_0_pbptr_pending_f() |
		pbdma_intr_0_pbentry_pending_f() |
		pbdma_intr_0_pbcrc_pending_f() |
		pbdma_intr_0_method_pending_f() |
		pbdma_intr_0_pbseg_pending_f() |
		pbdma_intr_0_eng_reset_pending_f() |
		pbdma_intr_0_semaphore_pending_f() |
		pbdma_intr_0_signature_pending_f();

	return channel_fatal_0_intr_descs;
}

u32 ga10b_pbdma_device_fatal_0_intr_descs(void)
{
	/*
	 * These are all errors which indicate something really wrong
	 * going on in the device.
	 */
	u32 fatal_device_0_intr_descs = pbdma_intr_0_pri_pending_f();

	return fatal_device_0_intr_descs;
}

u32 ga10b_pbdma_set_channel_info_chid(u32 chid)
{
	return pbdma_set_channel_info_chid_f(chid);
}

u32 ga10b_pbdma_set_intr_notify(u32 eng_intr_vector)
{
	return pbdma_intr_notify_vector_f(eng_intr_vector) |
		pbdma_intr_notify_ctrl_gsp_disable_f() |
		pbdma_intr_notify_ctrl_cpu_enable_f();
}

u32 ga10b_pbdma_get_fc_target(const struct nvgpu_device *dev)
{
	return (pbdma_target_engine_f(dev->rleng_id) |
			pbdma_target_eng_ctx_valid_true_f() |
			pbdma_target_ce_ctx_valid_true_f());
}

u32 ga10b_pbdma_get_mmu_fault_id(struct gk20a *g, u32 pbdma_id)
{
	u32 pbdma_cfg0 = nvgpu_readl(g, pbdma_cfg0_r(pbdma_id));

	return pbdma_cfg0_pbdma_fault_id_v(pbdma_cfg0);
}

u32 ga10b_pbdma_get_num_of_pbdmas(void)
{
	return pbdma_cfg0__size_1_v();
}
