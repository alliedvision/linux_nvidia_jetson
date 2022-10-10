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

#include <nvgpu/log.h>
#include <nvgpu/types.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/mmu_fault.h>

#include "hal/fb/fb_mmu_fault_gv11b.h"
#include "hal/mm/mmu_fault/mmu_fault_gv11b.h"

#include "fb_mmu_fault_ga10b.h"

#include "nvgpu/hw/ga10b/hw_fb_ga10b.h"

void ga10b_fb_handle_mmu_fault(struct gk20a *g, u32 intr_unit_bitmask)
{
	u32 fault_status = g->ops.fb.read_mmu_fault_status(g);

	nvgpu_log(g, gpu_dbg_intr, "mmu_fault_status = 0x%08x", fault_status);

	if ((intr_unit_bitmask & BIT32(NVGPU_CIC_INTR_UNIT_MMU_INFO_FAULT)) != 0) {

		gv11b_fb_handle_dropped_mmu_fault(g, fault_status);
		gv11b_mm_mmu_fault_handle_other_fault_notify(g, fault_status);
	}

#if defined(CONFIG_NVGPU_HAL_NON_FUSA)
	if ((fault_status & fb_mmu_fault_status_vab_error_m()) != 0U) {
		if (g->ops.fb.vab.recover != NULL) {
			g->ops.fb.vab.recover(g);
		}
	}
#endif

	if (gv11b_fb_is_fault_buf_enabled(g,
			NVGPU_MMU_FAULT_NONREPLAY_REG_INDX)) {
		if ((intr_unit_bitmask &
			BIT32(NVGPU_CIC_INTR_UNIT_MMU_NON_REPLAYABLE_FAULT)) != 0) {

			gv11b_mm_mmu_fault_handle_nonreplay_replay_fault(g,
					fault_status,
					NVGPU_MMU_FAULT_NONREPLAY_REG_INDX);

			/*
			 * When all the faults are processed,
			 * GET and PUT will have same value and mmu fault status
			 * bit will be reset by HW
			 */
		}

		if ((intr_unit_bitmask &
			BIT32(NVGPU_CIC_INTR_UNIT_MMU_NON_REPLAYABLE_FAULT_ERROR)) != 0) {

			gv11b_fb_handle_nonreplay_fault_overflow(g,
				 fault_status);
		}
	}

#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	if (gv11b_fb_is_fault_buf_enabled(g,
			NVGPU_MMU_FAULT_REPLAY_REG_INDX)) {
		if ((intr_unit_bitmask &
			BIT32(NVGPU_CIC_INTR_UNIT_MMU_REPLAYABLE_FAULT)) != 0) {

			gv11b_mm_mmu_fault_handle_nonreplay_replay_fault(g,
					fault_status,
					NVGPU_MMU_FAULT_REPLAY_REG_INDX);
		}

		if ((intr_unit_bitmask &
			BIT32(NVGPU_CIC_INTR_UNIT_MMU_REPLAYABLE_FAULT_ERROR)) != 0) {

			gv11b_fb_handle_replay_fault_overflow(g,
				 fault_status);
		}
	}
#endif

	nvgpu_log(g, gpu_dbg_intr, "clear mmu fault status");
	g->ops.fb.write_mmu_fault_status(g,
				fb_mmu_fault_status_valid_clear_f());
}
