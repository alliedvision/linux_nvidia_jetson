/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/gr/gr_falcon.h>

#include "gr_falcon_gp10b.h"
#include "gr_falcon_gm20b.h"
#include "common/gr/gr_falcon_priv.h"

#include <nvgpu/hw/gp10b/hw_gr_gp10b.h>

int gp10b_gr_falcon_init_ctx_state(struct gk20a *g,
		struct nvgpu_gr_falcon_query_sizes *sizes)
{
	int err;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	err = gm20b_gr_falcon_init_ctx_state(g, sizes);

#ifdef CONFIG_NVGPU_GFXP
	if (err != 0) {
		return err;
	}

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		err = g->ops.gr.falcon.ctrl_ctxsw(g,
			NVGPU_GR_FALCON_METHOD_PREEMPT_IMAGE_SIZE, 0U,
			&sizes->preempt_image_size);
		if (err != 0) {
			nvgpu_err(g, "query preempt image size failed");
			return err;
		}
	}

	nvgpu_log(g, gpu_dbg_gr, "Preempt image size = %u", sizes->preempt_image_size);
#endif

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");

	return err;
}

int gp10b_gr_falcon_ctrl_ctxsw(struct gk20a *g, u32 fecs_method,
						u32 data, u32 *ret_val)
{
#if defined(CONFIG_NVGPU_GFXP) || defined(CONFIG_NVGPU_CILP)
	struct nvgpu_fecs_method_op op = {
		.mailbox = { .id = 0U, .data = 0U, .ret = NULL,
			     .clr = ~U32(0U), .ok = 0U, .fail = 0U},
		.method.data = 0U,
		.cond.ok = GR_IS_UCODE_OP_NOT_EQUAL,
		.cond.fail = GR_IS_UCODE_OP_SKIP,
		};
#endif
	int ret;

	nvgpu_log_info(g, "fecs method %d data 0x%x ret_val %p",
				fecs_method, data, ret_val);

#if defined(CONFIG_NVGPU_GFXP) || defined(CONFIG_NVGPU_CILP)
	switch (fecs_method) {
#ifdef CONFIG_NVGPU_GFXP
	case NVGPU_GR_FALCON_METHOD_PREEMPT_IMAGE_SIZE:
		op.method.addr =
			gr_fecs_method_push_adr_discover_preemption_image_size_v();
		op.mailbox.ret = ret_val;
		ret = gm20b_gr_falcon_submit_fecs_method_op(g, op, 0U);
	break;
#endif

#ifdef CONFIG_NVGPU_CILP
	case NVGPU_GR_FALCON_METHOD_CONFIGURE_CTXSW_INTR:
		op.method.addr =
			gr_fecs_method_push_adr_configure_interrupt_completion_option_v();
		op.method.data = data;
		op.mailbox.id = 1U;
		op.mailbox.ok = gr_fecs_ctxsw_mailbox_value_pass_v();
		op.cond.ok = GR_IS_UCODE_OP_EQUAL;
		ret = gm20b_gr_falcon_submit_fecs_sideband_method_op(g, op);
	break;
#endif

	default:
#endif
		ret = gm20b_gr_falcon_ctrl_ctxsw(g, fecs_method, data, ret_val);
#if defined(CONFIG_NVGPU_GFXP) || defined(CONFIG_NVGPU_CILP)
	break;
	}
#endif
	return ret;

}
