/*
 * Copyright (c) 2014-2020, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/static_analysis.h>
#include <nvgpu/rc.h>
#include <nvgpu/device.h>

#include <nvgpu/hw/gm20b/hw_pbdma_gm20b.h>

#include "pbdma_gm20b.h"

#define PBDMA_SUBDEVICE_ID  1U

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

static bool gm20b_pbdma_is_sw_method_subch(struct gk20a *g, u32 pbdma_id,
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

static void gm20b_pbdma_disable_all_intr(struct gk20a *g, u32 pbdma_id)
{
	nvgpu_writel(g, pbdma_intr_en_0_r(pbdma_id), 0U);
	nvgpu_writel(g, pbdma_intr_en_1_r(pbdma_id), 0U);
}

void gm20b_pbdma_clear_all_intr(struct gk20a *g, u32 pbdma_id)
{
	nvgpu_writel(g, pbdma_intr_0_r(pbdma_id), U32_MAX);
	nvgpu_writel(g, pbdma_intr_1_r(pbdma_id), U32_MAX);
}

void gm20b_pbdma_disable_and_clear_all_intr(struct gk20a *g)
{
	u32 pbdma_id = 0;
	u32 num_pbdma = nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_PBDMA);

	for (pbdma_id = 0; pbdma_id < num_pbdma; pbdma_id++) {
		gm20b_pbdma_disable_all_intr(g, pbdma_id);
		gm20b_pbdma_clear_all_intr(g, pbdma_id);
	}
}

static void gm20b_pbdma_dump_intr_0(struct gk20a *g, u32 pbdma_id,
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

static u32 pbdma_get_intr_descs(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;
	u32 intr_descs = (f->intr.pbdma.device_fatal_0 |
				f->intr.pbdma.channel_fatal_0 |
	      				f->intr.pbdma.restartable_0);

	return intr_descs;
}

bool gm20b_pbdma_handle_intr_0(struct gk20a *g, u32 pbdma_id,
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

		gm20b_pbdma_dump_intr_0(g, pbdma_id, pbdma_intr_0);

		recover = true;
	}

	if ((pbdma_intr_0 & pbdma_intr_0_acquire_pending_f()) != 0U) {
		u32 val = nvgpu_readl(g, pbdma_acquire_r(pbdma_id));

		val &= ~pbdma_acquire_timeout_en_enable_f();
		nvgpu_writel(g, pbdma_acquire_r(pbdma_id), val);
		if (nvgpu_is_timeouts_enabled(g)) {
			recover = true;
			nvgpu_err(g, "semaphore acquire timeout!");

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
		gm20b_pbdma_reset_method(g, pbdma_id, 0);
		recover = true;
	}

	if ((pbdma_intr_0 & pbdma_intr_0_method_pending_f()) != 0U) {
		gm20b_pbdma_reset_method(g, pbdma_id, 0);
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
			if (gm20b_pbdma_is_sw_method_subch(g,
					pbdma_id, i)) {
				gm20b_pbdma_reset_method(g,
						pbdma_id, i);
			}
		}
		recover = true;
	}

	return recover;
}

void gm20b_pbdma_reset_header(struct gk20a *g, u32 pbdma_id)
{
	nvgpu_writel(g, pbdma_pb_header_r(pbdma_id),
			pbdma_pb_header_first_true_f() |
			pbdma_pb_header_type_non_inc_f());
}

void gm20b_pbdma_reset_method(struct gk20a *g, u32 pbdma_id,
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

u32 gm20b_pbdma_acquire_val(u64 timeout)
{
	u32 val, exponent, mantissa;

	val = pbdma_acquire_retry_man_2_f() |
		pbdma_acquire_retry_exp_2_f();

	if (timeout == 0ULL) {
		return val;
	}

	/* set acquire timeout to 80% of channel wdt, and convert to ns */
	timeout = nvgpu_safe_mult_u64(timeout, (1000000UL * 80UL) / 100UL);
	do_div(timeout, 1024U); /* in unit of 1024ns */

	exponent = 0;
	while ((timeout > pbdma_acquire_timeout_man_max_v()) &&
		(exponent <= pbdma_acquire_timeout_exp_max_v())) {
		timeout >>= 1U;
		exponent++;
	}

	if (exponent > pbdma_acquire_timeout_exp_max_v()) {
		exponent = pbdma_acquire_timeout_exp_max_v();
		mantissa = pbdma_acquire_timeout_man_max_v();
	} else {
		mantissa = nvgpu_safe_cast_u64_to_u32(timeout);
	}

	val |= pbdma_acquire_timeout_exp_f(exponent) |
		pbdma_acquire_timeout_man_f(mantissa) |
		pbdma_acquire_timeout_en_enable_f();

	return val;
}

u32 gm20b_pbdma_read_data(struct gk20a *g, u32 pbdma_id)
{
	return nvgpu_readl(g, pbdma_hdr_shadow_r(pbdma_id));
}

void gm20b_pbdma_format_gpfifo_entry(struct gk20a *g,
		struct nvgpu_gpfifo_entry *gpfifo_entry,
		u64 pb_gpu_va, u32 method_size)
{
	gpfifo_entry->entry0 = u64_lo32(pb_gpu_va);
	gpfifo_entry->entry1 = u64_hi32(pb_gpu_va) |
					pbdma_gp_entry1_length_f(method_size);
}

u32 gm20b_pbdma_device_fatal_0_intr_descs(void)
{
	/*
	 * These are all errors which indicate something really wrong
	 * going on in the device.
	 */
	u32 fatal_device_0_intr_descs =
		pbdma_intr_0_memreq_pending_f() |
		pbdma_intr_0_memack_timeout_pending_f() |
		pbdma_intr_0_memack_extra_pending_f() |
		pbdma_intr_0_memdat_timeout_pending_f() |
		pbdma_intr_0_memdat_extra_pending_f() |
		pbdma_intr_0_memflush_pending_f() |
		pbdma_intr_0_memop_pending_f() |
		pbdma_intr_0_lbconnect_pending_f() |
		pbdma_intr_0_lback_timeout_pending_f() |
		pbdma_intr_0_lback_extra_pending_f() |
		pbdma_intr_0_lbdat_timeout_pending_f() |
		pbdma_intr_0_lbdat_extra_pending_f() |
		pbdma_intr_0_pri_pending_f();

	return fatal_device_0_intr_descs;
}

u32 gm20b_pbdma_restartable_0_intr_descs(void)
{
	/* Can be used for sw-methods, or represents a recoverable timeout. */
	u32 restartable_0_intr_descs =
		pbdma_intr_0_device_pending_f();

	return restartable_0_intr_descs;
}

void gm20b_pbdma_handle_intr(struct gk20a *g, u32 pbdma_id, bool recover)
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

u32 gm20b_pbdma_get_gp_base(u64 gpfifo_base)
{
	return pbdma_gp_base_offset_f(
		u64_lo32(gpfifo_base >> pbdma_gp_base_rsvd_s()));
}

u32 gm20b_pbdma_get_gp_base_hi(u64 gpfifo_base, u32 gpfifo_entry)
{
	return 	(pbdma_gp_base_hi_offset_f(u64_hi32(gpfifo_base)) |
		pbdma_gp_base_hi_limit2_f(
			nvgpu_safe_cast_u64_to_u32(ilog2(gpfifo_entry))));
}

u32 gm20b_pbdma_get_fc_subdevice(void)
{
	return (pbdma_subdevice_id_f(PBDMA_SUBDEVICE_ID) |
		pbdma_subdevice_status_active_f() |
		pbdma_subdevice_channel_dma_enable_f());
}

u32 gm20b_pbdma_get_fc_target(const struct nvgpu_device *dev)
{
	return pbdma_target_engine_sw_f();
}

u32 gm20b_pbdma_get_ctrl_hce_priv_mode_yes(void)
{
	return pbdma_hce_ctrl_hce_priv_mode_yes_f();
}

u32 gm20b_pbdma_get_userd_aperture_mask(struct gk20a *g,
		struct nvgpu_mem *mem)
{
	return	(nvgpu_aperture_mask(g, mem,
			pbdma_userd_target_sys_mem_ncoh_f(),
			pbdma_userd_target_sys_mem_coh_f(),
			pbdma_userd_target_vid_mem_f()));
}

u32 gm20b_pbdma_get_userd_addr(u32 addr_lo)
{
	return pbdma_userd_addr_f(addr_lo);
}

u32 gm20b_pbdma_get_userd_hi_addr(u32 addr_hi)
{
	return pbdma_userd_hi_addr_f(addr_hi);
}
