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

#ifndef NVGPU_GR_FALCON_GA10B_H
#define NVGPU_GR_FALCON_GA10B_H

#include <nvgpu/types.h>

struct gk20a;

u32 ga10b_gr_falcon_get_fecs_ctxsw_mailbox_size(void);
void ga10b_gr_falcon_fecs_ctxsw_clear_mailbox(struct gk20a *g,
						u32 reg_index, u32 clear_val);
void ga10b_gr_falcon_dump_stats(struct gk20a *g);
#ifdef CONFIG_NVGPU_GR_FALCON_NON_SECURE_BOOT
void ga10b_gr_falcon_gpccs_dmemc_write(struct gk20a *g, u32 port, u32 offs,
	u32 blk, u32 ainc);
void ga10b_gr_falcon_fecs_dmemc_write(struct gk20a *g, u32 reg_offset, u32 port,
	u32 offs, u32 blk, u32 ainc);
void ga10b_gr_falcon_gpccs_imemc_write(struct gk20a *g, u32 port, u32 offs,
	u32 blk, u32 ainc);
void ga10b_gr_falcon_load_fecs_imem(struct gk20a *g,
			const u32 *ucode_u32_data, u32 ucode_u32_size);
void ga10b_gr_falcon_fecs_imemc_write(struct gk20a *g, u32 port, u32 offs,
	u32 blk, u32 ainc);
#endif
#endif /* NVGPU_GR_FALCON_GA10B_H */
