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

#ifndef NVGPU_GR_INTR_GA100_H
#define NVGPU_GR_INTR_GA100_H

struct gk20a;
struct nvgpu_gr_config;

/* Copy required definitions from clc6c0.h class file */
#define NVC6C0_SET_CB_BASE			0x0220U
#define NVC6C0_SET_BES_CROP_DEBUG4		0x022cU
#define NVC6C0_SET_TEX_IN_DBG			0x0238U
#define NVC6C0_SET_SKEDCHECK			0x023cU
#define NVC697_SET_SHADER_EXCEPTIONS		0x1528U
#define NVC6C0_SET_SHADER_EXCEPTIONS		0x1528U
#define NVC697_SET_CIRCULAR_BUFFER_SIZE		0x1280U
#define NVC697_SET_ALPHA_CIRCULAR_BUFFER_SIZE	0x02dcU

/*
 * Hardware divides sw_method enum value by 2 before passing as "offset".
 * Left shift given offset by 2 to obtain sw_method enum value.
 */
#define NVGPU_GA100_SW_METHOD_SHIFT		2U

int ga100_gr_intr_handle_sw_method(struct gk20a *g, u32 addr,
			u32 class_num, u32 offset, u32 data);
void ga100_gr_intr_enable_exceptions(struct gk20a *g,
			struct nvgpu_gr_config *gr_config, bool enable);
bool ga100_gr_intr_handle_exceptions(struct gk20a *g, bool *is_gpc_exception);
void ga100_gr_intr_enable_gpc_exceptions(struct gk20a *g,
					 struct nvgpu_gr_config *gr_config);
u32 ga100_gr_intr_enable_mask(struct gk20a *g);
u32 ga100_gr_intr_read_pending_interrupts(struct gk20a *g,
					struct nvgpu_gr_intr_info *intr_info);

#endif /* NVGPU_GR_INTR_GA100_H */
