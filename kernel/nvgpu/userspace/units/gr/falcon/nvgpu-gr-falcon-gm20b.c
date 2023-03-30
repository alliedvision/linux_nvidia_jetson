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

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/posix/io.h>
#include <nvgpu/posix/timers.h>
#include <nvgpu/posix/posix-fault-injection.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/gr_falcon.h>

#include <nvgpu/hw/gv11b/hw_gr_gv11b.h>

#include "common/gr/gr_priv.h"
#include "common/gr/gr_falcon_priv.h"

#include "hal/gr/falcon/gr_falcon_gm20b.h"

#include "../nvgpu-gr.h"
#include "nvgpu-gr-falcon-gm20b.h"

struct gr_falcon_gm20b_fecs_op {
	u32 id;
	u32 data;
	u32 ok;
	u32 fail;
	u32 cond_ok;
	u32 cond_fail;
	u32 result;
};

static void gr_falcon_fecs_dump_stats(struct gk20a *g)
{
	/* Do Nothing */
}

static int gr_falcon_ctrl_ctxsw_stub(struct gk20a *g, u32 fecs_method,
			u32 data, u32 *ret_val)
{
	return -EINVAL;
}

static int gr_falcon_gm20b_submit_fecs_mthd_op(struct unit_module *m,
				struct gk20a *g)
{
	int err, i;
	struct nvgpu_fecs_method_op op = {
		.mailbox = { .id = 4U, .data = 0U, .ret = NULL,
			     .clr = ~U32(0U), .ok = 0U, .fail = 0U},
		.method.data = 0U,
		.cond.ok = GR_IS_UCODE_OP_SKIP,
		.cond.fail = GR_IS_UCODE_OP_SKIP,
		};

	struct gr_falcon_gm20b_fecs_op fecs_op_stat[] = {
		[0] = {
			.id = 4U,
			.data = 0U,
			.ok = 0U,
			.fail = 0U,
			.cond_ok = GR_IS_UCODE_OP_SKIP,
			.cond_fail = GR_IS_UCODE_OP_SKIP,
			.result = 0U,
		      },
		[1] = {
			.id = 2U,
			.data = 1U,
			.ok = 0U,
			.fail = 2U,
			.cond_ok = GR_IS_UCODE_OP_SKIP,
			.cond_fail = GR_IS_UCODE_OP_LESSER_EQUAL,
			.result = 1U,
		      },
		[2] = {
			.id = 2U,
			.data = 1U,
			.ok = 2U,
			.fail = 0U,
			.cond_ok = GR_IS_UCODE_OP_LESSER_EQUAL,
			.cond_fail = 10,
			.result = 1U,
		      },
		[3] = {
			.id = 2U,
			.data = 1U,
			.ok = 2U,
			.fail = 1U,
			.cond_ok = GR_IS_UCODE_OP_LESSER,
			.cond_fail = GR_IS_UCODE_OP_EQUAL,
			.result = 1U,
		      },
		[4] = {
			.id = 2U,
			.data = 1U,
			.ok = 0U,
			.fail = 1U,
			.cond_ok = GR_IS_UCODE_OP_LESSER_EQUAL,
			.cond_fail = GR_IS_UCODE_OP_AND,
			.result = 1U,
		      },
		[5] = {
			.id = 2U,
			.data = 1U,
			.ok = 0U,
			.fail = 2U,
			.cond_ok = GR_IS_UCODE_OP_LESSER,
			.cond_fail = GR_IS_UCODE_OP_LESSER,
			.result = 1U,
		      },
		[6] = {
			.id = 2U,
			.data = 1U,
			.ok = 1U,
			.fail = 2U,
			.cond_ok = GR_IS_UCODE_OP_NOT_EQUAL,
			.cond_fail = GR_IS_UCODE_OP_NOT_EQUAL,
			.result = 1U,
		      },
		[7] = {
			.id = 2U,
			.data = 1U,
			.ok = 1U,
			.fail = 2U,
			.cond_ok = GR_IS_UCODE_OP_EQUAL,
			.cond_fail = GR_IS_UCODE_OP_EQUAL,
			.result = 0U,
		      },
	};
	int arry_cnt = sizeof(fecs_op_stat)/
			sizeof(struct gr_falcon_gm20b_fecs_op);

	g->ops.gr.falcon.dump_stats = gr_falcon_fecs_dump_stats;
	for (i = 0; i < arry_cnt; i++) {
		op.mailbox.ok = fecs_op_stat[i].ok;
		op.mailbox.fail = fecs_op_stat[i].fail;
		op.mailbox.id = fecs_op_stat[i].id;
		op.mailbox.data = fecs_op_stat[i].data;
		op.cond.ok = fecs_op_stat[i].cond_ok;
		op.cond.fail = fecs_op_stat[i].cond_fail;

		err = gm20b_gr_falcon_submit_fecs_method_op(g, op, false);
		if ((fecs_op_stat[i].result == 0) && err) {
			unit_return_fail(m, "submit_fecs_method_op failed\n");
		} else if (fecs_op_stat[i].result && (err == 0)){
			unit_return_fail(m, "submit_fecs_method_op failed\n");
		}
	}

	return UNIT_SUCCESS;
}

static int gr_falcon_timer_init_error(struct unit_module *m,
					struct gk20a *g)
{
	int err, i;
	u32 fecs_imem = 0, gpccs_imem = 0;
	int (*gr_falcon_ctrl_ctxsw_local)(struct gk20a *g,
			u32 fecs_method,
			u32 data, u32 *ret_val);

	for (i = 0; i < 2; i++) {
		switch (i) {
		case 0:
			fecs_imem = gr_fecs_dmactl_imem_scrubbing_m();
			break;
		case 1:
			fecs_imem = 0;
			gpccs_imem = gr_gpccs_dmactl_imem_scrubbing_m();
			break;
		}
		nvgpu_posix_io_writel_reg_space(g, gr_fecs_dmactl_r(),
							fecs_imem);
		nvgpu_posix_io_writel_reg_space(g, gr_gpccs_dmactl_r(),
							gpccs_imem);
		err = g->ops.gr.falcon.wait_mem_scrubbing(g);
		if (err == 0) {
			unit_return_fail(m,
			 "gr_falcon_wait_mem_scrubbing case %d failed\n", i);
		}
	}

	/* branch coverage check */
	nvgpu_set_enabled(g, NVGPU_GR_USE_DMA_FOR_FW_BOOTSTRAP, false);
	nvgpu_set_enabled(g, NVGPU_SEC_SECUREGPCCS, false);
	err = g->ops.gr.falcon.wait_ctxsw_ready(g);
	if (err != 0) {
		unit_return_fail(m,
			"gr_falcon_wait_ctxsw_ready failed\n");
	}

	nvgpu_set_enabled(g, NVGPU_GR_USE_DMA_FOR_FW_BOOTSTRAP, true);
	gr_falcon_ctrl_ctxsw_local = g->ops.gr.falcon.ctrl_ctxsw;
	g->ops.gr.falcon.ctrl_ctxsw = gr_falcon_ctrl_ctxsw_stub;
	err = g->ops.gr.falcon.wait_ctxsw_ready(g);
	if (err == 0) {
		unit_return_fail(m,
			"gr_falcon_wait_ctxsw_ready failed\n");
	}
	g->ops.gr.falcon.ctrl_ctxsw = gr_falcon_ctrl_ctxsw_local;

	err = g->ops.gr.falcon.wait_ctxsw_ready(g);
	if (err != 0) {
		unit_return_fail(m,
			"gr_falcon_wait_ctxsw_ready failed\n");
	}
	nvgpu_set_enabled(g, NVGPU_SEC_SECUREGPCCS, true);
	return UNIT_SUCCESS;
}

int test_gr_falcon_gm20b_ctrl_ctxsw(struct unit_module *m,
				struct gk20a *g, void *args)
{
	int err = 0;
	u32 data = 0;

	err = gm20b_gr_falcon_ctrl_ctxsw(g,
		NVGPU_GR_FALCON_METHOD_SET_WATCHDOG_TIMEOUT, data, NULL);
	if (err) {
		unit_return_fail(m,
			"falcon_gm20b_ctrl_ctxsw watchdog timeout failed\n");
	}

	err = g->ops.gr.falcon.ctrl_ctxsw(g,
			NVGPU_GR_FALCON_METHOD_GOLDEN_IMAGE_SAVE, data, NULL);
	if (err) {
		unit_return_fail(m, "falcon_gm20b_ctrl_ctxsw failed\n");
	}

	/* Invalid Method */
	err = g->ops.gr.falcon.ctrl_ctxsw(g, 0, data, NULL);
	if (err) {
		unit_return_fail(m, "falcon_gm20b_ctrl_ctxsw failed\n");
	}

	err = gr_falcon_timer_init_error(m, g);
	if (err) {
		unit_return_fail(m, "gr_falcon_timer_init_error failed\n");
	}

	err = gr_falcon_gm20b_submit_fecs_mthd_op(m, g);
	if (err) {
		unit_return_fail(m, "gr_falcon_gm20b_fecs_mthd_op failed\n");
	}

	return UNIT_SUCCESS;
}
