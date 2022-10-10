/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

/* Currently, single VAB entry should suffice */
#define GA10B_VAB_NUM_ENTRIES	1UL

/* Each packet has 64 bytes (32 bytes for access bits and 32 bytes meta) */
#define GA10B_VAB_PACKET_SIZE_IN_BYTES		64UL

/* The access bits are in the first 32 bytes */
#define GA10B_VAB_PACKET_ACCESS_BITS_SIZE_IN_BYTES 32UL

int ga10b_fb_vab_init(struct gk20a *g)
{
	int err = 0;
	struct vm_gk20a *vm = g->mm.bar2.vm;
	struct nvgpu_vab *vab = &g->vab;
	struct nvgpu_mem *vab_buf = &g->vab.buffer;
	u32 vab_reg = 0U;
	u32 vab_size_exponent = 0U;
	unsigned long vab_size_bytes = 0UL;
	unsigned long vab_entry_size = 0UL;

	/* Retrieve VAB access bits length */
	vab_reg = nvgpu_readl(g, fb_mmu_vidmem_access_bit_r());
	vab_size_exponent = fb_mmu_vidmem_access_bit_size_v(vab_reg);

	/*
	 * VAB access bits = 1024 * (0x1 << exponent)
	 * VAB access bytes = 128 * (0x1 << exponent)
	 */
	vab_size_bytes = 128UL * (0x1UL << vab_size_exponent);
	nvgpu_log(g, gpu_dbg_vab, "vab access bytes %lu", vab_size_bytes);

	/*
	 * VAB dump packets have fixed width of 64B
	 * Each packet contains 32B access bits and 32B meta data.
	 * Thus, total entry size is twice of the VAB access bits.
	 */
	vab_entry_size = nvgpu_safe_mult_u32((u32)vab_size_bytes, 2UL);
	nvgpu_log(g, gpu_dbg_vab, "vab_entry_size 0x%lx", vab_entry_size);

	vab->entry_size = vab_entry_size;
	vab->num_entries = GA10B_VAB_NUM_ENTRIES;

	if (!nvgpu_mem_is_valid(vab_buf)) {
		/* Allocate memory for single VAB entry */
		err = nvgpu_dma_alloc_map_sys(vm, nvgpu_safe_mult_u32(
			(u32)vab->entry_size, vab->num_entries), vab_buf);
		if (err != 0) {
			nvgpu_err(g, "Error in vab buffer alloc in bar2 vm ");
			return -ENOMEM;
		}
	}
	nvgpu_log(g, gpu_dbg_vab, "buf_addr 0x%llx", vab_buf->gpu_va);

	g->ops.fb.vab.set_vab_buffer_address(g, vab_buf->gpu_va);

	return 0;
}

void ga10b_fb_vab_set_vab_buffer_address(struct gk20a *g, u64 buf_addr)
{
	nvgpu_writel(g, fb_mmu_vidmem_access_bit_buffer_hi_r(),
		fb_mmu_vidmem_access_bit_buffer_hi_addr_f(u64_hi32(buf_addr)));
	nvgpu_writel(g, fb_mmu_vidmem_access_bit_buffer_lo_r(),
		(fb_mmu_vidmem_access_bit_buffer_lo_addr_m() &
			u64_lo32(buf_addr)));
	nvgpu_writel(g, fb_mmu_vidmem_access_bit_buffer_size_r(),
		fb_mmu_vidmem_access_bit_buffer_size_val_f(
			g->vab.num_entries));
}

static void ga10b_fb_vab_enable(struct gk20a *g, bool enable)
{
	u32 vab_buf_size_reg = 0U;
	u32 vab_reg = 0U;

	vab_buf_size_reg = nvgpu_readl(g,
		fb_mmu_vidmem_access_bit_buffer_size_r());
	vab_reg = nvgpu_readl(g, fb_mmu_vidmem_access_bit_r());

	if (enable) {
		vab_buf_size_reg = set_field(vab_buf_size_reg,
			fb_mmu_vidmem_access_bit_buffer_size_enable_m(),
			fb_mmu_vidmem_access_bit_buffer_size_enable_f(
			fb_mmu_vidmem_access_bit_buffer_size_enable_true_v()));
		vab_reg = set_field(vab_reg,
			fb_mmu_vidmem_access_bit_enable_m(),
			fb_mmu_vidmem_access_bit_enable_f(
				fb_mmu_vidmem_access_bit_enable_true_v()));
	} else {
		vab_buf_size_reg = set_field(vab_buf_size_reg,
			fb_mmu_vidmem_access_bit_buffer_size_enable_m(),
			fb_mmu_vidmem_access_bit_buffer_size_enable_f(
			fb_mmu_vidmem_access_bit_buffer_size_enable_false_v()));
		vab_reg = set_field(vab_reg,
			fb_mmu_vidmem_access_bit_enable_m(),
			fb_mmu_vidmem_access_bit_enable_f(
				fb_mmu_vidmem_access_bit_enable_false_v()));
	}

	nvgpu_writel(g, fb_mmu_vidmem_access_bit_buffer_size_r(),
		vab_buf_size_reg);
	nvgpu_writel(g, fb_mmu_vidmem_access_bit_r(), vab_reg);

	/*
	 * - Configure VAB in GPC
	 */
	g->ops.gr.vab_configure(g, vab_reg);
}

void ga10b_fb_vab_recover(struct gk20a *g)
{
	/*
	 * This function is called while recovering from an MMU VAB_ERROR fault.
	 * It must not perform any operations which may block.
	 */
	struct nvgpu_mem *vab_buf = &g->vab.buffer;

	/*
	 * Share with polling thread that a VAB_ERROR MMU fault has happened.
	 * When this flag is set, either the other thread is still polling or
	 * polling has already timed out. This should be safe because when a
	 * new VAB dump request would be triggered, the flag would be reset.
	 * The chance of the problematic sequence (enter trigger (vab mmu fault
	 * raised) -> timeout -> enter new trigger -> just then set flag) is
	 * incredibly slim due to timing. Each trigger is a new ioctl with polling
	 * having a large timeout.
	 */
	nvgpu_atomic_set(&g->vab.mmu_vab_error_flag, 1U);

	ga10b_fb_vab_enable(g, false);

	if (nvgpu_mem_is_valid(vab_buf)) {
		g->ops.fb.vab.set_vab_buffer_address(g, vab_buf->gpu_va);
	}

	/* Re-enable */
	ga10b_fb_vab_enable(g, true);
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
	u32 granularity_shift_bits_base = 16U; /* log(64KB) */
	u32 granularity_shift_bits = 0U;
	int err = 0U;

	nvgpu_log_fn(g, " ");

	g->vab.user_num_range_checkers = num_range_checkers;
	nvgpu_log(g, gpu_dbg_vab, "num_range_checkers %u", num_range_checkers);

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
			((u32)vab_range_checker[i].start_phys_addr &
			fb_mmu_vidmem_access_bit_start_addr_lo_val_m()) |
			fb_mmu_vidmem_access_bit_start_addr_lo_granularity_f(
				granularity_shift_bits));
	}
	return err;
}

int ga10b_fb_vab_reserve(struct gk20a *g, u32 vab_mode, u32 num_range_checkers,
	struct nvgpu_vab_range_checker *vab_range_checker)
{
	u32 vab_reg = 0U;
	int err = 0U;

	nvgpu_log_fn(g, " ");

	if (num_range_checkers > fb_mmu_vidmem_access_bit_num_range_checker_v()) {
		nvgpu_err(g, "VAB range range checker config failed");
		return -EINVAL;
	}

	err = ga10b_fb_vab_config_address_range(g, num_range_checkers,
		vab_range_checker);
	if (err != 0) {
		nvgpu_err(g, "VAB range range checker config failed");
		goto fail;
	}

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

	nvgpu_writel(g, fb_mmu_vidmem_access_bit_r(), vab_reg);

	/*
	 * Setup VAB in GPC
	 */
	g->ops.gr.vab_reserve(g, vab_reg, num_range_checkers, vab_range_checker);

	/* Enable VAB */
	ga10b_fb_vab_enable(g, true);
fail:
	return err;
}

static int ga10b_fb_vab_request_dump(struct gk20a *g)
{
	struct nvgpu_timeout timeout;
	u32 delay = POLL_DELAY_MIN_US;
	u32 vab_dump_reg;
	u32 trigger_set;
	u32 trigger_reset;
	struct nvgpu_vab *vab = &g->vab;

	/*
	 * Reset VAB_ERROR MMU flag to 0 before attempting to request dump.
	 * Later, if a VAB_ERROR MMU fault is triggered, the handler will set the flag.
	 * This enables the dumping code to exit early from polling.
	 * Doing this is safe, because a VAB_ERROR MMU fault can only be raised after
	 * requesting a dump.
	 */
	nvgpu_atomic_set(&vab->mmu_vab_error_flag, 0U);

	/* Set trigger to start vab dump */
	trigger_set = fb_mmu_vidmem_access_bit_dump_trigger_f(
			fb_mmu_vidmem_access_bit_dump_trigger_true_v());
	nvgpu_writel(g, fb_mmu_vidmem_access_bit_dump_r(), trigger_set);

	/* Wait for trigger to go down */
	trigger_reset = fb_mmu_vidmem_access_bit_dump_trigger_f(
			fb_mmu_vidmem_access_bit_dump_trigger_false_v());
	nvgpu_timeout_init_cpu_timer(g, &timeout, 1000U);
	do {
		vab_dump_reg = nvgpu_readl(g, fb_mmu_vidmem_access_bit_dump_r());
		nvgpu_log(g, gpu_dbg_vab, "vab_dump_reg 0x%x", vab_dump_reg);
		if (fb_mmu_vidmem_access_bit_dump_trigger_v(vab_dump_reg) ==
			trigger_reset) {
			return 0;
		}
		nvgpu_usleep_range(delay, delay * 2U);
		delay = min_t(u32, delay << 1, POLL_DELAY_MAX_US);
	} while (nvgpu_timeout_expired(&timeout) == 0
				&& nvgpu_atomic_read(&vab->mmu_vab_error_flag) == 0);
	return -ETIMEDOUT;
}

static int ga10b_fb_vab_query_valid_bit(struct gk20a *g,
	struct nvgpu_mem *vab_buf, u64 valid_offset_in_bytes, u32 *out_valid_wr)
{
	struct nvgpu_timeout timeout;
	u32 delay = POLL_DELAY_MIN_US;
	u32 valid_wr = 0;
	nvgpu_timeout_init_cpu_timer(g, &timeout, 1000U);
	do {
		nvgpu_mem_rd_n(g, vab_buf, valid_offset_in_bytes, &valid_wr,
			sizeof(valid_wr));
		if ((valid_wr >> 31U) == 1U) {
			*out_valid_wr = valid_wr;
			return 0;
		}
		nvgpu_usleep_range(delay, delay * 2U);
		delay = min_t(u32, delay << 1, POLL_DELAY_MAX_US);
	} while (nvgpu_timeout_expired(&timeout) == 0
				&& nvgpu_atomic_read(&g->vab.mmu_vab_error_flag) == 0);
	nvgpu_err(g, "VAB write bit not valid");
	return -ETIMEDOUT;
}

int ga10b_fb_vab_dump_and_clear(struct gk20a *g, u8 *user_buf,
	u64 user_buf_size)
{
	/*
	 * set NV_PFB_PRI_MMU_VIDMEM_ACCESS_BIT_DUMP_TRIGGER
	 * poll NV_PFB_PRI_MMU_VIDMEM_ACCESS_BIT_DUMP_TRIGGER to be cleared
	 * clear what? buffer or access bits or buffer_put_ptr
	 */
	const u64 rd_wr_granularity_size = 4ULL;
	int err = 0;
	struct nvgpu_mem *vab_buf = &g->vab.buffer;
	struct nvgpu_vab *vab = &g->vab;
	u32 vab_put_ptr = 0U;
	u32 vab_put_ptr_value = 0U;
	u32 valid_wr = 0U;
	unsigned long i = 0UL;
	unsigned long vab_num_packets = 0UL;
	unsigned long vab_user_buf_min_size_bytes = 0UL;
	u64 valid_offset_in_bytes = 0ULL;

	vab_num_packets = vab->entry_size / GA10B_VAB_PACKET_SIZE_IN_BYTES;
	nvgpu_log(g, gpu_dbg_vab, "vab num_packets 0x%lx", vab_num_packets);
	vab_user_buf_min_size_bytes =
		vab_num_packets * GA10B_VAB_PACKET_ACCESS_BITS_SIZE_IN_BYTES;

	if ((user_buf_size % rd_wr_granularity_size) != 0UL) {
		/* Restriction comes from the rd_n/wr_n operations */
		nvgpu_err(g, "user_buf size must 4-bytes-aligned.");
		return -EINVAL;
	}

	if (user_buf_size < vab_user_buf_min_size_bytes) {
		nvgpu_err(g,
			"user_buf size must be at least %lu bytes. Given: %llu",
			vab_user_buf_min_size_bytes, user_buf_size);
		return -EINVAL;
	}

	/* Get buffer address */
	vab_put_ptr = nvgpu_readl(g, fb_mmu_vidmem_access_bit_buffer_put_r());
	vab_put_ptr_value =
		fb_mmu_vidmem_access_bit_buffer_put_ptr_v(vab_put_ptr);
	nvgpu_log(g, gpu_dbg_vab, "vab_put_ptr 0x%x", vab_put_ptr);

	if (vab_put_ptr_value != 0U) {
		nvgpu_err(g, "unexpected vab_put_ptr value: %u",
			vab_put_ptr_value);
		return -EINVAL;
	}

	/* Dump VAB */
	err = ga10b_fb_vab_request_dump(g);
	if (err < 0) {
		nvgpu_err(g, "VAB collection failed");
		goto done;
	}

	for (i = 0U; i < vab_num_packets; i++) {
		/*
		 * The valid bit is the very top bit of this packet's 64 bytes
		 */
		valid_offset_in_bytes =
			(i + 1ULL) * GA10B_VAB_PACKET_SIZE_IN_BYTES -
			rd_wr_granularity_size;
		/* Poll the bit to see if this packet's results are valid */
		err = ga10b_fb_vab_query_valid_bit(g, vab_buf,
			valid_offset_in_bytes, &valid_wr);
		if (err == 0) {
			/*
			 * Read VAB bits. Each packet is 64 bytes, but only 32
			 * are access bytes.  User expects contiguous dump of
			 * access bits, so some extra calculations
			 * are necessary.
			 */
			const u64 num_bytes_to_copy =
				GA10B_VAB_PACKET_ACCESS_BITS_SIZE_IN_BYTES;
			/* Determine source buffer */
			const u64 vab_offset =
				i * GA10B_VAB_PACKET_SIZE_IN_BYTES;
			/* Determine destination va */
			u8 *user_buf_destination = user_buf +
				i * GA10B_VAB_PACKET_ACCESS_BITS_SIZE_IN_BYTES;
			nvgpu_mem_rd_n(g, vab_buf, vab_offset,
				user_buf_destination, num_bytes_to_copy);
		} else {
			nvgpu_err(g, "Reading packet's %lu failed", i);
			goto clear_valid_bits;
		}
	}
	err = 0;

clear_valid_bits:
	/*
	 * Clear MSB valid bits to indicate packets were read.
	 * All bits need to be cleared even if querying failed for any of the bits.
	 */
	valid_wr = 0;
	for (i = 0U; i < vab_num_packets; i++) {
		valid_offset_in_bytes =
			(i + 1ULL) * GA10B_VAB_PACKET_SIZE_IN_BYTES -
			rd_wr_granularity_size;
		nvgpu_mem_wr_n(g, vab_buf,
			valid_offset_in_bytes, &valid_wr, sizeof(valid_wr));
	}

done:
	return err;
}

int ga10b_fb_vab_release(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	ga10b_fb_vab_enable(g, false);

	return 0;
}

int ga10b_fb_vab_teardown(struct gk20a *g)
{
	/*
	 * free vab buffer
	 */
	struct vm_gk20a *vm = g->mm.bar2.vm;
	struct nvgpu_mem *vab_buf = &g->vab.buffer;

	if (nvgpu_mem_is_valid(vab_buf)) {
		nvgpu_dma_unmap_free(vm, vab_buf);
	}

	return 0;
}
