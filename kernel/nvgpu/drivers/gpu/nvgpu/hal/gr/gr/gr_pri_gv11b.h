/*
 * GV11B/GV100 Graphics Context Pri Register Addressing
 *
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef GR_PRI_GV11B_H
#define GR_PRI_GV11B_H

#ifdef CONFIG_NVGPU_DEBUGGER

/*
 * These convenience macros are generally for use in the management/modificaiton
 * of the context state store for gr/compute contexts.
 */

/* Broadcast PMM defines */
#define NV_PERF_PMMFBP_FBPGS_LTC             0x00250800U
#define NV_PERF_PMMFBP_FBPGS_ROP             0x00250A00U
#define NV_PERF_PMMGPC_GPCGS_GPCTPCA         0x00250000U
#define NV_PERF_PMMGPC_GPCGS_GPCTPCB         0x00250200U
#define NV_PERF_PMMGPC_GPCS                  0x00278000U
#define NV_PERF_PMMFBP_FBPS                  0x0027C000U

#define PRI_PMMGS_ADDR_WIDTH                 9U
#define PRI_PMMS_ADDR_WIDTH                  14U

/* Get the offset to be added to the chiplet base addr to get the unicast address */
#define PRI_PMMGS_OFFSET_MASK(addr)    ((addr) & (BIT32(PRI_PMMGS_ADDR_WIDTH) - 1U))
#define PRI_PMMGS_BASE_ADDR_MASK(addr) ((addr) & (~(BIT32(PRI_PMMGS_ADDR_WIDTH) - 1U)))

#define PRI_PMMS_ADDR_MASK(addr)      ((addr) & (BIT32(PRI_PMMS_ADDR_WIDTH) - 1U))
#define PRI_PMMS_BASE_ADDR_MASK(addr) ((addr) & (~(BIT32(PRI_PMMS_ADDR_WIDTH) - 1U)))

static inline u32 pri_sm_in_tpc_addr_mask(struct gk20a *g, u32 addr)
{
	return addr & (nvgpu_get_litter_value(g, GPU_LIT_SM_PRI_STRIDE) - 1U);
}

static inline bool pri_is_sm_addr_shared(struct gk20a *g, u32 addr)
{
	u32 sm_shared_base = nvgpu_get_litter_value(g, GPU_LIT_SM_SHARED_BASE);
	u32 sm_stride = nvgpu_get_litter_value(g, GPU_LIT_SM_PRI_STRIDE);

	return (addr >= sm_shared_base) &&
		(addr < sm_shared_base + sm_stride);
}

static inline u32 pri_sm_addr(struct gk20a *g, u32 addr, u32 gpc, u32 tpc,
			u32 sm)
{
	u32 gpc_base = nvgpu_get_litter_value(g, GPU_LIT_GPC_BASE);
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_base = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_BASE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 sm_unique_base = nvgpu_get_litter_value(g, GPU_LIT_SM_UNIQUE_BASE);
	u32 sm_stride = nvgpu_get_litter_value(g, GPU_LIT_SM_PRI_STRIDE);

	return gpc_base + (gpc * gpc_stride) +
		tpc_in_gpc_base + (tpc * tpc_in_gpc_stride) +
		sm_unique_base + (sm * sm_stride) +
		addr;
}

#endif /* CONFIG_NVGPU_DEBUGGER */
#endif /* GR_PRI_GV11B_H */
