/*
 * GM20B GPC MMU
 *
 * Copyright (c) 2014-2018, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/sizes.h>

#include "gk20a/gk20a.h"
#include "gk20a/fb_gk20a.h"
#include "gm20b/fb_gm20b.h"

#include <nvgpu/hw/gm20b/hw_fb_gm20b.h>
#include <nvgpu/hw/gm20b/hw_top_gm20b.h>
#include <nvgpu/hw/gm20b/hw_gmmu_gm20b.h>
#include <nvgpu/hw/gm20b/hw_gr_gm20b.h>

#define VPR_INFO_FETCH_WAIT	(5)
#define WPR_INFO_ADDR_ALIGNMENT 0x0000000c

void fb_gm20b_init_fs_state(struct gk20a *g)
{
	nvgpu_log_info(g, "initialize gm20b fb");

	gk20a_writel(g, fb_fbhub_num_active_ltcs_r(),
			g->ltc_count);
}

void gm20b_fb_set_mmu_page_size(struct gk20a *g)
{
	/* set large page size in fb */
	u32 fb_mmu_ctrl = gk20a_readl(g, fb_mmu_ctrl_r());
	fb_mmu_ctrl |= fb_mmu_ctrl_use_pdb_big_page_size_true_f();
	gk20a_writel(g, fb_mmu_ctrl_r(), fb_mmu_ctrl);
}

bool gm20b_fb_set_use_full_comp_tag_line(struct gk20a *g)
{
	/* set large page size in fb */
	u32 fb_mmu_ctrl = gk20a_readl(g, fb_mmu_ctrl_r());
	fb_mmu_ctrl |= fb_mmu_ctrl_use_full_comp_tag_line_true_f();
	gk20a_writel(g, fb_mmu_ctrl_r(), fb_mmu_ctrl);

	return true;
}

unsigned int gm20b_fb_compression_page_size(struct gk20a *g)
{
	return SZ_128K;
}

unsigned int gm20b_fb_compressible_page_size(struct gk20a *g)
{
	return SZ_64K;
}

u32 gm20b_fb_compression_align_mask(struct gk20a *g)
{
	return SZ_64K - 1;
}

void gm20b_fb_dump_vpr_wpr_info(struct gk20a *g)
{
	u32 val;

	/* print vpr and wpr info */
	val = gk20a_readl(g, fb_mmu_vpr_info_r());
	val &= ~0x3;
	val |= fb_mmu_vpr_info_index_addr_lo_v();
	gk20a_writel(g, fb_mmu_vpr_info_r(), val);
	nvgpu_err(g, "VPR: %08x %08x %08x %08x",
		gk20a_readl(g, fb_mmu_vpr_info_r()),
		gk20a_readl(g, fb_mmu_vpr_info_r()),
		gk20a_readl(g, fb_mmu_vpr_info_r()),
		gk20a_readl(g, fb_mmu_vpr_info_r()));

	val = gk20a_readl(g, fb_mmu_wpr_info_r());
	val &= ~0xf;
	val |= (fb_mmu_wpr_info_index_allow_read_v());
	gk20a_writel(g, fb_mmu_wpr_info_r(), val);
	nvgpu_err(g, "WPR: %08x %08x %08x %08x %08x %08x",
		gk20a_readl(g, fb_mmu_wpr_info_r()),
		gk20a_readl(g, fb_mmu_wpr_info_r()),
		gk20a_readl(g, fb_mmu_wpr_info_r()),
		gk20a_readl(g, fb_mmu_wpr_info_r()),
		gk20a_readl(g, fb_mmu_wpr_info_r()),
		gk20a_readl(g, fb_mmu_wpr_info_r()));

}

static int gm20b_fb_vpr_info_fetch_wait(struct gk20a *g,
					    unsigned int msec)
{
	struct nvgpu_timeout timeout;

	nvgpu_timeout_init(g, &timeout, msec, NVGPU_TIMER_CPU_TIMER);

	do {
		u32 val;

		val = gk20a_readl(g, fb_mmu_vpr_info_r());
		if (fb_mmu_vpr_info_fetch_v(val) ==
		    fb_mmu_vpr_info_fetch_false_v())
			return 0;

	} while (!nvgpu_timeout_expired(&timeout));

	return -ETIMEDOUT;
}

int gm20b_fb_vpr_info_fetch(struct gk20a *g)
{
	if (gm20b_fb_vpr_info_fetch_wait(g, VPR_INFO_FETCH_WAIT)) {
		return -ETIMEDOUT;
	}

	gk20a_writel(g, fb_mmu_vpr_info_r(),
			fb_mmu_vpr_info_fetch_true_v());

	return gm20b_fb_vpr_info_fetch_wait(g, VPR_INFO_FETCH_WAIT);
}

void gm20b_fb_read_wpr_info(struct gk20a *g, struct wpr_carveout_info *inf)
{
	u32 val = 0;
	u64 wpr_start = 0;
	u64 wpr_end = 0;

	val = gk20a_readl(g, fb_mmu_wpr_info_r());
	val &= ~0xF;
	val |= fb_mmu_wpr_info_index_wpr1_addr_lo_v();
	gk20a_writel(g, fb_mmu_wpr_info_r(), val);

	val = gk20a_readl(g, fb_mmu_wpr_info_r()) >> 0x4;
	wpr_start = hi32_lo32_to_u64(
			(val >> (32 - WPR_INFO_ADDR_ALIGNMENT)),
			(val << WPR_INFO_ADDR_ALIGNMENT));

	val = gk20a_readl(g, fb_mmu_wpr_info_r());
	val &= ~0xF;
	val |= fb_mmu_wpr_info_index_wpr1_addr_hi_v();
	gk20a_writel(g, fb_mmu_wpr_info_r(), val);

	val = gk20a_readl(g, fb_mmu_wpr_info_r()) >> 0x4;
	wpr_end = hi32_lo32_to_u64(
			(val >> (32 - WPR_INFO_ADDR_ALIGNMENT)),
			(val << WPR_INFO_ADDR_ALIGNMENT));

	inf->wpr_base = wpr_start;
	inf->nonwpr_base = 0;
	inf->size = (wpr_end - wpr_start);
}

bool gm20b_fb_debug_mode_enabled(struct gk20a *g)
{
	u32 debug_ctrl = gk20a_readl(g, gr_gpcs_pri_mmu_debug_ctrl_r());
	return gr_gpcs_pri_mmu_debug_ctrl_debug_v(debug_ctrl) ==
		gr_gpcs_pri_mmu_debug_ctrl_debug_enabled_v();
}

void gm20b_fb_set_debug_mode(struct gk20a *g, bool enable)
{
	u32 reg_val, fb_debug_ctrl, gpc_debug_ctrl;

	if (enable) {
		fb_debug_ctrl = fb_mmu_debug_ctrl_debug_enabled_f();
		gpc_debug_ctrl = gr_gpcs_pri_mmu_debug_ctrl_debug_enabled_f();
		g->mmu_debug_ctrl = true;
	} else {
		fb_debug_ctrl = fb_mmu_debug_ctrl_debug_disabled_f();
		gpc_debug_ctrl = gr_gpcs_pri_mmu_debug_ctrl_debug_disabled_f();
		g->mmu_debug_ctrl = false;
	}

	reg_val = gk20a_readl(g, fb_mmu_debug_ctrl_r());
	reg_val = set_field(reg_val,
			fb_mmu_debug_ctrl_debug_m(), fb_debug_ctrl);
	gk20a_writel(g, fb_mmu_debug_ctrl_r(), reg_val);

	reg_val = gk20a_readl(g, gr_gpcs_pri_mmu_debug_ctrl_r());
	reg_val = set_field(reg_val,
			gr_gpcs_pri_mmu_debug_ctrl_debug_m(), gpc_debug_ctrl);
	gk20a_writel(g, gr_gpcs_pri_mmu_debug_ctrl_r(), reg_val);
}
