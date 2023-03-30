/*
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
#include <nvgpu/engines.h>
#include <nvgpu/channel.h>
#include <nvgpu/tsg.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/ltc.h>
#include <nvgpu/rc.h>
#include <nvgpu/string.h>

#include "hal/fb/fb_mmu_fault_gv11b.h"
#include "hal/mm/mmu_fault/mmu_fault_gv11b.h"

#include "fb_gm20b.h"
#include "fb_gp10b.h"
#include "fb_gv11b.h"

#include <nvgpu/hw/gv11b/hw_fb_gv11b.h>

static const char *const gv11b_fault_access_type_descs[] = {
	"virt read",
	"virt write",
	"virt atomic strong",
	"virt prefetch",
	"virt atomic weak",
	"xxx",
	"xxx",
	"xxx",
	"phys read",
	"phys write",
	"phys atomic",
	"phys prefetch",
};

bool gv11b_fb_is_fault_buf_enabled(struct gk20a *g, u32 index)
{
	u32 reg_val;

	reg_val = g->ops.fb.read_mmu_fault_buffer_size(g, index);
	return fb_mmu_fault_buffer_size_enable_v(reg_val) != 0U;
}

void gv11b_fb_fault_buffer_get_ptr_update(struct gk20a *g,
				 u32 index, u32 next)
{
	u32 reg_val;

	nvgpu_log(g, gpu_dbg_intr, "updating get index with = %d", next);

	reg_val = g->ops.fb.read_mmu_fault_buffer_get(g, index);
	reg_val = set_field(reg_val, fb_mmu_fault_buffer_get_ptr_m(),
			 fb_mmu_fault_buffer_get_ptr_f(next));

	/*
	 * while the fault is being handled it is possible for overflow
	 * to happen,
	 */
	if ((reg_val & fb_mmu_fault_buffer_get_overflow_m()) != 0U) {
		reg_val |= fb_mmu_fault_buffer_get_overflow_clear_f();
	}

	g->ops.fb.write_mmu_fault_buffer_get(g, index, reg_val);

	/*
	 * make sure get ptr update is visible to everyone to avoid
	 * reading already read entry
	 */
	nvgpu_mb();
}

static u32 gv11b_fb_fault_buffer_get_index(struct gk20a *g, u32 index)
{
	u32 reg_val;

	reg_val = g->ops.fb.read_mmu_fault_buffer_get(g, index);
	return fb_mmu_fault_buffer_get_ptr_v(reg_val);
}

static u32 gv11b_fb_fault_buffer_put_index(struct gk20a *g, u32 index)
{
	u32 reg_val;

	reg_val = g->ops.fb.read_mmu_fault_buffer_put(g, index);
	return fb_mmu_fault_buffer_put_ptr_v(reg_val);
}

u32 gv11b_fb_fault_buffer_size_val(struct gk20a *g, u32 index)
{
	u32 reg_val;

	reg_val = g->ops.fb.read_mmu_fault_buffer_size(g, index);
	return fb_mmu_fault_buffer_size_val_v(reg_val);
}

bool gv11b_fb_is_fault_buffer_empty(struct gk20a *g,
		 u32 index, u32 *get_idx)
{
	u32 put_idx;

	*get_idx = gv11b_fb_fault_buffer_get_index(g, index);
	put_idx = gv11b_fb_fault_buffer_put_index(g, index);

	return *get_idx == put_idx;
}

static bool gv11b_fb_is_fault_buffer_full(struct gk20a *g, u32 index)
{
	u32 get_idx, put_idx, entries;


	get_idx = gv11b_fb_fault_buffer_get_index(g, index);

	put_idx = gv11b_fb_fault_buffer_put_index(g, index);

	entries = gv11b_fb_fault_buffer_size_val(g, index);

	return get_idx == ((put_idx + 1U) % entries);
}

void gv11b_fb_fault_buf_set_state_hw(struct gk20a *g,
		 u32 index, u32 state)
{
	u32 fault_status;
	u32 reg_val;

	nvgpu_log_fn(g, " ");

	reg_val = g->ops.fb.read_mmu_fault_buffer_size(g, index);
	if (state == NVGPU_MMU_FAULT_BUF_ENABLED) {
		if (gv11b_fb_is_fault_buf_enabled(g, index)) {
			nvgpu_log_info(g, "fault buffer is already enabled");
		} else {
			reg_val |= fb_mmu_fault_buffer_size_enable_true_f();
			g->ops.fb.write_mmu_fault_buffer_size(g, index,
				reg_val);
		}

	} else {
		struct nvgpu_timeout timeout;
		u32 delay = POLL_DELAY_MIN_US;

		nvgpu_timeout_init_cpu_timer(g, &timeout, nvgpu_get_poll_timeout(g));

		reg_val &= (~(fb_mmu_fault_buffer_size_enable_m()));
		g->ops.fb.write_mmu_fault_buffer_size(g, index, reg_val);

		fault_status = g->ops.fb.read_mmu_fault_status(g);

		do {
			if ((fault_status &
			     fb_mmu_fault_status_busy_true_f()) == 0U) {
				break;
			}
			/*
			 * Make sure fault buffer is disabled.
			 * This is to avoid accessing fault buffer by hw
			 * during the window BAR2 is being unmapped by s/w
			 */
			nvgpu_log_info(g, "fault status busy set, check again");
			fault_status = g->ops.fb.read_mmu_fault_status(g);

			nvgpu_usleep_range(delay, delay * 2U);
			delay = min_t(u32, delay << 1, POLL_DELAY_MAX_US);
		} while (nvgpu_timeout_expired_msg(&timeout,
				"fault status busy set") == 0);
	}
}

void gv11b_fb_fault_buf_configure_hw(struct gk20a *g, u32 index)
{
	u32 addr_lo;
	u32 addr_hi;

	nvgpu_log_fn(g, " ");

	gv11b_fb_fault_buf_set_state_hw(g, index,
					 NVGPU_MMU_FAULT_BUF_DISABLED);
	addr_lo = u64_lo32(g->mm.hw_fault_buf[index].gpu_va >>
					fb_mmu_fault_buffer_lo_addr_b());
	addr_hi = u64_hi32(g->mm.hw_fault_buf[index].gpu_va);

	g->ops.fb.write_mmu_fault_buffer_lo_hi(g, index,
		fb_mmu_fault_buffer_lo_addr_f(addr_lo),
		fb_mmu_fault_buffer_hi_addr_f(addr_hi));

	g->ops.fb.write_mmu_fault_buffer_size(g, index,
		fb_mmu_fault_buffer_size_val_f(g->ops.channel.count(g)) |
		fb_mmu_fault_buffer_size_overflow_intr_enable_f());

	gv11b_fb_fault_buf_set_state_hw(g, index, NVGPU_MMU_FAULT_BUF_ENABLED);
}

void gv11b_fb_write_mmu_fault_buffer_lo_hi(struct gk20a *g, u32 index,
	u32 addr_lo, u32 addr_hi)
{
	nvgpu_writel(g, fb_mmu_fault_buffer_lo_r(index), addr_lo);
	nvgpu_writel(g, fb_mmu_fault_buffer_hi_r(index), addr_hi);
}

u32 gv11b_fb_read_mmu_fault_buffer_get(struct gk20a *g, u32 index)
{
	return nvgpu_readl(g, fb_mmu_fault_buffer_get_r(index));
}

void fb_gv11b_write_mmu_fault_buffer_get(struct gk20a *g, u32 index,
	u32 reg_val)
{
	nvgpu_writel(g, fb_mmu_fault_buffer_get_r(index), reg_val);
}

u32 gv11b_fb_read_mmu_fault_buffer_put(struct gk20a *g, u32 index)
{
	return nvgpu_readl(g, fb_mmu_fault_buffer_put_r(index));
}

u32 gv11b_fb_read_mmu_fault_buffer_size(struct gk20a *g, u32 index)
{
	return nvgpu_readl(g, fb_mmu_fault_buffer_size_r(index));
}

void gv11b_fb_write_mmu_fault_buffer_size(struct gk20a *g, u32 index,
	u32 reg_val)
{
	nvgpu_writel(g, fb_mmu_fault_buffer_size_r(index), reg_val);
}

void gv11b_fb_read_mmu_fault_addr_lo_hi(struct gk20a *g,
	u32 *addr_lo, u32 *addr_hi)
{
	*addr_lo = nvgpu_readl(g, fb_mmu_fault_addr_lo_r());
	*addr_hi = nvgpu_readl(g, fb_mmu_fault_addr_hi_r());
}

void gv11b_fb_read_mmu_fault_inst_lo_hi(struct gk20a *g,
	u32 *inst_lo, u32 *inst_hi)
{
	*inst_lo = nvgpu_readl(g, fb_mmu_fault_inst_lo_r());
	*inst_hi = nvgpu_readl(g, fb_mmu_fault_inst_hi_r());
}

u32 gv11b_fb_read_mmu_fault_info(struct gk20a *g)
{
	return nvgpu_readl(g, fb_mmu_fault_info_r());
}

u32 gv11b_fb_read_mmu_fault_status(struct gk20a *g)
{
	return nvgpu_readl(g, fb_mmu_fault_status_r());
}

void gv11b_fb_write_mmu_fault_status(struct gk20a *g, u32 reg_val)
{

	nvgpu_writel(g, fb_mmu_fault_status_r(), reg_val);
}

void gv11b_fb_mmu_fault_info_dump(struct gk20a *g,
			 struct mmu_fault_info *mmufault)
{
	if ((mmufault != NULL) && mmufault->valid) {
		nvgpu_err(g, "[MMU FAULT] "
			"mmu engine id:  %d, "
			"ch id:  %d, "
			"fault addr: 0x%llx, "
			"fault addr aperture: %d, "
			"fault type: %s, "
			"access type: %s, ",
			mmufault->mmu_engine_id,
			mmufault->chid,
			mmufault->fault_addr,
			mmufault->fault_addr_aperture,
			mmufault->fault_type_desc,
			gv11b_fault_access_type_descs[mmufault->access_type]);
		nvgpu_err(g, "[MMU FAULT] "
			"protected mode: %d, "
			"client type: %s, "
			"client id:  %s, "
			"gpc id if client type is gpc: %d, ",
			mmufault->protected_mode,
			mmufault->client_type_desc,
			mmufault->client_id_desc,
			mmufault->gpc_id);

		nvgpu_log(g, gpu_dbg_intr, "[MMU FAULT] "
			"faulted act eng id if any: 0x%x, "
			"faulted veid if any: 0x%x, "
			"faulted pbdma id if any: 0x%x, ",
			mmufault->faulted_engine,
			mmufault->faulted_subid,
			mmufault->faulted_pbdma);
		nvgpu_log(g, gpu_dbg_intr, "[MMU FAULT] "
			"inst ptr: 0x%llx, "
			"inst ptr aperture: %d, "
			"replayable fault: %d, "
			"replayable fault en:  %d "
			"timestamp hi:lo 0x%08x:0x%08x, ",
			mmufault->inst_ptr,
			mmufault->inst_aperture,
			mmufault->replayable_fault,
			mmufault->replay_fault_en,
			mmufault->timestamp_hi, mmufault->timestamp_lo);
	}
}

void gv11b_mm_copy_from_fault_snap_reg(struct gk20a *g,
		u32 fault_status, struct mmu_fault_info *mmufault)
{
	u32 reg_val;
	u32 addr_lo, addr_hi;
	u64 inst_ptr;
	u32 chid = NVGPU_INVALID_CHANNEL_ID;
	struct nvgpu_channel *refch;

	(void) memset(mmufault, 0, sizeof(*mmufault));

	if ((fault_status & fb_mmu_fault_status_valid_set_f()) == 0U) {

		nvgpu_log(g, gpu_dbg_intr, "mmu fault status valid not set");
		return;
	}

	g->ops.fb.read_mmu_fault_inst_lo_hi(g, &reg_val, &addr_hi);

	addr_lo = fb_mmu_fault_inst_lo_addr_v(reg_val);
	addr_lo = addr_lo << fb_mmu_fault_inst_lo_addr_b();

	addr_hi = fb_mmu_fault_inst_hi_addr_v(addr_hi);
	inst_ptr = hi32_lo32_to_u64(addr_hi, addr_lo);

	/* refch will be put back after fault is handled */
	refch = nvgpu_channel_refch_from_inst_ptr(g, inst_ptr);
	if (refch != NULL) {
		chid = refch->chid;
	}

	/* It is still ok to continue if refch is NULL */
	mmufault->refch = refch;
	mmufault->chid = chid;
	mmufault->inst_ptr = inst_ptr;
	mmufault->inst_aperture = fb_mmu_fault_inst_lo_aperture_v(reg_val);
	mmufault->mmu_engine_id = fb_mmu_fault_inst_lo_engine_id_v(reg_val);

	nvgpu_engine_mmu_fault_id_to_eng_ve_pbdma_id(g, mmufault->mmu_engine_id,
		 &mmufault->faulted_engine, &mmufault->faulted_subid,
		 &mmufault->faulted_pbdma);

	g->ops.fb.read_mmu_fault_addr_lo_hi(g, &reg_val, &addr_hi);

	addr_lo = fb_mmu_fault_addr_lo_addr_v(reg_val);
	addr_lo = addr_lo << fb_mmu_fault_addr_lo_addr_b();

	mmufault->fault_addr_aperture =
			 fb_mmu_fault_addr_lo_phys_aperture_v(reg_val);

	addr_hi = fb_mmu_fault_addr_hi_addr_v(addr_hi);
	mmufault->fault_addr = hi32_lo32_to_u64(addr_hi, addr_lo);

	reg_val = g->ops.fb.read_mmu_fault_info(g);
	mmufault->fault_type = fb_mmu_fault_info_fault_type_v(reg_val);
	mmufault->replayable_fault =
			(fb_mmu_fault_info_replayable_fault_v(reg_val) == 1U);
	mmufault->client_id = fb_mmu_fault_info_client_v(reg_val);
	mmufault->access_type = fb_mmu_fault_info_access_type_v(reg_val);
	mmufault->client_type = fb_mmu_fault_info_client_type_v(reg_val);
	mmufault->gpc_id = fb_mmu_fault_info_gpc_id_v(reg_val);
	mmufault->protected_mode =
			 fb_mmu_fault_info_protected_mode_v(reg_val);
	mmufault->replay_fault_en =
			fb_mmu_fault_info_replayable_fault_en_v(reg_val);

	mmufault->valid = (fb_mmu_fault_info_valid_v(reg_val) == 1U);

	fault_status &= ~(fb_mmu_fault_status_valid_m());
	g->ops.fb.write_mmu_fault_status(g, fault_status);

	g->ops.mm.mmu_fault.parse_mmu_fault_info(mmufault);

}

void gv11b_fb_handle_nonreplay_fault_overflow(struct gk20a *g,
			 u32 fault_status)
{
	u32 reg_val;
	u32 index = NVGPU_MMU_FAULT_NONREPLAY_REG_INDX;

	reg_val = g->ops.fb.read_mmu_fault_buffer_get(g, index);

	if ((fault_status &
	     fb_mmu_fault_status_non_replayable_getptr_corrupted_m()) != 0U) {

		nvgpu_err(g, "non replayable getptr corrupted set");

		gv11b_fb_fault_buf_configure_hw(g, index);

		reg_val = set_field(reg_val,
			fb_mmu_fault_buffer_get_getptr_corrupted_m(),
			fb_mmu_fault_buffer_get_getptr_corrupted_clear_f());
	}

	if ((fault_status &
	     fb_mmu_fault_status_non_replayable_overflow_m()) != 0U) {

		bool buffer_full = gv11b_fb_is_fault_buffer_full(g, index);

		nvgpu_err(g, "non replayable overflow: buffer full:%s",
				buffer_full?"true":"false");

		reg_val = set_field(reg_val,
			fb_mmu_fault_buffer_get_overflow_m(),
			fb_mmu_fault_buffer_get_overflow_clear_f());
	}

	g->ops.fb.write_mmu_fault_buffer_get(g, index, reg_val);
}

void gv11b_fb_handle_bar2_fault(struct gk20a *g,
			struct mmu_fault_info *mmufault, u32 fault_status)
{
	int err;

	if ((fault_status &
	     fb_mmu_fault_status_non_replayable_error_m()) != 0U) {
		if (gv11b_fb_is_fault_buf_enabled(g,
				NVGPU_MMU_FAULT_NONREPLAY_REG_INDX)) {
			gv11b_fb_fault_buf_configure_hw(g,
				NVGPU_MMU_FAULT_NONREPLAY_REG_INDX);
		}
	}

#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	if ((fault_status & fb_mmu_fault_status_replayable_error_m()) != 0U) {
		if (gv11b_fb_is_fault_buf_enabled(g,
				NVGPU_MMU_FAULT_REPLAY_REG_INDX)) {
			gv11b_fb_fault_buf_configure_hw(g,
				NVGPU_MMU_FAULT_REPLAY_REG_INDX);
		}
	}
#endif
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	g->ops.ce.mthd_buffer_fault_in_bar2_fault(g);
#endif
	err = g->ops.bus.bar2_bind(g, &g->mm.bar2.inst_block);
	if (err != 0) {
		nvgpu_err(g, "bar2_bind failed!");
	}

	if (mmufault->refch != NULL) {
		nvgpu_channel_put(mmufault->refch);
		mmufault->refch = NULL;
	}
}

void gv11b_fb_handle_dropped_mmu_fault(struct gk20a *g, u32 fault_status)
{
	u32 dropped_faults = 0;

	dropped_faults = fb_mmu_fault_status_dropped_bar1_phys_set_f() |
			fb_mmu_fault_status_dropped_bar1_virt_set_f() |
			fb_mmu_fault_status_dropped_bar2_phys_set_f() |
			fb_mmu_fault_status_dropped_bar2_virt_set_f() |
			fb_mmu_fault_status_dropped_ifb_phys_set_f() |
			fb_mmu_fault_status_dropped_ifb_virt_set_f() |
			fb_mmu_fault_status_dropped_other_phys_set_f()|
			fb_mmu_fault_status_dropped_other_virt_set_f();

	if ((fault_status & dropped_faults) != 0U) {
		nvgpu_err(g, "dropped mmu fault (0x%08x)",
				 fault_status & dropped_faults);
		g->ops.fb.write_mmu_fault_status(g, dropped_faults);
	}
}

void gv11b_fb_handle_mmu_fault(struct gk20a *g, u32 niso_intr)
{
	u32 fault_status = g->ops.fb.read_mmu_fault_status(g);

	nvgpu_log(g, gpu_dbg_intr, "mmu_fault_status = 0x%08x", fault_status);

	if ((niso_intr &
	     fb_niso_intr_mmu_other_fault_notify_m()) != 0U) {

		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_HUBMMU,
				GPU_HUBMMU_PAGE_FAULT_OTHER_FAULT_NOTIFY_ERROR);
		nvgpu_err(g, "GPU_HUBMMU_PAGE_FAULT_ERROR. "
				"sub-err: OTHER_FAULT_NOTIFY. "
				"fault_status(0x%x)", fault_status);

		gv11b_fb_handle_dropped_mmu_fault(g, fault_status);

		gv11b_mm_mmu_fault_handle_other_fault_notify(g, fault_status);
	}

	if (gv11b_fb_is_fault_buf_enabled(g, NVGPU_MMU_FAULT_NONREPLAY_REG_INDX)) {

		if ((niso_intr &
		     fb_niso_intr_mmu_nonreplayable_fault_notify_m()) != 0U) {

			gv11b_mm_mmu_fault_handle_nonreplay_replay_fault(g,
					fault_status,
					NVGPU_MMU_FAULT_NONREPLAY_REG_INDX);

			/*
			 * When all the faults are processed,
			 * GET and PUT will have same value and mmu fault status
			 * bit will be reset by HW
			 */
		}
		if ((niso_intr &
		     fb_niso_intr_mmu_nonreplayable_fault_overflow_m()) != 0U) {

			nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_HUBMMU,
				GPU_HUBMMU_PAGE_FAULT_NONREPLAYABLE_FAULT_OVERFLOW_ERROR);
			nvgpu_err(g, "GPU_HUBMMU_PAGE_FAULT_ERROR. "
				"sub-err: NONREPLAYABLE_FAULT_OVERFLOW. "
				"fault_status(0x%x)", fault_status);

			gv11b_fb_handle_nonreplay_fault_overflow(g,
				 fault_status);
		}

	}

#ifdef CONFIG_NVGPU_SUPPORT_MMU_REPLAYABLE_FAULT
	if (gv11b_fb_is_fault_buf_enabled(g, NVGPU_MMU_FAULT_REPLAY_REG_INDX)) {

		if ((niso_intr &
		     fb_niso_intr_mmu_replayable_fault_notify_m()) != 0U) {

			gv11b_mm_mmu_fault_handle_nonreplay_replay_fault(g,
					fault_status,
					NVGPU_MMU_FAULT_REPLAY_REG_INDX);
		}
		if ((niso_intr &
		     fb_niso_intr_mmu_replayable_fault_overflow_m()) != 0U) {

			nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_HUBMMU,
				GPU_HUBMMU_PAGE_FAULT_REPLAYABLE_FAULT_OVERFLOW_ERROR);
			nvgpu_err(g, "GPU_HUBMMU_PAGE_FAULT_ERROR. "
				"sub-err: REPLAYABLE_FAULT_OVERFLOW. "
				"fault_status(0x%x)", fault_status);

			gv11b_fb_handle_replay_fault_overflow(g,
				 fault_status);
		}

	}
#endif

	nvgpu_log(g, gpu_dbg_intr, "clear mmu fault status");
	g->ops.fb.write_mmu_fault_status(g,
			fb_mmu_fault_status_valid_clear_f());
}

#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
void gv11b_fb_handle_replayable_mmu_fault(struct gk20a *g)
{
	u32 fault_status = nvgpu_readl(g, fb_mmu_fault_status_r());

	if ((fault_status & fb_mmu_fault_status_replayable_m()) == 0U) {
		return;
	}

	if (gv11b_fb_is_fault_buf_enabled(g,
			NVGPU_MMU_FAULT_NONREPLAY_REG_INDX)) {
		gv11b_mm_mmu_fault_handle_nonreplay_replay_fault(g,
				fault_status,
				NVGPU_MMU_FAULT_REPLAY_REG_INDX);
	}
}

void gv11b_fb_handle_replay_fault_overflow(struct gk20a *g,
			 u32 fault_status)
{
	u32 reg_val;
	u32 index = NVGPU_MMU_FAULT_REPLAY_REG_INDX;

	reg_val = g->ops.fb.read_mmu_fault_buffer_get(g, index);

	if ((fault_status &
	     fb_mmu_fault_status_replayable_getptr_corrupted_m()) != 0U) {

		nvgpu_err(g, "replayable getptr corrupted set");

		gv11b_fb_fault_buf_configure_hw(g, index);

		reg_val = set_field(reg_val,
			fb_mmu_fault_buffer_get_getptr_corrupted_m(),
			fb_mmu_fault_buffer_get_getptr_corrupted_clear_f());
	}

	if ((fault_status &
	     fb_mmu_fault_status_replayable_overflow_m()) != 0U) {
		bool buffer_full = gv11b_fb_is_fault_buffer_full(g, index);

		nvgpu_err(g, "replayable overflow: buffer full:%s",
				buffer_full?"true":"false");

		reg_val = set_field(reg_val,
			fb_mmu_fault_buffer_get_overflow_m(),
			fb_mmu_fault_buffer_get_overflow_clear_f());
	}

	g->ops.fb.write_mmu_fault_buffer_get(g, index, reg_val);
}

int gv11b_fb_replay_or_cancel_faults(struct gk20a *g,
			 u32 invalidate_replay_val)
{
	int err = 0;

	nvgpu_log_fn(g, " ");

	if ((invalidate_replay_val &
	     fb_mmu_invalidate_replay_cancel_global_f()) != 0U) {
		/*
		 * cancel faults so that next time it faults as
		 * replayable faults and channel recovery can be done
		 */
		err = g->ops.fb.mmu_invalidate_replay(g,
			fb_mmu_invalidate_replay_cancel_global_f());
	} else if ((invalidate_replay_val &
		    fb_mmu_invalidate_replay_start_ack_all_f()) != 0U) {
		/* pte valid is fixed. replay faulting request */
		err = g->ops.fb.mmu_invalidate_replay(g,
			fb_mmu_invalidate_replay_start_ack_all_f());
	}

	return err;
}

u32 gv11b_fb_get_replay_cancel_global_val(void)
{
	return fb_mmu_invalidate_replay_cancel_global_f();
}

u32 gv11b_fb_get_replay_start_ack_all(void)
{
	return fb_mmu_invalidate_replay_start_ack_all_f();
}

int gv11b_fb_mmu_invalidate_replay(struct gk20a *g,
			 u32 invalidate_replay_val)
{
	int err = -ETIMEDOUT;
	u32 reg_val;
	struct nvgpu_timeout timeout;

	nvgpu_log_fn(g, " ");

	nvgpu_mutex_acquire(&g->mm.tlb_lock);

	reg_val = nvgpu_readl(g, fb_mmu_invalidate_r());

	reg_val |= fb_mmu_invalidate_all_va_true_f() |
		fb_mmu_invalidate_all_pdb_true_f() |
		invalidate_replay_val |
		fb_mmu_invalidate_trigger_true_f();

	nvgpu_writel(g, fb_mmu_invalidate_r(), reg_val);

	nvgpu_timeout_init_retry(g, &timeout, 200U);

	do {
		reg_val = nvgpu_readl(g, fb_mmu_ctrl_r());
		if (fb_mmu_ctrl_pri_fifo_empty_v(reg_val) !=
			fb_mmu_ctrl_pri_fifo_empty_false_f()) {
			err = 0;
			break;
		}
		nvgpu_udelay(5);
	} while (nvgpu_timeout_expired_msg(&timeout,
			"invalidate replay failed 0x%x",
			invalidate_replay_val) == 0);
	if (err != 0) {
		nvgpu_err(g, "invalidate replay timedout");
	}

	nvgpu_mutex_release(&g->mm.tlb_lock);

	return err;
}
#endif
