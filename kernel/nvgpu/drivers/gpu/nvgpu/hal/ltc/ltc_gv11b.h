/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef LTC_GV11B_H
#define LTC_GV11B_H

#include <nvgpu/types.h>
#include <nvgpu/nvgpu_err.h>

struct gk20a;
struct nvgpu_hw_err_inject_info;
struct nvgpu_hw_err_inject_info_desc;

int gv11b_lts_ecc_init(struct gk20a *g);

#ifdef CONFIG_NVGPU_GRAPHICS
void gv11b_ltc_set_zbc_stencil_entry(struct gk20a *g,
					  u32 stencil_depth,
					  u32 index);
#endif /* CONFIG_NVGPU_GRAPHICS */

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
void gv11b_ltc_init_fs_state(struct gk20a *g);
/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

#endif
