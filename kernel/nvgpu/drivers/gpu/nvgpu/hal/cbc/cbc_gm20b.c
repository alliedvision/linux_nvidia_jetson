/*
 * GM20B CBC
 *
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/trace.h>
#include <nvgpu/timers.h>
#include <nvgpu/enabled.h>
#include <nvgpu/bug.h>
#include <nvgpu/ltc.h>
#include <nvgpu/cbc.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/gk20a.h>

#include <nvgpu/hw/gm20b/hw_ltc_gm20b.h>

#include "cbc_gm20b.h"

int gm20b_cbc_alloc_comptags(struct gk20a *g, struct nvgpu_cbc *cbc)
{
	/* max memory size (MB) to cover */
	u32 max_size = g->max_comptag_mem;
	/* one tag line covers 128KB */
	u32 max_comptag_lines = max_size << 3U;

	u32 hw_max_comptag_lines =
		ltc_ltcs_ltss_cbc_ctrl3_clear_upper_bound_init_v();

	u32 cbc_param =
		gk20a_readl(g, ltc_ltcs_ltss_cbc_param_r());
	u32 comptags_per_cacheline =
		ltc_ltcs_ltss_cbc_param_comptags_per_cache_line_v(cbc_param);

	u32 compbit_backing_size;

	int err;

	nvgpu_log_fn(g, " ");

	if (max_comptag_lines == 0U) {
		return 0;
	}

	/* Already initialized */
	if (cbc->max_comptag_lines != 0U) {
		return 0;
	}

	if (max_comptag_lines > hw_max_comptag_lines) {
		max_comptag_lines = hw_max_comptag_lines;
	}

	compbit_backing_size =
		DIV_ROUND_UP(max_comptag_lines, comptags_per_cacheline) *
			(nvgpu_ltc_get_ltc_count(g) *
			nvgpu_ltc_get_slices_per_ltc(g) *
			nvgpu_ltc_get_cacheline_size(g));

	/* aligned to 2KB * ltc_count */
	compbit_backing_size +=
		nvgpu_ltc_get_ltc_count(g) <<
			ltc_ltcs_ltss_cbc_base_alignment_shift_v();

	/* must be a multiple of 64KB */
	compbit_backing_size = round_up(compbit_backing_size,
					U32(64) * U32(1024));

	max_comptag_lines =
		(compbit_backing_size * comptags_per_cacheline) /
			(nvgpu_ltc_get_ltc_count(g) *
			nvgpu_ltc_get_slices_per_ltc(g) *
			nvgpu_ltc_get_cacheline_size(g));

	if (max_comptag_lines > hw_max_comptag_lines) {
		max_comptag_lines = hw_max_comptag_lines;
	}

	nvgpu_log_info(g, "compbit backing store size : %d",
		compbit_backing_size);
	nvgpu_log_info(g, "max comptag lines : %d",
		max_comptag_lines);

	err = nvgpu_cbc_alloc(g, compbit_backing_size, false);
	if (err != 0) {
		return err;
	}

	err = gk20a_comptag_allocator_init(g, &cbc->comp_tags,
						max_comptag_lines);
	if (err != 0) {
		return err;
	}

	cbc->max_comptag_lines = max_comptag_lines;
	cbc->comptags_per_cacheline = comptags_per_cacheline;
	cbc->compbit_backing_size = compbit_backing_size;

	return 0;
}

int gm20b_cbc_ctrl(struct gk20a *g, enum nvgpu_cbc_op op,
		       u32 min, u32 max)
{
	struct nvgpu_timeout timeout;
	int err = 0;
	u32 ltc, slice, ctrl1, val, hw_op = 0U;
	u32 slices_per_ltc = ltc_ltcs_ltss_cbc_param_slices_per_ltc_v(
				gk20a_readl(g, ltc_ltcs_ltss_cbc_param_r()));
	u32 ltc_stride = nvgpu_get_litter_value(g, GPU_LIT_LTC_STRIDE);
	u32 lts_stride = nvgpu_get_litter_value(g, GPU_LIT_LTS_STRIDE);
	const u32 max_lines = 16384U;

	nvgpu_log_fn(g, " ");

#ifdef CONFIG_NVGPU_TRACE
	trace_gk20a_ltc_cbc_ctrl_start(g->name, op, min, max);
#endif

	if (g->cbc->compbit_store.mem.size == 0ULL) {
		return 0;
	}

	while (true) {
		const u32 iter_max = min(min + max_lines - 1U, max);
		bool full_cache_op = true;

		nvgpu_mutex_acquire(&g->mm.l2_op_lock);

		nvgpu_log_info(g, "clearing CBC lines %u..%u", min, iter_max);

		if (op == nvgpu_cbc_op_clear) {
			gk20a_writel(
				g, ltc_ltcs_ltss_cbc_ctrl2_r(),
				ltc_ltcs_ltss_cbc_ctrl2_clear_lower_bound_f(
					min));
			gk20a_writel(
				g, ltc_ltcs_ltss_cbc_ctrl3_r(),
				ltc_ltcs_ltss_cbc_ctrl3_clear_upper_bound_f(
					iter_max));
			hw_op = ltc_ltcs_ltss_cbc_ctrl1_clear_active_f();
			full_cache_op = false;
		} else if (op == nvgpu_cbc_op_clean) {
			/* this is full-cache op */
			hw_op = ltc_ltcs_ltss_cbc_ctrl1_clean_active_f();
		} else if (op == nvgpu_cbc_op_invalidate) {
			/* this is full-cache op */
			hw_op = ltc_ltcs_ltss_cbc_ctrl1_invalidate_active_f();
		} else {
			nvgpu_err(g, "Unknown op: %u", (unsigned)op);
			err = -EINVAL;
			goto out;
		}
		gk20a_writel(g, ltc_ltcs_ltss_cbc_ctrl1_r(),
			     gk20a_readl(g,
					 ltc_ltcs_ltss_cbc_ctrl1_r()) | hw_op);

		for (ltc = 0; ltc < nvgpu_ltc_get_ltc_count(g); ltc++) {
			for (slice = 0; slice < slices_per_ltc; slice++) {

				ctrl1 = ltc_ltc0_lts0_cbc_ctrl1_r() +
					ltc * ltc_stride + slice * lts_stride;

				nvgpu_timeout_init_retry(g, &timeout, 2000);
				do {
					val = gk20a_readl(g, ctrl1);
					if ((val & hw_op) == 0U) {
						break;
					}
					nvgpu_udelay(5);
				} while (nvgpu_timeout_expired(&timeout) == 0);

				if (nvgpu_timeout_peek_expired(&timeout)) {
					nvgpu_err(g, "comp tag clear timeout");
					err = -EBUSY;
					goto out;
				}
			}
		}

		/* are we done? */
		if (full_cache_op || iter_max == max) {
			break;
		}

		/* note: iter_max is inclusive upper bound */
		min = iter_max + 1U;

		/* give a chance for higher-priority threads to progress */
		nvgpu_mutex_release(&g->mm.l2_op_lock);
	}
out:
#ifdef CONFIG_NVGPU_TRACE
	trace_gk20a_ltc_cbc_ctrl_done(g->name);
#endif
	nvgpu_mutex_release(&g->mm.l2_op_lock);
	return err;
}

u32 gm20b_cbc_fix_config(struct gk20a *g, int base)
{
	u32 val = gk20a_readl(g, ltc_ltcs_ltss_cbc_num_active_ltcs_r());

	if (val == 2U) {
		return (u32)(base * 2);
	} else if (val != 1U) {
		nvgpu_err(g, "Invalid number of active ltcs: %08x", val);
	}

	return (u32)base;
}


void gm20b_cbc_init(struct gk20a *g, struct nvgpu_cbc *cbc, bool is_resume)
{
	u32 compbit_base_post_divide;
	u64 compbit_base_post_multiply64;
	u64 compbit_store_iova;
	u64 compbit_base_post_divide64;
	enum nvgpu_cbc_op cbc_op = is_resume ? nvgpu_cbc_op_invalidate
					     : nvgpu_cbc_op_clear;

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

	compbit_base_post_divide64 = compbit_store_iova >>
		ltc_ltcs_ltss_cbc_base_alignment_shift_v();

	do_div(compbit_base_post_divide64, nvgpu_ltc_get_ltc_count(g));
	compbit_base_post_divide = u64_lo32(compbit_base_post_divide64);

	compbit_base_post_multiply64 = ((u64)compbit_base_post_divide *
		nvgpu_ltc_get_ltc_count(g)) <<
			ltc_ltcs_ltss_cbc_base_alignment_shift_v();

	if (compbit_base_post_multiply64 < compbit_store_iova) {
		compbit_base_post_divide++;
	}

	/* Bug 1477079 indicates sw adjustment on the posted divided base. */
	if (g->ops.cbc.fix_config != NULL) {
		compbit_base_post_divide =
			g->ops.cbc.fix_config(g, (int)compbit_base_post_divide);
	}

	gk20a_writel(g, ltc_ltcs_ltss_cbc_base_r(),
		compbit_base_post_divide);

	nvgpu_log(g, gpu_dbg_info | gpu_dbg_map_v | gpu_dbg_pte,
		   "compbit base.pa: 0x%x,%08x cbc_base:0x%08x\n",
		   (u32)(compbit_store_iova >> 32),
		   (u32)(compbit_store_iova & 0xffffffffU),
		   compbit_base_post_divide);

	cbc->compbit_store.base_hw = compbit_base_post_divide;

	g->ops.cbc.ctrl(g, cbc_op, 0, cbc->max_comptag_lines - 1U);

}
