/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/io.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/fifo.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/soc.h>
#include <nvgpu/device.h>

#include <nvgpu/hw/gv11b/hw_pbdma_gv11b.h>

#include "pbdma_gm20b.h"
#include "pbdma_gv11b.h"

static void report_pbdma_error(struct gk20a *g, u32 pbdma_id,
		u32 pbdma_intr_0)
{
	u32 err_type = GPU_HOST_INVALID_ERROR;

	/*
	 * Multiple errors have been grouped as part of a single
	 * top-level error.
	 */
	if ((pbdma_intr_0 & (
		pbdma_intr_0_memreq_pending_f() |
		pbdma_intr_0_memack_timeout_pending_f() |
		pbdma_intr_0_memdat_timeout_pending_f() |
		pbdma_intr_0_memflush_pending_f() |
		pbdma_intr_0_memop_pending_f() |
		pbdma_intr_0_lbconnect_pending_f() |
		pbdma_intr_0_lback_timeout_pending_f() |
		pbdma_intr_0_lbdat_timeout_pending_f())) != 0U) {
			err_type = GPU_HOST_PBDMA_TIMEOUT_ERROR;
			nvgpu_err (g, "Host pbdma timeout error");
	}
	if ((pbdma_intr_0 & (
		pbdma_intr_0_memack_extra_pending_f() |
		pbdma_intr_0_memdat_extra_pending_f() |
		pbdma_intr_0_lback_extra_pending_f() |
		pbdma_intr_0_lbdat_extra_pending_f())) != 0U) {
			err_type = GPU_HOST_PBDMA_EXTRA_ERROR;
			nvgpu_err (g, "Host pbdma extra error");
	}
	if ((pbdma_intr_0 & (
		pbdma_intr_0_gpfifo_pending_f() |
		pbdma_intr_0_gpptr_pending_f() |
		pbdma_intr_0_gpentry_pending_f() |
		pbdma_intr_0_gpcrc_pending_f() |
		pbdma_intr_0_pbptr_pending_f() |
		pbdma_intr_0_pbentry_pending_f() |
		pbdma_intr_0_pbcrc_pending_f())) != 0U) {
			err_type = GPU_HOST_PBDMA_GPFIFO_PB_ERROR;
			nvgpu_err (g, "Host pbdma gpfifo pb error");
	}
	if ((pbdma_intr_0 & (
		pbdma_intr_0_clear_faulted_error_pending_f() |
		pbdma_intr_0_method_pending_f() |
		pbdma_intr_0_methodcrc_pending_f() |
		pbdma_intr_0_device_pending_f() |
		pbdma_intr_0_eng_reset_pending_f() |
		pbdma_intr_0_semaphore_pending_f() |
		pbdma_intr_0_acquire_pending_f() |
		pbdma_intr_0_pri_pending_f() |
		pbdma_intr_0_pbseg_pending_f())) != 0U) {
			err_type = GPU_HOST_PBDMA_METHOD_ERROR;
			nvgpu_err (g, "Host pbdma method error");
	}
	if ((pbdma_intr_0 &
		pbdma_intr_0_signature_pending_f()) != 0U) {
			err_type = GPU_HOST_PBDMA_SIGNATURE_ERROR;
			nvgpu_err (g, "Host pbdma signature error");
	}
	if (err_type != GPU_HOST_INVALID_ERROR) {
		nvgpu_log_info(g, "pbdma id:%u", pbdma_id);
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_HOST, err_type);
	}
	return;
}

void gv11b_pbdma_intr_enable(struct gk20a *g, bool enable)
{
	u32 pbdma_id = 0;
	u32 intr_stall;
	u32 num_pbdma = nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_PBDMA);

	if (!enable) {
		gm20b_pbdma_disable_and_clear_all_intr(g);
		return;
	}

	/* clear and enable pbdma interrupt */
	for (pbdma_id = 0; pbdma_id < num_pbdma; pbdma_id++) {

		gm20b_pbdma_clear_all_intr(g, pbdma_id);

		intr_stall = nvgpu_readl(g, pbdma_intr_stall_r(pbdma_id));
		nvgpu_log_info(g, "pbdma id:%u, intr_en_0 0x%08x", pbdma_id,
			intr_stall);
		nvgpu_writel(g, pbdma_intr_en_0_r(pbdma_id), intr_stall);

		intr_stall = nvgpu_readl(g, pbdma_intr_stall_1_r(pbdma_id));
		/*
		 * For bug 2082123
		 * Mask the unused HCE_RE_ILLEGAL_OP bit from the interrupt.
		 */
		intr_stall &= ~pbdma_intr_stall_1_hce_illegal_op_enabled_f();
		nvgpu_log_info(g, "pbdma id:%u, intr_en_1 0x%08x", pbdma_id,
			intr_stall);
		nvgpu_writel(g, pbdma_intr_en_1_r(pbdma_id), intr_stall);
	}
}

bool gv11b_pbdma_handle_intr_0(struct gk20a *g, u32 pbdma_id, u32 pbdma_intr_0,
			u32 *error_notifier)
{
	bool recover = gm20b_pbdma_handle_intr_0(g, pbdma_id,
			 pbdma_intr_0, error_notifier);

	if ((pbdma_intr_0 & pbdma_intr_0_clear_faulted_error_pending_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr, "clear faulted error on pbdma id %d",
				 pbdma_id);
		gm20b_pbdma_reset_method(g, pbdma_id, 0);
		recover = true;
	}

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

bool gv11b_pbdma_handle_intr_1(struct gk20a *g, u32 pbdma_id, u32 pbdma_intr_1,
			u32 *error_notifier)
{
	bool recover = false;

	u32 pbdma_intr_1_current = gk20a_readl(g, pbdma_intr_1_r(pbdma_id));

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

u32 gv11b_pbdma_channel_fatal_0_intr_descs(void)
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
		pbdma_intr_0_methodcrc_pending_f() |
		pbdma_intr_0_pbseg_pending_f() |
		pbdma_intr_0_clear_faulted_error_pending_f() |
		pbdma_intr_0_eng_reset_pending_f() |
		pbdma_intr_0_semaphore_pending_f() |
		pbdma_intr_0_signature_pending_f();

	return channel_fatal_0_intr_descs;
}

void gv11b_pbdma_setup_hw(struct gk20a *g)
{
	u32 host_num_pbdma = nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_PBDMA);
	u32 i, timeout;

	for (i = 0U; i < host_num_pbdma; i++) {
		timeout = nvgpu_readl(g, pbdma_timeout_r(i));
		nvgpu_log_info(g, "pbdma_timeout reg val = 0x%08x",
						 timeout);
		if (nvgpu_platform_is_silicon(g)) {
			timeout = set_field(timeout, pbdma_timeout_period_m(),
					pbdma_timeout_period_max_f());
			nvgpu_log_info(g, "new pbdma_timeout reg val = 0x%08x",
						 timeout);
			nvgpu_writel(g, pbdma_timeout_r(i), timeout);
		}
	}
}

u32 gv11b_pbdma_get_fc_pb_header(void)
{
	return (pbdma_pb_header_method_zero_f() |
		pbdma_pb_header_subchannel_zero_f() |
		pbdma_pb_header_level_main_f() |
		pbdma_pb_header_first_true_f() |
		pbdma_pb_header_type_inc_f());
}

u32 gv11b_pbdma_get_fc_target(const struct nvgpu_device *dev)
{
	return (gm20b_pbdma_get_fc_target(dev) |
			pbdma_target_eng_ctx_valid_true_f() |
			pbdma_target_ce_ctx_valid_true_f());
}

u32 gv11b_pbdma_set_channel_info_veid(u32 subctx_id)
{
	return pbdma_set_channel_info_veid_f(subctx_id);
}

u32 gv11b_pbdma_config_userd_writeback_enable(u32 v)
{
	return set_field(v, pbdma_config_userd_writeback_m(),
			pbdma_config_userd_writeback_enable_f());
}

