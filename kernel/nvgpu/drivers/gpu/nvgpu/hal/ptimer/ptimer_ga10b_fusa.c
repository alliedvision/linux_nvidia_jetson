// SPDX-License-Identifier: GPL-2.0-or-later
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
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_err.h>

#include "ptimer_ga10b.h"

#include <nvgpu/hw/ga10b/hw_timer_ga10b.h>

void ga10b_ptimer_isr(struct gk20a *g)
{
	u32 save0, save1, fecs_errcode = 0;
	u32 error_addr;

	save0 = nvgpu_readl(g, timer_pri_timeout_save_0_r());
	if (timer_pri_timeout_save_0_fecs_tgt_v(save0) != 0U) {
		/*
		 * write & addr fields in timeout_save0
		 * might not be reliable
		 */
		fecs_errcode = nvgpu_readl(g,
				timer_pri_timeout_fecs_errcode_r());
	}

	save1 = nvgpu_readl(g, timer_pri_timeout_save_1_r());
	error_addr = timer_pri_timeout_save_0_addr_v(save0) << 2;
	nvgpu_err(g, "PRI timeout: ADR 0x%08x "
		"%s  DATA 0x%08x",
		error_addr,
		(timer_pri_timeout_save_0_write_v(save0) != 0U) ?
		"WRITE" : "READ", save1);

	nvgpu_writel(g, timer_pri_timeout_save_0_r(), 0);
	nvgpu_writel(g, timer_pri_timeout_save_1_r(), 0);

	if (fecs_errcode != 0U) {
		nvgpu_err(g, "FECS_ERRCODE 0x%08x", fecs_errcode);
		if (g->ops.priv_ring.decode_error_code != NULL) {
			g->ops.priv_ring.decode_error_code(g,
						fecs_errcode);
		}
	}

	nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PRI,
			GPU_PRI_TIMEOUT_ERROR);
}
