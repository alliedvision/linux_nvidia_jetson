/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GR_INTR_TU104_H
#define NVGPU_GR_INTR_TU104_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_gr_config;

#define NVC5C0_SET_SHADER_EXCEPTIONS            0x1528U
#define NVC5C0_SET_SKEDCHECK                    0x23cU
#define NVC5C0_SET_SHADER_CUT_COLLECTOR         0x254U

#define NVC5C0_SET_SM_DISP_CTRL                             0x250U
#define NVC5C0_SET_SM_DISP_CTRL_COMPUTE_SHADER_QUAD_MASK    0x1U
#define NVC5C0_SET_SM_DISP_CTRL_COMPUTE_SHADER_QUAD_DISABLE 0U
#define NVC5C0_SET_SM_DISP_CTRL_COMPUTE_SHADER_QUAD_ENABLE  1U

#define NVC597_SET_SHADER_EXCEPTIONS            0x1528U
#define NVC597_SET_CIRCULAR_BUFFER_SIZE         0x1280U
#define NVC597_SET_ALPHA_CIRCULAR_BUFFER_SIZE   0x02dcU
#define NVC597_SET_GO_IDLE_TIMEOUT              0x022cU
#define NVC597_SET_TEX_IN_DBG                   0x10bcU
#define NVC597_SET_SKEDCHECK                    0x10c0U
#define NVC597_SET_BES_CROP_DEBUG3              0x10c4U
#define NVC597_SET_BES_CROP_DEBUG4              0x10b0U
#define NVC597_SET_SM_DISP_CTRL                 0x10c8U
#define NVC597_SET_SHADER_CUT_COLLECTOR         0x10d0U

int tu104_gr_intr_handle_sw_method(struct gk20a *g, u32 addr,
			      u32 class_num, u32 offset, u32 data);
void tu104_gr_intr_enable_gpc_exceptions(struct gk20a *g,
					 struct nvgpu_gr_config *gr_config);
void tu104_gr_intr_log_mme_exception(struct gk20a *g);

#endif /* NVGPU_GR_INTR_TU104_H */
