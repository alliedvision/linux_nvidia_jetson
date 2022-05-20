/*
 * GM20B GPC MMU
 *
 * Copyright (c) 2014-2020, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/sizes.h>
#include <nvgpu/utils.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/bug.h>
#include <nvgpu/ltc.h>

#include "fb_gm20b.h"

#include <nvgpu/io.h>
#include <nvgpu/timers.h>

#include <nvgpu/hw/gm20b/hw_fb_gm20b.h>

#define VPR_INFO_FETCH_WAIT	(5)
#define WPR_INFO_ADDR_ALIGNMENT 0x0000000c

void fb_gm20b_init_fs_state(struct gk20a *g)
{
	nvgpu_log_info(g, "initialize gm20b fb");

	gk20a_writel(g, fb_fbhub_num_active_ltcs_r(),
			g->ops.priv_ring.enum_ltc(g));

	if (!nvgpu_is_enabled(g, NVGPU_SEC_PRIVSECURITY)) {
		/* Bypass MMU check for non-secure boot. For
		 * secure-boot,this register write has no-effect */
		gk20a_writel(g, fb_priv_mmu_phy_secure_r(), 0xffffffffU);
	}
}

void gm20b_fb_set_mmu_page_size(struct gk20a *g)
{
	/* set large page size in fb */
	u32 fb_mmu_ctrl = gk20a_readl(g, fb_mmu_ctrl_r());
	fb_mmu_ctrl |= fb_mmu_ctrl_use_pdb_big_page_size_true_f();
	gk20a_writel(g, fb_mmu_ctrl_r(), fb_mmu_ctrl);
}

#ifdef CONFIG_NVGPU_COMPRESSION
bool gm20b_fb_set_use_full_comp_tag_line(struct gk20a *g)
{
	/* set large page size in fb */
	u32 fb_mmu_ctrl = gk20a_readl(g, fb_mmu_ctrl_r());

	fb_mmu_ctrl |= fb_mmu_ctrl_use_full_comp_tag_line_true_f();
	gk20a_writel(g, fb_mmu_ctrl_r(), fb_mmu_ctrl);

	return true;
}

u64 gm20b_fb_compression_page_size(struct gk20a *g)
{
	return SZ_128K;
}

unsigned int gm20b_fb_compressible_page_size(struct gk20a *g)
{
	return (unsigned int)SZ_64K;
}

u64 gm20b_fb_compression_align_mask(struct gk20a *g)
{
	return SZ_64K - 1UL;
}
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
bool gm20b_fb_debug_mode_enabled(struct gk20a *g)
{
	u32 debug_ctrl = gk20a_readl(g, fb_mmu_debug_ctrl_r());

	return fb_mmu_debug_ctrl_debug_v(debug_ctrl) ==
			fb_mmu_debug_ctrl_debug_enabled_v();
}

void gm20b_fb_set_mmu_debug_mode(struct gk20a *g, bool enable)
{
	u32 reg_val, fb_debug_ctrl;

	if (enable) {
		fb_debug_ctrl = fb_mmu_debug_ctrl_debug_enabled_f();
		g->mmu_debug_ctrl = true;
	} else {
		fb_debug_ctrl = fb_mmu_debug_ctrl_debug_disabled_f();
		g->mmu_debug_ctrl = false;
	}

	reg_val = nvgpu_readl(g, fb_mmu_debug_ctrl_r());
	reg_val = set_field(reg_val,
			fb_mmu_debug_ctrl_debug_m(), fb_debug_ctrl);
	nvgpu_writel(g, fb_mmu_debug_ctrl_r(), reg_val);
}

void gm20b_fb_set_debug_mode(struct gk20a *g, bool enable)
{
	gm20b_fb_set_mmu_debug_mode(g, enable);
	g->ops.gr.set_debug_mode(g, enable);
}
#endif
