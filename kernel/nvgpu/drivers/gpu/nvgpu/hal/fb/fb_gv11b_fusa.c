/*
 * GV11B FB
 *
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

static void gv11b_init_nvlink_soc_credits(struct gk20a *g)
{
	(void)g;
#ifndef __NVGPU_POSIX__
	if (nvgpu_platform_is_silicon(g)) {
		nvgpu_log(g, gpu_dbg_info, "nvlink soc credits init done by bpmp");
	} else {
#ifdef CONFIG_NVGPU_NVLINK
		nvgpu_mss_nvlink_init_credits(g);
#endif
	}
#endif
}

int gv11b_fb_set_atomic_mode(struct gk20a *g)
{
	u32 reg_val;

	/*
	 * NV_PFB_PRI_MMU_CTRL_ATOMIC_CAPABILITY_MODE to RMW MODE
	 * NV_PFB_PRI_MMU_CTRL_ATOMIC_CAPABILITY_SYS_NCOH_MODE to L2
	 */
	reg_val = nvgpu_readl(g, fb_mmu_ctrl_r());
	reg_val = set_field(reg_val, fb_mmu_ctrl_atomic_capability_mode_m(),
		fb_mmu_ctrl_atomic_capability_mode_rmw_f());
	reg_val = set_field(reg_val, fb_mmu_ctrl_atomic_capability_sys_ncoh_mode_m(),
		fb_mmu_ctrl_atomic_capability_sys_ncoh_mode_l2_f());
	nvgpu_writel(g, fb_mmu_ctrl_r(), reg_val);

	/* NV_PFB_HSHUB_NUM_ACTIVE_LTCS_HUB_SYS_ATOMIC_MODE to USE_RMW */
	reg_val = nvgpu_readl(g, fb_hshub_num_active_ltcs_r());
	reg_val = set_field(reg_val, fb_hshub_num_active_ltcs_hub_sys_atomic_mode_m(),
		    fb_hshub_num_active_ltcs_hub_sys_atomic_mode_use_rmw_f());
	nvgpu_writel(g, fb_hshub_num_active_ltcs_r(), reg_val);

	nvgpu_log(g,  gpu_dbg_info, "fb_mmu_ctrl_r 0x%x",
					nvgpu_readl(g, fb_mmu_ctrl_r()));

	nvgpu_log(g,   gpu_dbg_info, "fb_hshub_num_active_ltcs_r 0x%x",
			nvgpu_readl(g, fb_hshub_num_active_ltcs_r()));

	return 0;
}

void gv11b_fb_init_hw(struct gk20a *g)
{
	gm20b_fb_init_hw(g);

	g->ops.fb.intr.enable(g);
}

void gv11b_fb_init_fs_state(struct gk20a *g)
{
	nvgpu_log(g, gpu_dbg_fn, "initialize gv11b fb");

	gv11b_init_nvlink_soc_credits(g);

	nvgpu_log(g, gpu_dbg_info, "fbhub active ltcs %x",
			nvgpu_readl(g, fb_fbhub_num_active_ltcs_r()));

	nvgpu_log(g, gpu_dbg_info, "mmu active ltcs %u",
			fb_mmu_num_active_ltcs_count_v(
			nvgpu_readl(g, fb_mmu_num_active_ltcs_r())));

	if (!nvgpu_is_enabled(g, NVGPU_SEC_PRIVSECURITY)) {
		/* Bypass MMU check for non-secure boot. For
		 * secure-boot,this register write has no-effect */
		nvgpu_writel(g, fb_priv_mmu_phy_secure_r(), U32_MAX);
	}
}

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
		nvgpu_ltc_get_ltc_count(g)) <<
		fb_mmu_cbc_base_address_alignment_shift_v();

	if (compbit_base_post_multiply64 < compbit_store_iova) {
		compbit_base_post_divide++;
	}

	if (g->ops.cbc.fix_config != NULL) {
		compbit_base_post_divide =
			g->ops.cbc.fix_config(g, (int)compbit_base_post_divide);
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
