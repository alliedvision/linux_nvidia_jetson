/*
 * GA100 CBC
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

#include <nvgpu/gk20a.h>
#include <nvgpu/io.h>
#include <nvgpu/sizes.h>
#include <nvgpu/ltc.h>
#include <nvgpu/cbc.h>
#include <nvgpu/comptags.h>

#include "cbc_ga100.h"

#include <nvgpu/hw/ga100/hw_ltc_ga100.h>

#define SIZE_2K	(SZ_1K << 1U)
#define AMAP_DIVIDE_ROUNDING_BASE_VALUE		U32(SIZE_2K)
#define AMAP_SWIZZLE_ROUNDING_BASE_VALUE	U32(SZ_64K)

int ga100_cbc_alloc_comptags(struct gk20a *g, struct nvgpu_cbc *cbc)
{
	/*
	 * - Compbit backing store is a memory buffer to store compressed data
	 * corresponding to total compressible memory.
	 * - In GA10B, 1 ROP tile = 256B data is compressed to 1B compression
	 * bits. i.e. 1 GOB = 512B data is compressed to 2B compbits.
	 * - A comptagline is a collection of compbits corresponding to a
	 * compressible page size. In GA10B, compressible page size is 256KB.
	 *
	 * - GA10B has 2 LTCs with 4 slices each. A 256KB page is distributed
	 *   into 8 slices having 32KB (64 GOBs) data each.
	 * - Thus, each comptagline per slice contains compression status bits
	 * corresponding to 64 GOBs.
	 */
	u32 compbit_backing_size;

	/* max memory size (MB) to cover */
	u32 max_size = g->max_comptag_mem;

	u32 max_comptag_lines;

	u32 hw_max_comptag_lines =
		ltc_ltcs_ltss_cbc_ctrl3_clear_upper_bound_init_v();

	u32 gobs_per_comptagline_per_slice =
		ltc_ltcs_ltss_cbc_param2_gobs_per_comptagline_per_slice_v(
				nvgpu_readl(g, ltc_ltcs_ltss_cbc_param2_r()));

	u32 compstatus_per_gob = 2U;

	u32 cbc_param = nvgpu_readl(g, ltc_ltcs_ltss_cbc_param_r());

	u32 comptags_size =
		ltc_ltcs_ltss_cbc_param_bytes_per_comptagline_per_slice_v(
			cbc_param);

	u32 amap_divide_rounding = AMAP_DIVIDE_ROUNDING_BASE_VALUE <<
		ltc_ltcs_ltss_cbc_param_amap_divide_rounding_v(cbc_param);

	u32 amap_swizzle_rounding = AMAP_SWIZZLE_ROUNDING_BASE_VALUE <<
		ltc_ltcs_ltss_cbc_param_amap_swizzle_rounding_v(cbc_param);

	int err;

	nvgpu_log_fn(g, " ");

	/* Already initialized */
	if (cbc->max_comptag_lines != 0U) {
		return 0;
	}

	if (g->ops.fb.is_comptagline_mode_enabled(g)) {
		/*
		 * one tag line covers 256KB
		 * So, number of comptag lines = (max_size * SZ_1M) / SZ_256K
		 */
		max_comptag_lines = max_size << 2U;

		if (max_comptag_lines == 0U) {
			return 0;
		}

		if (max_comptag_lines > hw_max_comptag_lines) {
			max_comptag_lines = hw_max_comptag_lines;
		}
	} else {
		max_comptag_lines = hw_max_comptag_lines;
	}

	/* Memory required for comptag lines in all slices of all ltcs */
	compbit_backing_size =  nvgpu_safe_mult_u32(
		nvgpu_safe_mult_u32(max_comptag_lines,
			nvgpu_ltc_get_slices_per_ltc(g)),
		nvgpu_ltc_get_ltc_count(g));

	/* Total memory required for compstatus */
	compbit_backing_size =  nvgpu_safe_mult_u32(
		nvgpu_safe_mult_u32(compbit_backing_size,
			gobs_per_comptagline_per_slice), compstatus_per_gob);

	compbit_backing_size += nvgpu_ltc_get_ltc_count(g) *
					amap_divide_rounding;
	compbit_backing_size += amap_swizzle_rounding;

	/* aligned to 2KB * ltc_count */
	compbit_backing_size +=
		nvgpu_ltc_get_ltc_count(g) <<
			ltc_ltcs_ltss_cbc_base_alignment_shift_v();

	/* must be a multiple of 64KB */
	compbit_backing_size = round_up(compbit_backing_size, SZ_64K);

	err = nvgpu_cbc_alloc(g, compbit_backing_size, true);
	if (err != 0) {
		return err;
	}

	err = gk20a_comptag_allocator_init(g, &cbc->comp_tags,
						max_comptag_lines);
	if (err != 0) {
		return err;
	}

	cbc->max_comptag_lines = max_comptag_lines;
	cbc->comptags_per_cacheline =
		nvgpu_ltc_get_cacheline_size(g) / comptags_size;
	cbc->gobs_per_comptagline_per_slice = gobs_per_comptagline_per_slice;
	cbc->compbit_backing_size = compbit_backing_size;

	nvgpu_log(g, gpu_dbg_pte, "compbit backing store size : 0x%x",
		compbit_backing_size);
	nvgpu_log(g, gpu_dbg_pte, "max comptag lines: %d",
		max_comptag_lines);
	nvgpu_log(g, gpu_dbg_pte, "gobs_per_comptagline_per_slice: %d",
		cbc->gobs_per_comptagline_per_slice);

	return 0;
}
