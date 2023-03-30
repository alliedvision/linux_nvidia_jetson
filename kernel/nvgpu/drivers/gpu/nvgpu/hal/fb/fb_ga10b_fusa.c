/*
 * GA10B FB
 *
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/log.h>
#include <nvgpu/enabled.h>
#include <nvgpu/errata.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/static_analysis.h>

#include "hal/fb/fb_gm20b.h"
#include "hal/fb/fb_ga10b.h"
#include "intr/fb_intr_ga10b.h"

#include <nvgpu/hw/ga10b/hw_fb_ga10b.h>

#define HSHUB_ID_0	0U

int ga10b_fb_set_atomic_mode(struct gk20a *g)
{
	u32 reg_val;
	u32 num_hshubs = 0U;
	u32 hshub_ltcs, fbhub_ltcs;
	u32 i;

	/*
	 * NV_PFB_PRI_MMU_CTRL_ATOMIC_CAPABILITY_MODE to RMW MODE
	 * NV_PFB_PRI_MMU_CTRL_ATOMIC_CAPABILITY_SYS_NCOH_MODE to L2
	 */
	reg_val = nvgpu_readl(g, fb_mmu_ctrl_r());
	reg_val = set_field(reg_val, fb_mmu_ctrl_atomic_capability_mode_m(),
		fb_mmu_ctrl_atomic_capability_mode_rmw_f());
	reg_val = set_field(reg_val,
		fb_mmu_ctrl_atomic_capability_sys_ncoh_mode_m(),
		fb_mmu_ctrl_atomic_capability_sys_ncoh_mode_l2_f());
	nvgpu_writel(g, fb_mmu_ctrl_r(), reg_val);

	/* NV_PFB_HSHUB_NUM_ACTIVE_LTCS_HUB_SYS_ATOMIC_MODE to USE_RMW */
	reg_val = nvgpu_readl(g, fb_fbhub_num_active_ltcs_r());
	reg_val = set_field(reg_val,
		fb_fbhub_num_active_ltcs_hub_sys_atomic_mode_m(),
		fb_fbhub_num_active_ltcs_hub_sys_atomic_mode_use_rmw_f());
	nvgpu_writel(g, fb_fbhub_num_active_ltcs_r(), reg_val);
	nvgpu_writel(g, fb_hshub_num_active_ltcs_r(HSHUB_ID_0), reg_val);

	/*
	 * Note: For iGPU, num_hshubs should be 1.
	 * For num_hshubs = 1, NVLINK_CAPABILITY bits are invalid and
	 * are ignored.
	 */
	reg_val = nvgpu_readl(g, fb_hshub_prg_config_r(HSHUB_ID_0));
	num_hshubs = fb_hshub_prg_config_num_hshubs_v(reg_val);

	nvgpu_assert(num_hshubs == 1U);

	/*
	 * HW expects that SW copies the value of
	 * FBHUB registers over into HSHUBs since
	 * they are supposed to have the exact same fields.
	 */
	fbhub_ltcs = nvgpu_readl(g, fb_fbhub_num_active_ltcs_r());
	for (i = 0U; i < num_hshubs; i++) {
		hshub_ltcs = nvgpu_readl(g, fb_hshub_num_active_ltcs_r(i));
		if (hshub_ltcs != fbhub_ltcs) {
			nvgpu_writel(g,
				fb_hshub_num_active_ltcs_r(i), fbhub_ltcs);
		}
	}

	return 0;

}

static void ga10b_fb_check_ltcs_count(struct gk20a *g)
{
	u32 reg_val;
	u32 ltcs_count;

	/*
	 * Number of active ltcs should be same in below registers
	 * - pri_ringmaster_enum_ltc_r
	 * - fb_mmu_num_active_ltcs_r
	 * - fb_fbhub_num_active_ltcs_r
	 *
	 * top_num_ltcs_r gives max number of ltcs. If chip is floorswept
	 * then max ltcs count may not match active ltcs count.
	 */
	ltcs_count = g->ops.priv_ring.enum_ltc(g);

	if (fb_mmu_num_active_ltcs_count_v(
		nvgpu_readl(g, fb_mmu_num_active_ltcs_r())) != ltcs_count) {
		nvgpu_err(g,
			"mmu_num_active_ltcs = %u not equal to enum_ltc() = %u",
			fb_mmu_num_active_ltcs_count_v(
				nvgpu_readl(g, fb_mmu_num_active_ltcs_r())),
			ltcs_count);
	} else {
		nvgpu_log(g, gpu_dbg_info, "mmu active ltcs %u",
			fb_mmu_num_active_ltcs_count_v(
			nvgpu_readl(g, fb_mmu_num_active_ltcs_r())));
	}

	reg_val = nvgpu_readl(g, fb_fbhub_num_active_ltcs_r());
	if (fb_fbhub_num_active_ltcs_count_v(reg_val) != ltcs_count) {
		nvgpu_err(g,
			"fbhub active_ltcs = %u != ringmaster_enum_ltc() = %u",
				fb_fbhub_num_active_ltcs_count_v(reg_val),
				ltcs_count);
		/*
		 * set num_active_ltcs = ltcs count in pri_ringmaster_enum_ltc_r
		 */
		if (nvgpu_is_errata_present(g, NVGPU_ERRATA_2969956)) {
			reg_val = set_field(reg_val,
				fb_fbhub_num_active_ltcs_count_m(),
				fb_fbhub_num_active_ltcs_count_f(ltcs_count));
			nvgpu_writel(g, fb_fbhub_num_active_ltcs_r(), reg_val);

			nvgpu_err(g, "Updated fbhub active ltcs 0x%x",
				nvgpu_readl(g, fb_fbhub_num_active_ltcs_r()));
		}
	} else {
		nvgpu_log(g, gpu_dbg_info, "fbhub active ltcs 0x%x",
			nvgpu_readl(g, fb_fbhub_num_active_ltcs_r()));
	}
}

void ga10b_fb_init_fs_state(struct gk20a *g)
{
	nvgpu_log(g, gpu_dbg_fn, "initialize ga10b fb");

#if defined(CONFIG_NVGPU_HAL_NON_FUSA)
	g->ops.mssnvlink.init_soc_credits(g);
#endif
	ga10b_fb_check_ltcs_count(g);

	if (!nvgpu_is_enabled(g, NVGPU_SEC_PRIVSECURITY)) {
		/* Bypass MMU check for non-secure boot. For
		 * secure-boot,this register write has no-effect
		 */
		nvgpu_writel(g, fb_priv_mmu_phy_secure_r(), U32_MAX);
	}
}

void ga10b_fb_init_hw(struct gk20a *g)
{

	gm20b_fb_init_hw(g);

	ga10b_fb_intr_vectorid_init(g);

	if (g->ops.fb.intr.enable != NULL) {
		g->ops.fb.intr.enable(g);
	}
}

u32 ga10b_fb_get_num_active_ltcs(struct gk20a *g)
{
	return nvgpu_readl(g, fb_mmu_num_active_ltcs_r());
}

void ga10b_fb_read_wpr_info(struct gk20a *g, u64 *wpr_base, u64 *wpr_size)
{
	u32 val = 0U;
	u64 wpr_start = 0U;
	u64 wpr_end = 0U;

	val = fb_mmu_wpr1_addr_lo_val_v(
			nvgpu_readl(g, fb_mmu_wpr1_addr_lo_r()));
	wpr_start = hi32_lo32_to_u64(
		(val >> ALIGN_HI32(fb_mmu_wpr1_addr_lo_val_alignment_v())),
		(val << fb_mmu_wpr1_addr_lo_val_alignment_v()));

	val = fb_mmu_wpr1_addr_hi_val_v(
			nvgpu_readl(g, fb_mmu_wpr1_addr_hi_r()));
	wpr_end = hi32_lo32_to_u64(
		(val >> ALIGN_HI32(fb_mmu_wpr1_addr_hi_val_alignment_v())),
		(val << fb_mmu_wpr1_addr_hi_val_alignment_v()));

	*wpr_base = wpr_start;
	*wpr_size = nvgpu_safe_sub_u64(wpr_end, wpr_start);
}

void ga10b_fb_dump_wpr_info(struct gk20a *g)
{
	u32 val;
	u32 allow_read, allow_write;
	u64 wpr1_addr_lo, wpr1_addr_hi;
	u64 wpr2_addr_lo, wpr2_addr_hi;

	allow_read = nvgpu_readl(g, fb_mmu_wpr_allow_read_r());
	allow_write = nvgpu_readl(g, fb_mmu_wpr_allow_write_r());

	val = fb_mmu_wpr1_addr_lo_val_v(nvgpu_readl(g, fb_mmu_wpr1_addr_lo_r()));
	wpr1_addr_lo = hi32_lo32_to_u64(
		(val >> ALIGN_HI32(fb_mmu_wpr1_addr_lo_val_alignment_v())),
		(val << fb_mmu_wpr1_addr_lo_val_alignment_v()));

	val = fb_mmu_wpr1_addr_hi_val_v(nvgpu_readl(g, fb_mmu_wpr1_addr_hi_r()));
	wpr1_addr_hi = hi32_lo32_to_u64(
		(val >> ALIGN_HI32(fb_mmu_wpr1_addr_hi_val_alignment_v())),
		(val << fb_mmu_wpr1_addr_hi_val_alignment_v()));

	val = fb_mmu_wpr2_addr_lo_val_v(nvgpu_readl(g, fb_mmu_wpr2_addr_lo_r()));
	wpr2_addr_lo = hi32_lo32_to_u64(
		(val >> ALIGN_HI32(fb_mmu_wpr2_addr_lo_val_alignment_v())),
		(val << fb_mmu_wpr2_addr_lo_val_alignment_v()));

	val = fb_mmu_wpr2_addr_hi_val_v(nvgpu_readl(g, fb_mmu_wpr2_addr_hi_r()));
	wpr2_addr_hi = hi32_lo32_to_u64(
		(val >> ALIGN_HI32(fb_mmu_wpr2_addr_hi_val_alignment_v())),
		(val << fb_mmu_wpr2_addr_hi_val_alignment_v()));

	nvgpu_err(g, "WPR: allow_read: 0x%08x allow_write: 0x%08x "
		"wpr1_addr_lo: 0x%08llx wpr1_addr_hi: 0x%08llx "
		"wpr2_addr_lo: 0x%08llx wpr2_addr_hi: 0x%08llx",
		allow_read, allow_write,
		wpr1_addr_lo, wpr1_addr_hi,
		wpr2_addr_lo, wpr2_addr_hi);
}

void ga10b_fb_dump_vpr_info(struct gk20a *g)
{
	u32 val;
	u32 cya_lo, cya_hi;
	u64 addr_lo, addr_hi;

	val = fb_mmu_vpr_addr_lo_val_v(
		nvgpu_readl(g, fb_mmu_vpr_addr_lo_r()));
	addr_lo = hi32_lo32_to_u64(
		(val >> ALIGN_HI32(fb_mmu_vpr_addr_lo_val_alignment_v())),
		(val << fb_mmu_vpr_addr_lo_val_alignment_v()));

	val = fb_mmu_vpr_addr_hi_val_v(
		nvgpu_readl(g, fb_mmu_vpr_addr_hi_r()));
	addr_hi = hi32_lo32_to_u64(
		(val >> ALIGN_HI32(fb_mmu_vpr_addr_hi_val_alignment_v())),
		(val << fb_mmu_vpr_addr_hi_val_alignment_v()));

	cya_lo = nvgpu_readl(g, fb_mmu_vpr_cya_lo_r());
	cya_hi = nvgpu_readl(g, fb_mmu_vpr_cya_hi_r());

	nvgpu_err(g, "VPR: addr_lo: 0x%08llx addr_hi: 0x%08llx "
		"cya_lo: 0x%08x cya_hi: 0x%08x",
		addr_lo, addr_hi, cya_lo, cya_hi);
}

static int ga10b_fb_vpr_mode_fetch_poll(struct gk20a *g, unsigned int poll_ms)
{
	struct nvgpu_timeout timeout;
	u32 val = 0U;
	u32 delay = POLL_DELAY_MIN_US;

	nvgpu_timeout_init_cpu_timer(g, &timeout, poll_ms);

	do {
		val = nvgpu_readl(g, fb_mmu_vpr_mode_r());
		if (fb_mmu_vpr_mode_fetch_v(val) ==
		    fb_mmu_vpr_mode_fetch_false_v()) {
			return 0;
		}

		nvgpu_usleep_range(delay, delay * 2U);
		delay = min_t(u32, delay << 1, POLL_DELAY_MAX_US);

	} while (nvgpu_timeout_expired(&timeout) == 0);

	return -ETIMEDOUT;
}

int ga10b_fb_vpr_info_fetch(struct gk20a *g)
{
	int err;

	err = ga10b_fb_vpr_mode_fetch_poll(g, VPR_INFO_FETCH_POLL_MS);
	if (err != 0) {
		return err;
	}

	nvgpu_writel(g, fb_mmu_vpr_mode_r(),
			fb_mmu_vpr_mode_fetch_true_f());

	err = ga10b_fb_vpr_mode_fetch_poll(g, VPR_INFO_FETCH_POLL_MS);
	if (err != 0) {
		nvgpu_err(g, "ga10b_fb_vpr_mode_fetch_poll failed!");
	}
	return err;
}
