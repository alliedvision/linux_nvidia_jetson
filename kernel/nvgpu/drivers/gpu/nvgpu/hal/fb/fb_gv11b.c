/*
 * GV11B FB
 *
 * Copyright (c) 2016-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/dma.h>
#include <nvgpu/log.h>
#include <nvgpu/enabled.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/barrier.h>
#include <nvgpu/bug.h>
#include <nvgpu/soc.h>
#include <nvgpu/ptimer.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/timers.h>
#include <nvgpu/fifo.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/ltc.h>
#include <nvgpu/rc.h>
#include <nvgpu/engines.h>

#include "fb_gm20b.h"
#include "fb_gp10b.h"
#include "fb_gv11b.h"

#include <nvgpu/hw/gv11b/hw_fb_gv11b.h>

#ifdef CONFIG_NVGPU_COMPRESSION
void gv11b_fb_cbc_configure(struct gk20a *g, struct nvgpu_cbc *cbc)
{
	u32 compbit_base_post_divide;
	u64 compbit_base_post_multiply64;
	u64 compbit_store_iova;
	u64 compbit_base_post_divide64;

#ifdef CONFIG_NVGPU_SIM
	if (nvgpu_is_enabled(g, NVGPU_IS_FMODEL)) {
		compbit_store_iova = nvgpu_mem_get_phys_addr(g,
						&cbc->compbit_store.mem);
	} else
#endif
	{
		compbit_store_iova = nvgpu_mem_get_addr(g,
						&cbc->compbit_store.mem);
	}
	/* must be aligned to 64 KB */
	compbit_store_iova = round_up(compbit_store_iova, (u64)SZ_64K);

	compbit_base_post_divide64 = compbit_store_iova >>
		fb_mmu_cbc_base_address_alignment_shift_v();

	do_div(compbit_base_post_divide64, nvgpu_ltc_get_ltc_count(g));
	compbit_base_post_divide = u64_lo32(compbit_base_post_divide64);

	compbit_base_post_multiply64 = ((u64)compbit_base_post_divide *
		nvgpu_ltc_get_ltc_count(g))
			<< fb_mmu_cbc_base_address_alignment_shift_v();

	if (compbit_base_post_multiply64 < compbit_store_iova) {
		compbit_base_post_divide++;
	}

	if (g->ops.cbc.fix_config != NULL) {
		compbit_base_post_divide =
			g->ops.cbc.fix_config(g, compbit_base_post_divide);
	}

	nvgpu_writel(g, fb_mmu_cbc_base_r(),
		fb_mmu_cbc_base_address_f(compbit_base_post_divide));

	nvgpu_log(g, gpu_dbg_info | gpu_dbg_map_v | gpu_dbg_pte,
		"compbit base.pa: 0x%x,%08x cbc_base:0x%08x\n",
		(u32)(compbit_store_iova >> 32),
		(u32)(compbit_store_iova & U32_MAX),
		compbit_base_post_divide);
	nvgpu_log(g, gpu_dbg_fn, "cbc base %x",
		nvgpu_readl(g, fb_mmu_cbc_base_r()));

	cbc->compbit_store.base_hw = compbit_base_post_divide;

}
#endif
