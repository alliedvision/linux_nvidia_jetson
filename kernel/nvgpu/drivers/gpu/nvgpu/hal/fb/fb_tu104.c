/*
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/log.h>
#include <nvgpu/types.h>
#include <nvgpu/timers.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/bug.h>
#include <nvgpu/nvgpu_init.h>

#include "hal/fb/fb_gv11b.h"
#include "hal/fb/fb_gv100.h"
#include "hal/mc/mc_tu104.h"

#include "fb_tu104.h"

#include "nvgpu/hw/tu104/hw_fb_tu104.h"
#include "nvgpu/hw/tu104/hw_func_tu104.h"

int fb_tu104_tlb_invalidate(struct gk20a *g, struct nvgpu_mem *pdb)
{
	struct nvgpu_timeout timeout;
	u32 addr_lo;
	u32 data;
	int err = 0;

	nvgpu_log_fn(g, " ");

	/*
	 * pagetables are considered sw states which are preserved after
	 * prepare_poweroff. When gk20a deinit releases those pagetables,
	 * common code in vm unmap path calls tlb invalidate that touches
	 * hw. Use the power_on flag to skip tlb invalidation when gpu
	 * power is turned off
	 */
	if (nvgpu_is_powered_off(g)) {
		return 0;
	}

	addr_lo = u64_lo32(nvgpu_mem_get_addr(g, pdb) >> 12);

	nvgpu_timeout_init_retry(g, &timeout, 1000);

	nvgpu_mutex_acquire(&g->mm.tlb_lock);

#ifdef CONFIG_NVGPU_TRACE
	trace_gk20a_mm_tlb_invalidate(g->name);
#endif

	nvgpu_func_writel(g, func_priv_mmu_invalidate_pdb_r(),
		fb_mmu_invalidate_pdb_addr_f(addr_lo) |
		nvgpu_aperture_mask(g, pdb,
				fb_mmu_invalidate_pdb_aperture_sys_mem_f(),
				fb_mmu_invalidate_pdb_aperture_sys_mem_f(),
				fb_mmu_invalidate_pdb_aperture_vid_mem_f()));

	nvgpu_func_writel(g, func_priv_mmu_invalidate_r(),
		fb_mmu_invalidate_all_va_true_f() |
		fb_mmu_invalidate_trigger_true_f());

	do {
		data = nvgpu_func_readl(g,
				func_priv_mmu_invalidate_r());
		if (fb_mmu_invalidate_trigger_v(data) !=
				fb_mmu_invalidate_trigger_true_v()) {
			break;
		}
		nvgpu_udelay(2);
	} while (nvgpu_timeout_expired_msg(&timeout,
					 "wait mmu invalidate") == 0);

#ifdef CONFIG_NVGPU_TRACE
	trace_gk20a_mm_tlb_invalidate_done(g->name);
#endif

	nvgpu_mutex_release(&g->mm.tlb_lock);
	return err;
}

static int tu104_fb_wait_mmu_bind(struct gk20a *g)
{
	struct nvgpu_timeout timeout;
	u32 val;

	nvgpu_timeout_init_retry(g, &timeout, 1000);

	do {
		val = nvgpu_readl(g, fb_mmu_bind_r());
		if ((val & fb_mmu_bind_trigger_true_f()) !=
			   fb_mmu_bind_trigger_true_f()) {
			return 0;
		}
		nvgpu_udelay(2);
	} while (nvgpu_timeout_expired_msg(&timeout, "mmu bind timedout") == 0);

	return -ETIMEDOUT;
}

int tu104_fb_apply_pdb_cache_errata(struct gk20a *g)
{
	u64 inst_blk_base_addr;
	u32 inst_blk_addr;
	u32 i;
	int err;

	if (!nvgpu_mem_is_valid(&g->pdb_cache_errata_mem)) {
		return -EINVAL;
	}

	inst_blk_base_addr = nvgpu_mem_get_addr(g, &g->pdb_cache_errata_mem);

	/* Bind 256 instance blocks to unused engine ID 0x0 */
	for (i = 0U; i < 256U; i++) {
		inst_blk_addr = u64_lo32((inst_blk_base_addr +
						(U64(i) * U64(NVGPU_CPU_PAGE_SIZE)))
				>> fb_mmu_bind_imb_addr_alignment_v());

		nvgpu_writel(g, fb_mmu_bind_imb_r(),
			fb_mmu_bind_imb_addr_f(inst_blk_addr) |
			nvgpu_aperture_mask(g, &g->pdb_cache_errata_mem,
				fb_mmu_bind_imb_aperture_sys_mem_nc_f(),
				fb_mmu_bind_imb_aperture_sys_mem_c_f(),
				fb_mmu_bind_imb_aperture_vid_mem_f()));

		nvgpu_writel(g, fb_mmu_bind_r(),
			fb_mmu_bind_engine_id_f(0x0U) |
			fb_mmu_bind_trigger_true_f());

		err = tu104_fb_wait_mmu_bind(g);
		if (err != 0) {
			return err;
		}
	}

	/* first unbind */
	nvgpu_writel(g, fb_mmu_bind_imb_r(),
		fb_mmu_bind_imb_aperture_f(0x1U) |
		fb_mmu_bind_imb_addr_f(0x0U));

	nvgpu_writel(g, fb_mmu_bind_r(),
		fb_mmu_bind_engine_id_f(0x0U) |
		fb_mmu_bind_trigger_true_f());

	err = tu104_fb_wait_mmu_bind(g);
	if (err != 0) {
		return err;
	}

	/* second unbind */
	nvgpu_writel(g, fb_mmu_bind_r(),
		fb_mmu_bind_engine_id_f(0x0U) |
		fb_mmu_bind_trigger_true_f());

	err = tu104_fb_wait_mmu_bind(g);
	if (err != 0) {
		return err;
	}

	/* Bind 257th (last) instance block that reserves PDB cache entry 255 */
	inst_blk_addr = u64_lo32((inst_blk_base_addr + (256ULL * U64(NVGPU_CPU_PAGE_SIZE)))
			>> U64(fb_mmu_bind_imb_addr_alignment_v()));

	nvgpu_writel(g, fb_mmu_bind_imb_r(),
		fb_mmu_bind_imb_addr_f(inst_blk_addr) |
		nvgpu_aperture_mask(g, &g->pdb_cache_errata_mem,
			fb_mmu_bind_imb_aperture_sys_mem_nc_f(),
			fb_mmu_bind_imb_aperture_sys_mem_c_f(),
			fb_mmu_bind_imb_aperture_vid_mem_f()));

	nvgpu_writel(g, fb_mmu_bind_r(),
		fb_mmu_bind_engine_id_f(0x0U) |
		fb_mmu_bind_trigger_true_f());

	err = tu104_fb_wait_mmu_bind(g);
	if (err != 0) {
		return err;
	}

	return 0;
}

#ifdef CONFIG_NVGPU_DGPU
size_t tu104_fb_get_vidmem_size(struct gk20a *g)
{
	u32 range = gk20a_readl(g, fb_mmu_local_memory_range_r());
	u32 mag = fb_mmu_local_memory_range_lower_mag_v(range);
	u32 scale = fb_mmu_local_memory_range_lower_scale_v(range);
	u32 ecc = fb_mmu_local_memory_range_ecc_mode_v(range);
	size_t bytes = ((size_t)mag << scale) * SZ_1M;

#ifdef CONFIG_NVGPU_SIM
	if (nvgpu_is_enabled(g, NVGPU_IS_FMODEL) && (bytes == 0)) {
		/* 192 MB */
		bytes = 192*1024*1024;
	}
#endif

	if (ecc != 0U) {
		bytes = bytes / 16U * 15U;
	}

	return bytes;
}
#endif

int tu104_fb_enable_nvlink(struct gk20a *g)
{
	nvgpu_log(g, gpu_dbg_nvlink|gpu_dbg_info, "enabling nvlink");

	return gv100_fb_enable_nvlink(g);

}

int tu104_fb_set_atomic_mode(struct gk20a *g)
{
	u32 data;

	gv100_fb_set_atomic_mode(g);

	/* NV_PFB_PRI_MMU_CTRL_ATOMIC_CAPABILITY_SYS_NCOH_MODE to L2 */
	data = nvgpu_readl(g, fb_mmu_ctrl_r());
	data = set_field(data, fb_mmu_ctrl_atomic_capability_sys_ncoh_mode_m(),
		fb_mmu_ctrl_atomic_capability_sys_ncoh_mode_l2_f());
	nvgpu_writel(g, fb_mmu_ctrl_r(), data);

	/* NV_PFB_FBHUB_NUM_ACTIVE_LTCS_HUB_SYS_NCOH_ATOMIC_MODE to USE_READ */
	data = nvgpu_readl(g, fb_fbhub_num_active_ltcs_r());
	data = set_field(data,
		fb_fbhub_num_active_ltcs_hub_sys_ncoh_atomic_mode_m(),
		fb_fbhub_num_active_ltcs_hub_sys_ncoh_atomic_mode_use_read_f());
	nvgpu_writel(g, fb_fbhub_num_active_ltcs_r(), data);

	return 0;
}
