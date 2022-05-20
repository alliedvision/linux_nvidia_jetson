/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/utils.h>
#include <nvgpu/bitops.h>
#include <nvgpu/bug.h>
#include <nvgpu/io.h>
#include <nvgpu/kmem.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/timers.h>
#include <nvgpu/fuse.h>
#include <nvgpu/fb.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/dma.h>

#include "hal/fb/vab/vab_ga10b.h"
#include <nvgpu/hw/ga10b/hw_fb_ga10b.h>

#define GA10B_VAB_ENTRY			512U	/* = vab_size * 2 */
#define GA10B_VAB_WRITE_PACKETS		8U	/* = num_range_checkers */
#define GA10B_VAB_WRITE_PACKET_DWORDS	8U	/* 512/8 = 64 bytes = 16 words = 8 double words*/
#define GA10B_VAB_WRITE_PACKET_ACCESS_DWORDS	4U

int ga10b_fb_vab_init(struct gk20a *g)
{
	/* - allocate buffer and mapped in bar2
	 * - single entry is 2K bits i.e. 256 bytes
	 * - write buffer address to NV_PFB_PRI_MMU_VIDMEM_ACCESS_BIT_BUFFER_LO_ADDR
	 * and NV_PFB_PRI_MMU_VIDMEM_ACCESS_BIT_BUFFER_HI_ADDR
	 * - write NV_PFB_PRI_MMU_VIDMEM_ACCESS_BIT_BUFFER_SIZE_VAL
	 */
	int err = 0;
	size_t num_vab_entries = 2U;
	struct vm_gk20a *vm = g->mm.bar2.vm;
	struct nvgpu_mem *vab_buf = &g->mm.vab.buffer;
	u64 buf_addr = 0ULL;

	if (!nvgpu_mem_is_valid(&g->mm.vab.buffer)) {

		err = nvgpu_dma_alloc_map_sys(vm,
			num_vab_entries * GA10B_VAB_ENTRY, vab_buf);
		if (err != 0) {
			nvgpu_err(g, "Error in vab buffer alloc in bar2 vm ");
			return -ENOMEM;
		}
	}
	buf_addr = ((u64)(uintptr_t)vab_buf->gpu_va);
	nvgpu_log(g, gpu_dbg_vab, "buf_addr 0x%llx", buf_addr);

	nvgpu_writel(g, fb_mmu_vidmem_access_bit_buffer_hi_r(),
		fb_mmu_vidmem_access_bit_buffer_hi_addr_f(u64_hi32(buf_addr)));
	nvgpu_writel(g, fb_mmu_vidmem_access_bit_buffer_lo_r(),
		(fb_mmu_vidmem_access_bit_buffer_lo_addr_m() &
			u64_lo32(buf_addr)));
	nvgpu_writel(g, fb_mmu_vidmem_access_bit_buffer_size_r(),
		fb_mmu_vidmem_access_bit_buffer_size_val_f(num_vab_entries));

	return 0;
}

static int ga10b_fb_vab_config_address_range(struct gk20a *g,
	u32 num_range_checkers,
	struct nvgpu_vab_range_checker *vab_range_checker)
{
	/*
	 * - read num_range_checker, verify user passed range_checker_num
	 * - for each range_checker,
	 *   - calculate granularity log from actual granularity
	 *   - drop (granularity_shift_bits + bitmask_size_shift_bits) bits from
	 *     start address
	 *   - add granularity and start address lo/hi
	 */
	/*
	 * check range address is not in VPR
	 */
	u32 i = 0U;
	u32 max_range_checkers = fb_mmu_vidmem_access_bit_num_range_checker_v();
	u32 granularity_shift_bits_base = 16U; /* log(64KB) */
	u32 granularity_shift_bits = 0U;
	int err = 0U;

	nvgpu_log_fn(g, " ");

	g->mm.vab.user_num_range_checkers = num_range_checkers;
	nvgpu_log(g, gpu_dbg_vab, "num_range_checkers %u", num_range_checkers);

	nvgpu_assert(num_range_checkers <= max_range_checkers);

	for (i = 0U; i < num_range_checkers; i++) {
		if (vab_range_checker[i].granularity_shift <
			granularity_shift_bits_base) {
			err = -EINVAL;
			break;
		}

		granularity_shift_bits =
			vab_range_checker[i].granularity_shift -
			granularity_shift_bits_base;

		nvgpu_log(g, gpu_dbg_vab, "\t%u: granularity_shift 0x%x",
			i, vab_range_checker[i].granularity_shift);
		nvgpu_log(g, gpu_dbg_vab, "\t%u: start_phys_addr 0x%llx",
			i, vab_range_checker[i].start_phys_addr);

		nvgpu_writel(g, fb_mmu_vidmem_access_bit_start_addr_hi_r(i),
			U32(vab_range_checker[i].start_phys_addr >> 32U));

		nvgpu_writel(g, fb_mmu_vidmem_access_bit_start_addr_lo_r(i),
			(vab_range_checker[i].start_phys_addr &
			fb_mmu_vidmem_access_bit_start_addr_lo_val_m()) |
			fb_mmu_vidmem_access_bit_start_addr_lo_granularity_f(
				granularity_shift_bits));
	}
	return err;
}

int ga10b_fb_vab_reserve(struct gk20a *g, u32 vab_mode, u32 num_range_checkers,
	struct nvgpu_vab_range_checker *vab_range_checker)
{
	u32 vab_buf_size_reg = 0U;
	u32 vab_reg = 0U;
	int err = 0U;

	nvgpu_log_fn(g, " ");

	err = ga10b_fb_vab_config_address_range(g, num_range_checkers,
		vab_range_checker);
	if (err != 0) {
		nvgpu_err(g, "VAB range range checker config failed");
		goto fail;
	}

	/*
	 * - set NV_PFB_PRI_MMU_VIDMEM_ACCESS_BIT_BUFFER_SIZE_ENABLE
	 */
	vab_buf_size_reg = nvgpu_readl(g,
		fb_mmu_vidmem_access_bit_buffer_size_r());
	vab_buf_size_reg = set_field(vab_buf_size_reg,
		fb_mmu_vidmem_access_bit_buffer_size_enable_m(),
		fb_mmu_vidmem_access_bit_buffer_size_enable_f(
			fb_mmu_vidmem_access_bit_buffer_size_enable_true_v()));

	nvgpu_writel(g, fb_mmu_vidmem_access_bit_buffer_size_r(),
		vab_buf_size_reg);

	/*
	 * - Update NV_PFB_PRI_MMU_VIDMEM_ACCESS_BIT settings
	 */
	vab_reg = nvgpu_readl(g, fb_mmu_vidmem_access_bit_r());
	nvgpu_log(g, gpu_dbg_vab, "vab size %u",
		fb_mmu_vidmem_access_bit_size_v(vab_reg));

	/* disable_mode_clear: after logging is disabled, MMU clears bitmask */
	vab_reg = set_field(vab_reg, fb_mmu_vidmem_access_bit_disable_mode_m(),
		fb_mmu_vidmem_access_bit_disable_mode_f(
			fb_mmu_vidmem_access_bit_disable_mode_clear_v()));

	/* set NV_PFB_PRI_MMU_VIDMEM_ACCESS_BIT_MODE to access or dirty */
	if (vab_mode == NVGPU_VAB_MODE_ACCESS) {
		vab_reg = set_field(vab_reg, fb_mmu_vidmem_access_bit_mode_m(),
		fb_mmu_vidmem_access_bit_mode_f(
			fb_mmu_vidmem_access_bit_mode_access_v()));
	} else if (vab_mode == NVGPU_VAB_MODE_DIRTY) {
		vab_reg = set_field(vab_reg, fb_mmu_vidmem_access_bit_mode_m(),
		fb_mmu_vidmem_access_bit_mode_f(
			fb_mmu_vidmem_access_bit_mode_dirty_v()));
	} else {
		nvgpu_err(g, "Unknown vab mode");
		err = -EINVAL;
		goto fail;
	}

	/* Enable VAB */
	vab_reg = set_field(vab_reg, fb_mmu_vidmem_access_bit_enable_m(),
		fb_mmu_vidmem_access_bit_enable_f(
			fb_mmu_vidmem_access_bit_enable_true_v()));

	nvgpu_writel(g, fb_mmu_vidmem_access_bit_r(), vab_reg);

	/*
	 * Enable VAB in GPC
	 */
	g->ops.gr.vab_init(g, vab_reg, num_range_checkers, vab_range_checker);

fail:
	return err;
}

int ga10b_fb_vab_dump_and_clear(struct gk20a *g, u64 *user_buf,
	u64 user_buf_size)
{
	/*
	 * set NV_PFB_PRI_MMU_VIDMEM_ACCESS_BIT_DUMP_TRIGGER
	 * poll NV_PFB_PRI_MMU_VIDMEM_ACCESS_BIT_DUMP_TRIGGER to be cleared
	 * clear what? buffer or access bits or buffer_put_ptr
	 */
	struct nvgpu_mem *vab_buf = &g->mm.vab.buffer;
	u64 buffer_offset = 0ULL;
	u64 req_buf_size = 0U;
	u32 i = 0U, j = 0U;
	u32 user_dword_offset = 0U;
	u32 user_buf_dwords = 0U;
	u32 vab_size = 0U;
	u32 vab_dump_reg = 0U;
	u32 vab_put_ptr = 0U;
	u32 delay = POLL_DELAY_MIN_US;
	struct nvgpu_timeout timeout;
	u32 max_range_checkers = fb_mmu_vidmem_access_bit_num_range_checker_v();
	u32 trigger_set = fb_mmu_vidmem_access_bit_dump_trigger_f(
			fb_mmu_vidmem_access_bit_dump_trigger_true_v());
	u32 trigger_reset = fb_mmu_vidmem_access_bit_dump_trigger_f(
			fb_mmu_vidmem_access_bit_dump_trigger_false_v());
	u64 *wr_pkt = nvgpu_kzalloc(g, nvgpu_safe_mult_u32(sizeof(u64),
			GA10B_VAB_WRITE_PACKET_DWORDS)); /* 64B write packet */
	u32 valid_wr = 0U;
	u32 valid_mask = 0x80000000U;
	u64 valid_offset = 0ULL;
	u64 vab_offset = 0ULL;

	/* Get buffer address */
	vab_put_ptr = nvgpu_readl(g, fb_mmu_vidmem_access_bit_buffer_put_r());

	nvgpu_log(g, gpu_dbg_vab, "vab_put_ptr 0x%x", vab_put_ptr);

	buffer_offset = U64(nvgpu_safe_mult_u32(
			fb_mmu_vidmem_access_bit_buffer_put_ptr_v(vab_put_ptr),
			GA10B_VAB_ENTRY));
	nvgpu_log(g, gpu_dbg_vab, "buffer_offset 0x%llx", buffer_offset);

	vab_size = fb_mmu_vidmem_access_bit_size_v(nvgpu_readl(g,
		fb_mmu_vidmem_access_bit_r()));
	/* 1024/8 bytes * 2^vab_size */
	req_buf_size = nvgpu_safe_mult_u64(128ULL, (1ULL << vab_size));
	/* buffer size will correspond to user range checker count */
	req_buf_size = (req_buf_size/max_range_checkers) *
		g->mm.vab.user_num_range_checkers;

	nvgpu_assert(user_buf_size >= req_buf_size);

	/* bytes to dwords */
	user_buf_dwords = user_buf_size/8;

	/* Set trigger to start vab dump */
	nvgpu_writel(g, fb_mmu_vidmem_access_bit_dump_r(), trigger_set);

	vab_dump_reg = nvgpu_readl(g, fb_mmu_vidmem_access_bit_dump_r());
	nvgpu_log(g, gpu_dbg_vab, "vab_dump_reg 0x%x", vab_dump_reg);

	nvgpu_timeout_init_cpu_timer(g, &timeout, 1000U);

	/* Check if trigger is cleared vab bits collection complete */
	do {
		nvgpu_usleep_range(delay, delay * 2U);
		delay = min_t(u32, delay << 1, POLL_DELAY_MAX_US);

		vab_dump_reg = nvgpu_readl(g, fb_mmu_vidmem_access_bit_dump_r());
		nvgpu_log(g, gpu_dbg_vab, "vab_dump_reg 0x%x", vab_dump_reg);
	} while((fb_mmu_vidmem_access_bit_dump_trigger_v(vab_dump_reg) !=
			trigger_reset) &&
		(nvgpu_timeout_expired(&timeout) == 0));

	user_dword_offset = 0U;

	for (i = 0U; i < GA10B_VAB_WRITE_PACKETS; i++) {
		/* Poll valid bit for write packet i */
		valid_offset = (buffer_offset / 4ULL) +
			((i+1) * (GA10B_VAB_WRITE_PACKET_DWORDS * 2)) - 1;
		nvgpu_log(g, gpu_dbg_vab, "Read valid bit at 0x%llx offset",
			valid_offset);

		do {
			valid_wr = nvgpu_mem_rd32(g, vab_buf, valid_offset);
		} while (valid_wr != valid_mask);

		/* Read VAB bits */
		vab_offset = buffer_offset +
			(i * GA10B_VAB_WRITE_PACKET_DWORDS * 8U);
		nvgpu_mem_rd_n(g, vab_buf, vab_offset , (void *)wr_pkt,
			GA10B_VAB_WRITE_PACKET_DWORDS * 8U);

		/* Copy and print access bits to user buffer */
		for (j = 0U; j < GA10B_VAB_WRITE_PACKET_DWORDS; j++) {

			if ((user_dword_offset < user_buf_dwords) &&
			    (j < GA10B_VAB_WRITE_PACKET_ACCESS_DWORDS)) {
				user_buf[user_dword_offset++] = wr_pkt[j];
			}
			nvgpu_log(g, gpu_dbg_vab, "wr_pkt %d: 0x%016llx",
				j, wr_pkt[j]);
		}

		/* Clear MSB valid bit to indicate packet read complete */
		nvgpu_mem_wr32(g, vab_buf, valid_offset,
			(valid_wr & ~valid_mask));
	}

	nvgpu_kfree(g, wr_pkt);
	return 0;
}

int ga10b_fb_vab_release(struct gk20a *g)
{
	/*
	 * - reset NV_PFB_PRI_MMU_VIDMEM_ACCESS_BIT_BUFFER_SIZE_ENABLE
	 * - reset NV_PFB_PRI_MMU_VIDMEM_ACCESS_BIT_ENABLE
	 */
	u32 vab_buf_size_reg = 0U;
	u32 vab_reg = 0U;

	nvgpu_log_fn(g, " ");

	vab_buf_size_reg = nvgpu_readl(g,
		fb_mmu_vidmem_access_bit_buffer_size_r());
	vab_buf_size_reg = set_field(vab_buf_size_reg,
		fb_mmu_vidmem_access_bit_buffer_size_enable_m(),
		fb_mmu_vidmem_access_bit_buffer_size_enable_f(
			fb_mmu_vidmem_access_bit_buffer_size_enable_false_v()));
	nvgpu_writel(g, fb_mmu_vidmem_access_bit_buffer_size_r(), vab_buf_size_reg);

	vab_reg = nvgpu_readl(g, fb_mmu_vidmem_access_bit_r());
	vab_reg = set_field(vab_reg, fb_mmu_vidmem_access_bit_enable_m(),
		fb_mmu_vidmem_access_bit_enable_f(
			fb_mmu_vidmem_access_bit_enable_false_v()));
	nvgpu_writel(g, fb_mmu_vidmem_access_bit_r(), vab_reg);

	/*
	 * - Disable VAB in GPC
	 */
	g->ops.gr.vab_release(g, vab_reg);

	return 0;
}

int ga10b_fb_vab_teardown(struct gk20a *g)
{
	/*
	 * free vab buffer
	 */
	struct vm_gk20a *vm = g->mm.bar2.vm;
	struct nvgpu_mem *vab_buf = &g->mm.vab.buffer;

	if (nvgpu_mem_is_valid(vab_buf)) {
		nvgpu_dma_unmap_free(vm, vab_buf);
	}

	return 0;
}
