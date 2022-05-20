/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/io.h>
#include <nvgpu/gr/gr_falcon.h>

#include "gr_falcon_gm20b.h"
#include "gr_falcon_gv11b.h"
#include "gr_falcon_tu104.h"
#include "common/gr/gr_falcon_priv.h"

#include <nvgpu/hw/tu104/hw_gr_tu104.h>

int tu104_gr_falcon_ctrl_ctxsw(struct gk20a *g, u32 fecs_method,
		u32 data, u32 *ret_val)
{
#if defined(CONFIG_NVGPU_DEBUGGER) || defined(CONFIG_NVGPU_PROFILER)
	struct nvgpu_fecs_method_op op = {
		.mailbox = { .id = 0U, .data = 0U, .ret = NULL,
			     .clr = ~U32(0U), .ok = 0U, .fail = 0U},
		.method.data = 0U,
		.cond.ok = GR_IS_UCODE_OP_NOT_EQUAL,
		.cond.fail = GR_IS_UCODE_OP_SKIP,
	};
	u32 flags = 0U;
#endif
	int ret;

	nvgpu_log_info(g, "fecs method %d data 0x%x ret_val %p",
				fecs_method, data, ret_val);

	switch (fecs_method) {
#if defined(CONFIG_NVGPU_DEBUGGER) || defined(CONFIG_NVGPU_PROFILER)
	case NVGPU_GR_FALCON_METHOD_START_SMPC_GLOBAL_MODE:
		op.method.addr =
			gr_fecs_method_push_adr_smpc_global_mode_start_v();
		op.method.data = ~U32(0U);
		op.mailbox.id = 1U;
		op.mailbox.ok = gr_fecs_ctxsw_mailbox_value_pass_v();
		op.mailbox.fail = gr_fecs_ctxsw_mailbox_value_fail_v();
		op.cond.ok = GR_IS_UCODE_OP_EQUAL;
		op.cond.fail = GR_IS_UCODE_OP_EQUAL;
		flags |= NVGPU_GR_FALCON_SUBMIT_METHOD_F_SLEEP;

		ret = gm20b_gr_falcon_submit_fecs_method_op(g, op, flags);
		break;

	case NVGPU_GR_FALCON_METHOD_STOP_SMPC_GLOBAL_MODE:
		op.method.addr =
			gr_fecs_method_push_adr_smpc_global_mode_stop_v();
		op.method.data = ~U32(0U);
		op.mailbox.id = 1U;
		op.mailbox.ok = gr_fecs_ctxsw_mailbox_value_pass_v();
		op.mailbox.fail = gr_fecs_ctxsw_mailbox_value_fail_v();
		op.cond.ok = GR_IS_UCODE_OP_EQUAL;
		op.cond.fail = GR_IS_UCODE_OP_EQUAL;
		flags |= NVGPU_GR_FALCON_SUBMIT_METHOD_F_SLEEP;

		ret = gm20b_gr_falcon_submit_fecs_method_op(g, op, flags);
		break;
#endif

	default:
		ret = gv11b_gr_falcon_ctrl_ctxsw(g, fecs_method,
				data, ret_val);
		break;
	}
	return ret;
}
