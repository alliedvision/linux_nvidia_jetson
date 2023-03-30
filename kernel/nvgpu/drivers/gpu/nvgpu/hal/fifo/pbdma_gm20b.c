/*
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

#include <nvgpu/log.h>
#include <nvgpu/log2.h>
#include <nvgpu/utils.h>
#include <nvgpu/io.h>
#include <nvgpu/bitops.h>
#include <nvgpu/bug.h>
#include <nvgpu/debug.h>
#include <nvgpu/error_notifier.h>
#include <nvgpu/nvhost.h>
#include <nvgpu/fifo.h>
#include <nvgpu/ptimer.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/pbdma_status.h>

#include <nvgpu/hw/gm20b/hw_pbdma_gm20b.h>

#include "pbdma_gm20b.h"

#define PBDMA_SUBDEVICE_ID  1U

void gm20b_pbdma_intr_enable(struct gk20a *g, bool enable)
{
	u32 pbdma_id = 0;
	u32 intr_stall;
	u32 num_pbdma = nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_PBDMA);

	if (!enable) {
		gm20b_pbdma_disable_and_clear_all_intr(g);
		return;
	}

	/* clear and enable pbdma interrupts */
	for (pbdma_id = 0; pbdma_id < num_pbdma; pbdma_id++) {

		gm20b_pbdma_clear_all_intr(g, pbdma_id);

		intr_stall = nvgpu_readl(g, pbdma_intr_stall_r(pbdma_id));
		intr_stall &= ~pbdma_intr_stall_lbreq_enabled_f();

		nvgpu_writel(g, pbdma_intr_stall_r(pbdma_id), intr_stall);
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

bool gm20b_pbdma_handle_intr_1(struct gk20a *g, u32 pbdma_id, u32 pbdma_intr_1,
			u32 *error_notifier)
{
	bool recover = true;

	(void)error_notifier;
	/*
	 * all of the interrupts in _intr_1 are "host copy engine"
	 * related, which is not supported. For now just make them
	 * channel fatal.
	 */
	nvgpu_err(g, "hce err: pbdma_intr_1(%d):0x%08x",
		pbdma_id, pbdma_intr_1);

	return recover;
}

u32 gm20b_pbdma_get_signature(struct gk20a *g)
{
	(void)g;
	return pbdma_signature_hw_valid_f() | pbdma_signature_sw_zero_f();
}

u32 gm20b_pbdma_channel_fatal_0_intr_descs(void)
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
		pbdma_intr_0_signature_pending_f();

	return channel_fatal_0_intr_descs;
}

void gm20b_pbdma_syncpoint_debug_dump(struct gk20a *g,
			     struct nvgpu_debug_context *o,
			     struct nvgpu_channel_dump_info *info)
{
#ifdef CONFIG_TEGRA_GK20A_NVHOST

	u32 syncpointa, syncpointb;

	syncpointa = info->inst.syncpointa;
	syncpointb = info->inst.syncpointb;

	if ((pbdma_syncpointb_op_v(syncpointb) == pbdma_syncpointb_op_wait_v())
		&& (pbdma_syncpointb_wait_switch_v(syncpointb) ==
			pbdma_syncpointb_wait_switch_en_v())) {
		gk20a_debug_output(o, "%s on syncpt %u (%s) val %u",
			info->hw_state.pending_acquire ? "Waiting" : "Waited",
			pbdma_syncpointb_syncpt_index_v(syncpointb),
			nvgpu_nvhost_syncpt_get_name(g->nvhost,
			(int) pbdma_syncpointb_syncpt_index_v(syncpointb)),
			pbdma_syncpointa_payload_v(syncpointa));
	}
#endif
}

void gm20b_pbdma_setup_hw(struct gk20a *g)
{
	u32 host_num_pbdma = nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_PBDMA);
	u32 i, timeout;

	for (i = 0U; i < host_num_pbdma; i++) {
		timeout = nvgpu_readl(g, pbdma_timeout_r(i));
		timeout = set_field(timeout, pbdma_timeout_period_m(),
				    pbdma_timeout_period_max_f());
		nvgpu_log_info(g, "pbdma_timeout reg val = 0x%08x", timeout);
		nvgpu_writel(g, pbdma_timeout_r(i), timeout);
	}
}

u32 gm20b_pbdma_get_fc_formats(void)
{
	return	(pbdma_formats_gp_fermi0_f() | pbdma_formats_pb_fermi1_f() |
			pbdma_formats_mp_fermi0_f());
}

u32 gm20b_pbdma_get_fc_pb_header(void)
{
	return (pbdma_pb_header_priv_user_f() |
		pbdma_pb_header_method_zero_f() |
		pbdma_pb_header_subchannel_zero_f() |
		pbdma_pb_header_level_main_f() |
		pbdma_pb_header_first_true_f() |
		pbdma_pb_header_type_inc_f());
}

void gm20b_pbdma_dump_status(struct gk20a *g, struct nvgpu_debug_context *o)
{
	u32 i, host_num_pbdma;
	struct nvgpu_pbdma_status_info pbdma_status;

	host_num_pbdma = nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_PBDMA);

	gk20a_debug_output(o, "PBDMA Status - chip %-5s", g->name);
	gk20a_debug_output(o, "-------------------------");

	for (i = 0; i < host_num_pbdma; i++) {
		g->ops.pbdma_status.read_pbdma_status_info(g, i,
			&pbdma_status);

		gk20a_debug_output(o, "pbdma %d:", i);
		gk20a_debug_output(o,
			"  id: %d - %-9s next_id: - %d %-9s | status: %s",
			pbdma_status.id,
			nvgpu_pbdma_status_is_id_type_tsg(&pbdma_status) ?
				   "[tsg]" : "[channel]",
			pbdma_status.next_id,
			nvgpu_pbdma_status_is_next_id_type_tsg(
				&pbdma_status) ?
				   "[tsg]" : "[channel]",
			nvgpu_fifo_decode_pbdma_ch_eng_status(
				pbdma_status.pbdma_channel_status));
		gk20a_debug_output(o,
			"  PBDMA_PUT %016llx PBDMA_GET %016llx",
			(u64)nvgpu_readl(g, pbdma_put_r(i)) +
			((u64)nvgpu_readl(g, pbdma_put_hi_r(i)) << 32ULL),
			(u64)nvgpu_readl(g, pbdma_get_r(i)) +
			((u64)nvgpu_readl(g, pbdma_get_hi_r(i)) << 32ULL));
		gk20a_debug_output(o,
			"  GP_PUT    %08x  GP_GET  %08x  "
			"FETCH   %08x HEADER %08x",
			nvgpu_readl(g, pbdma_gp_put_r(i)),
			nvgpu_readl(g, pbdma_gp_get_r(i)),
			nvgpu_readl(g, pbdma_gp_fetch_r(i)),
			nvgpu_readl(g, pbdma_pb_header_r(i)));
		gk20a_debug_output(o,
			"  HDR       %08x  SHADOW0 %08x  SHADOW1 %08x",
			nvgpu_readl(g, pbdma_hdr_shadow_r(i)),
			nvgpu_readl(g, pbdma_gp_shadow_0_r(i)),
			nvgpu_readl(g, pbdma_gp_shadow_1_r(i)));
	}

	gk20a_debug_output(o, " ");
}
