/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_DBG_VGPU_H
#define NVGPU_DBG_VGPU_H

struct dbg_session_gk20a;
struct nvgpu_dbg_reg_op;
struct gk20a;
struct nvgpu_channel;

int vgpu_exec_regops(struct gk20a *g,
		struct nvgpu_tsg *tsg,
		struct nvgpu_dbg_reg_op *ops,
		u32 num_ops,
		u32 ctx_wr_count,
		u32 ctx_rd_count,
		u32 *flags);
int vgpu_dbg_set_powergate(struct dbg_session_gk20a *dbg_s,
			bool disable_powergate);
int vgpu_tsg_set_long_timeslice(struct nvgpu_tsg *tsg, u32 timeslice_us);

#endif /* NVGPU_DBG_VGPU_H */
