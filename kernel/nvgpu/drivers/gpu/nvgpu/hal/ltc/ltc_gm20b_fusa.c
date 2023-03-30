/*
 * GM20B L2
 *
 * Copyright (c) 2014-2021 NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/fbp.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/static_analysis.h>

#include <nvgpu/hw/gm20b/hw_ltc_gm20b.h>

#include "ltc_gm20b.h"

static int gm20b_ltc_wait_for_clean(struct gk20a *g)
{
	struct nvgpu_timeout timeout;
	u32 ltc;
	u32 ltc_stride = nvgpu_get_litter_value(g, GPU_LIT_LTC_STRIDE);
	bool is_clean_pending_set = false;
	int err = 0;

	/* Wait on each LTC individually. */
	for (ltc = 0; ltc < g->ltc->ltc_count; ltc++) {
		u32 op_pending;

		/*
		 * Use 5ms - this should be sufficient time to flush the cache.
		 * On tegra, rough EMC BW available can be estimated as follows:
		 *
		 * Lowest reasonable EMC clock speed will be around 204MHz on
		 * t234 for display enabled boards and generally fixed to max
		 * for non-display boards (since they are generally plugged in).
		 *
		 * Thus, the available BW is 128B * 2 * 204MHz = ~52GB/s. Of that
		 * BW the GPU will likely get about half (display and overhead/
		 * utilization inefficiency eating the rest) so 26GB/s at
		 * worst. Assuming at most 1MB of GPU L2 cache (less for most
		 * chips) worst case is we take 1MB/26GB/s = 38us.
		 *
		 * So 5ms timeout here should be more than sufficient.
		 */
		nvgpu_timeout_init_cpu_timer(g, &timeout, 5);

		do {
			u32 cmgmt1 = nvgpu_safe_add_u32(
					ltc_ltc0_ltss_tstg_cmgmt1_r(),
					nvgpu_safe_mult_u32(ltc, ltc_stride));
			op_pending = gk20a_readl(g, cmgmt1);
			is_clean_pending_set = (op_pending &
				ltc_ltc0_ltss_tstg_cmgmt1_clean_pending_f()) != 0U;
			if (nvgpu_timeout_expired_msg(&timeout,
					"L2 flush timeout!") != 0) {
				err = -ETIMEDOUT;
			}
		} while (is_clean_pending_set && (err == 0));
	}
	return err;
}

static int gm20b_ltc_wait_for_invalidate(struct gk20a *g)
{
	u32 ltc;
	u32 ltc_stride = nvgpu_get_litter_value(g, GPU_LIT_LTC_STRIDE);
	struct nvgpu_timeout timeout;
	bool is_invalidate_pending_set = false;
	int err = 0;

	for (ltc = 0; ltc < g->ltc->ltc_count; ltc++) {
		u32 op_pending;

		/* Again, 5ms. */
		nvgpu_timeout_init_cpu_timer(g, &timeout, 5);

		do {
			u32 cmgmt0 = nvgpu_safe_add_u32(
					ltc_ltc0_ltss_tstg_cmgmt0_r(),
					nvgpu_safe_mult_u32(ltc, ltc_stride));
			op_pending = gk20a_readl(g, cmgmt0);
			is_invalidate_pending_set = (op_pending &
				ltc_ltc0_ltss_tstg_cmgmt0_invalidate_pending_f()) != 0U;
			if (nvgpu_timeout_expired_msg(&timeout,
					"L2 flush timeout!") != 0) {
				err = -ETIMEDOUT;
			}
		} while (is_invalidate_pending_set && (err == 0));
	}
	return err;
}


/*
 * Performs a full flush of the L2 cache.
 */
void gm20b_flush_ltc(struct gk20a *g)
{
	int err;

	/* Clean... */
	nvgpu_writel(g, ltc_ltcs_ltss_tstg_cmgmt1_r(),
		ltc_ltcs_ltss_tstg_cmgmt1_clean_pending_f() |
		ltc_ltcs_ltss_tstg_cmgmt1_max_cycles_between_cleans_3_f() |
		ltc_ltcs_ltss_tstg_cmgmt1_clean_wait_for_fb_to_pull_true_f() |
		ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_last_class_true_f() |
		ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_normal_class_true_f() |
		ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_first_class_true_f());

	/* Wait on each LTC individually. */
	err = gm20b_ltc_wait_for_clean(g);
	if (err != 0) {
		nvgpu_err(g, "gm20b_ltc_wait_for_clean failed");
	}

	/* And invalidate. */
	nvgpu_writel(g, ltc_ltcs_ltss_tstg_cmgmt0_r(),
	     ltc_ltcs_ltss_tstg_cmgmt0_invalidate_pending_f() |
	     ltc_ltcs_ltss_tstg_cmgmt0_max_cycles_between_invalidates_3_f() |
	     ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_last_class_true_f() |
	     ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_normal_class_true_f() |
	     ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_first_class_true_f());

	/* Wait on each LTC individually. */
	err = gm20b_ltc_wait_for_invalidate(g);
	if (err != 0) {
		nvgpu_err(g, "gm20b_ltc_wait_for_invalidate failed");
	}
}

#ifdef CONFIG_NVGPU_GRAPHICS
/*
 * Sets the ZBC color for the passed index.
 */
void gm20b_ltc_set_zbc_color_entry(struct gk20a *g,
					  u32 *color_l2,
					  u32 index)
{
	u32 i;

	nvgpu_writel(g, ltc_ltcs_ltss_dstg_zbc_index_r(),
		ltc_ltcs_ltss_dstg_zbc_index_address_f(index));

	for (i = 0;
	     i < ltc_ltcs_ltss_dstg_zbc_color_clear_value__size_1_v(); i++) {
		nvgpu_writel(g, ltc_ltcs_ltss_dstg_zbc_color_clear_value_r(i),
			color_l2[i]);
	}
}

/*
 * Sets the ZBC depth for the passed index.
 */
void gm20b_ltc_set_zbc_depth_entry(struct gk20a *g,
					  u32 depth_val,
					  u32 index)
{
	nvgpu_writel(g, ltc_ltcs_ltss_dstg_zbc_index_r(),
		ltc_ltcs_ltss_dstg_zbc_index_address_f(index));

	nvgpu_writel(g, ltc_ltcs_ltss_dstg_zbc_depth_clear_value_r(),
		depth_val);
}
#endif /* CONFIG_NVGPU_GRAPHICS */
