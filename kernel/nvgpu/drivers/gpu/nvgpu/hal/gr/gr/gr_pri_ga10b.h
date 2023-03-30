/*
 * GA10B Graphics Context Pri Register Addressing
 *
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
#ifndef GR_PRI_GA10B_H
#define GR_PRI_GA10B_H

#ifdef CONFIG_NVGPU_DEBUGGER

#include <nvgpu/static_analysis.h>

static inline u32 pri_rop_in_gpc_shared_addr(struct gk20a *g, u32 addr)
{
	u32 gpc_addr = pri_gpccs_addr_mask(g, addr);
	u32 gpc_shared_base = nvgpu_get_litter_value(g, GPU_LIT_GPC_SHARED_BASE);
	u32 rop_base = nvgpu_get_litter_value(g, GPU_LIT_ROP_IN_GPC_BASE);
	u32 rop_shared_base = nvgpu_get_litter_value(g, GPU_LIT_ROP_IN_GPC_SHARED_BASE);
	u32 rop_stride = nvgpu_get_litter_value(g, GPU_LIT_ROP_IN_GPC_STRIDE);

	return nvgpu_safe_add_u32(
			nvgpu_safe_add_u32(gpc_shared_base, rop_shared_base),
			nvgpu_safe_sub_u32(gpc_addr, rop_base) % rop_stride);
}

static inline bool pri_is_rop_in_gpc_addr_shared(struct gk20a *g, u32 addr)
{
	u32 rop_shared_base = nvgpu_get_litter_value(g, GPU_LIT_ROP_IN_GPC_SHARED_BASE);
	u32 rop_stride = nvgpu_get_litter_value(g, GPU_LIT_ROP_IN_GPC_STRIDE);

	return (addr >= rop_shared_base) &&
		(addr < nvgpu_safe_add_u32(rop_shared_base, rop_stride));
}

static inline bool pri_is_rop_in_gpc_addr(struct gk20a *g, u32 addr)
{
	u32 rop_base = nvgpu_get_litter_value(g, GPU_LIT_ROP_IN_GPC_BASE);
	u32 rop_shared_base = nvgpu_get_litter_value(g, GPU_LIT_ROP_IN_GPC_SHARED_BASE);
	u32 rop_stride = nvgpu_get_litter_value(g, GPU_LIT_ROP_IN_GPC_STRIDE);

	return (addr >= rop_base) &&
		(addr < nvgpu_safe_add_u32(rop_shared_base, rop_stride));
}

#endif /* CONFIG_NVGPU_DEBUGGER */
#endif /* GR_PRI_GA10B_H */
