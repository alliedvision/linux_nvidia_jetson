/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <unit/unit.h>
#include <unit/io.h>
#include <nvgpu/posix/io.h>
#include <nvgpu/posix/posix-fault-injection.h>
#include <unistd.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/io.h>
#include <nvgpu/nvgpu_init.h>
#include "hal/mc/mc_gp10b.h"
#include "hal/fb/fb_gm20b.h"
#include "hal/fb/fb_gv11b.h"
#include "hal/fb/fb_mmu_fault_gv11b.h"
#include "hal/fb/intr/fb_intr_gv11b.h"
#include "hal/mm/mmu_fault/mmu_fault_gv11b.h"
#include <nvgpu/hw/gv11b/hw_fb_gv11b.h>

#include "fb_fusa.h"

#define FAULT_STATUS_TEST_VAL 0x101U
#define TEST_VALUE_1 0x80801234
#define TEST_VALUE_2 0xABCD4567

static u32 hal_channel_count(struct gk20a *g)
{
	/* Reasonable channel count for the purpose of this test */
	return 0x00000200U;
}

static int hal_bar2_bind_nop(struct gk20a *g, struct nvgpu_mem *bar2_inst)
{
	/* no-op */
	return 0;
}

static int hal_bar2_bind_fail(struct gk20a *g, struct nvgpu_mem *bar2_inst)
{
	return -1;
}

static u32 hal_fifo_mmu_fault_id_to_pbdma_id(struct gk20a *g, u32 mmu_fault_id)
{
	return INVAL_ID;
}

int fb_mmu_fault_gv11b_init_test(struct unit_module *m, struct gk20a *g,
	void *args)
{
	/* HALs under test */
	g->ops.fb.read_mmu_fault_buffer_size =
				gv11b_fb_read_mmu_fault_buffer_size;
	g->ops.fb.read_mmu_fault_buffer_get =
				gv11b_fb_read_mmu_fault_buffer_get;
	g->ops.fb.read_mmu_fault_buffer_put =
				gv11b_fb_read_mmu_fault_buffer_put;
	g->ops.fb.write_mmu_fault_buffer_get =
				fb_gv11b_write_mmu_fault_buffer_get;
	g->ops.fb.is_fault_buf_enabled = gv11b_fb_is_fault_buf_enabled;
	g->ops.fb.fault_buf_set_state_hw = gv11b_fb_fault_buf_set_state_hw;
	g->ops.fb.write_mmu_fault_buffer_size =
				gv11b_fb_write_mmu_fault_buffer_size;
	g->ops.fb.read_mmu_fault_status = gv11b_fb_read_mmu_fault_status;
	g->ops.fb.fault_buf_configure_hw = gv11b_fb_fault_buf_configure_hw;
	g->ops.fb.write_mmu_fault_buffer_lo_hi =
				gv11b_fb_write_mmu_fault_buffer_lo_hi;
	g->ops.fb.read_mmu_fault_addr_lo_hi =
				gv11b_fb_read_mmu_fault_addr_lo_hi;
	g->ops.fb.read_mmu_fault_inst_lo_hi =
				gv11b_fb_read_mmu_fault_inst_lo_hi;
	g->ops.fb.read_mmu_fault_info = gv11b_fb_read_mmu_fault_info;
	g->ops.fb.write_mmu_fault_status = gv11b_fb_write_mmu_fault_status;

	/* Other HALs that are needed */
	g->ops.channel.count = hal_channel_count;
	g->ops.bus.bar2_bind = hal_bar2_bind_nop;
	g->ops.fifo.mmu_fault_id_to_pbdma_id =
		hal_fifo_mmu_fault_id_to_pbdma_id;
	g->ops.mm.mmu_fault.parse_mmu_fault_info =
					gv11b_mm_mmu_fault_parse_mmu_fault_info;

	return UNIT_SUCCESS;
}

int fb_mmu_fault_gv11b_buffer_test(struct unit_module *m, struct gk20a *g,
	void *args)
{
	u32 get_idx;
	u32 val;
	u32 lo, hi;

	if (g->ops.fb.is_fault_buf_enabled(g, 0)) {
		unit_return_fail(m, "fault buffer not disabled as expected\n");
	}

	/* Standard case */
	gv11b_fb_fault_buffer_get_ptr_update(g, 0, 0);

	/* Overflow situation */
	nvgpu_writel(g, fb_mmu_fault_buffer_get_r(0),
		fb_mmu_fault_buffer_get_overflow_m());
	gv11b_fb_fault_buffer_get_ptr_update(g, 0, 0);

	gv11b_fb_fault_buffer_size_val(g, 0);
	if (!gv11b_fb_is_fault_buffer_empty(g, 0, &get_idx)) {
		unit_return_fail(m, "fault buffer not empty as expected\n");
	}

	/* Fault buffer hw setup */
	g->ops.fb.fault_buf_configure_hw(g, 0);

	/* Enable fault buffer */
	g->ops.fb.fault_buf_set_state_hw(g, 0, NVGPU_MMU_FAULT_BUF_ENABLED);

	/* Enabling again shouldn't cause an issue */
	g->ops.fb.fault_buf_set_state_hw(g, 0, NVGPU_MMU_FAULT_BUF_ENABLED);

	/* Disable */
	g->ops.fb.fault_buf_set_state_hw(g, 0, NVGPU_MMU_FAULT_BUF_DISABLED);

	/* Try to disable again, but cause a timeout as fault status is set */
	g->ops.fb.fault_buf_set_state_hw(g, 0, NVGPU_MMU_FAULT_BUF_ENABLED);
	nvgpu_writel(g, fb_mmu_fault_status_r(),
		fb_mmu_fault_status_busy_true_f());
	g->ops.fb.fault_buf_set_state_hw(g, 0, NVGPU_MMU_FAULT_BUF_DISABLED);
	if (g->ops.fb.is_fault_buf_enabled(g, 0)) {
		unit_return_fail(m, "fault buffer not disabled as expected\n");
	}

	nvgpu_writel(g, fb_mmu_fault_addr_lo_r(), TEST_VALUE_1);
	nvgpu_writel(g, fb_mmu_fault_addr_hi_r(), TEST_VALUE_2);
	g->ops.fb.read_mmu_fault_addr_lo_hi(g, &lo, &hi);
	if ((lo != TEST_VALUE_1) || (hi != TEST_VALUE_2)) {
		unit_return_fail(m, "Invalid MMU fault address\n");
	}

	nvgpu_writel(g, fb_mmu_fault_inst_lo_r(), TEST_VALUE_1);
	nvgpu_writel(g, fb_mmu_fault_inst_hi_r(), TEST_VALUE_2);
	g->ops.fb.read_mmu_fault_inst_lo_hi(g, &lo, &hi);
	if ((lo != TEST_VALUE_1) || (hi != TEST_VALUE_2)) {
		unit_return_fail(m, "Invalid MMU fault inst\n");
	}

	val = g->ops.fb.read_mmu_fault_info(g);
	if (val != nvgpu_readl(g, fb_mmu_fault_info_r())) {
		unit_return_fail(m, "invalid fb_mmu_fault_info_r value\n");
	}

	g->ops.fb.write_mmu_fault_status(g, FAULT_STATUS_TEST_VAL);
	if (nvgpu_readl(g, fb_mmu_fault_status_r()) != FAULT_STATUS_TEST_VAL) {
		unit_return_fail(m, "invalid fb_mmu_fault_status_r value\n");
	}

	return UNIT_SUCCESS;
}

int fb_mmu_fault_gv11b_snap_reg(struct unit_module *m, struct gk20a *g,
	void *args)
{
	struct mmu_fault_info mmufault;

	/* Not a valid fault, chid should just be zero'ed out by memset */
	gv11b_mm_copy_from_fault_snap_reg(g, 0, &mmufault);
	if (mmufault.chid != 0) {
		unit_return_fail(m, "chid updated for invalid fault\n");
	}

	/* Valid fault */
	gv11b_mm_copy_from_fault_snap_reg(g, fb_mmu_fault_status_valid_set_f(),
		&mmufault);
	if (mmufault.chid != NVGPU_INVALID_CHANNEL_ID) {
		unit_return_fail(m, "chid NOT updated for valid fault\n");
	}

	return UNIT_SUCCESS;
}

static bool helper_is_intr_cleared(struct gk20a *g)
{
	return (nvgpu_readl(g, fb_mmu_fault_status_r()) ==
		fb_mmu_fault_status_valid_clear_f());
}

int fb_mmu_fault_gv11b_handle_fault(struct unit_module *m, struct gk20a *g,
	void *args)
{
	u32 niso_intr;

	/* Set interrupt source as "other" and handle it */
	niso_intr = fb_niso_intr_mmu_other_fault_notify_m();
	nvgpu_writel(g, fb_mmu_fault_status_r(), 0);
	gv11b_fb_handle_mmu_fault(g, niso_intr);
	if (!helper_is_intr_cleared(g)) {
		unit_return_fail(m, "unhandled interrupt (1)\n");
	}

	/* Enable fault buffer */
	g->ops.fb.fault_buf_set_state_hw(g, 0, NVGPU_MMU_FAULT_BUF_ENABLED);

	/* Handle again for branch coverage */
	gv11b_fb_handle_mmu_fault(g, niso_intr);

	/* Set a valid dropped status and handle again */
	nvgpu_writel(g, fb_mmu_fault_status_r(),
		fb_mmu_fault_status_dropped_bar1_phys_set_f());
	gv11b_fb_handle_mmu_fault(g, niso_intr);
	if (!helper_is_intr_cleared(g)) {
		unit_return_fail(m, "unhandled interrupt (2)\n");
	}

	/* Now set interrupt source as a non-replayable fault and handle it */
	niso_intr = fb_niso_intr_mmu_nonreplayable_fault_notify_m();
	nvgpu_writel(g, fb_mmu_fault_status_r(), 0);
	gv11b_fb_handle_mmu_fault(g, niso_intr);
	if (!helper_is_intr_cleared(g)) {
		unit_return_fail(m, "unhandled interrupt (3)\n");
	}

	/* Now set source as non-replayable and overflow then handle it */
	niso_intr = fb_niso_intr_mmu_nonreplayable_fault_notify_m() |
		fb_niso_intr_mmu_nonreplayable_fault_overflow_m();
	nvgpu_writel(g, fb_mmu_fault_status_r(), 0);
	gv11b_fb_handle_mmu_fault(g, niso_intr);
	if (!helper_is_intr_cleared(g)) {
		unit_return_fail(m, "unhandled interrupt (4)\n");
	}

	/* Same case but ensure fault status register is also set properly */
	nvgpu_writel(g, fb_mmu_fault_status_r(),
		fb_mmu_fault_status_non_replayable_overflow_m());
	gv11b_fb_handle_mmu_fault(g, niso_intr);
	if (!helper_is_intr_cleared(g)) {
		unit_return_fail(m, "unhandled interrupt (5)\n");
	}

	/* Case where getptr is reported as corrupted */
	nvgpu_writel(g, fb_mmu_fault_status_r(),
		fb_mmu_fault_status_non_replayable_overflow_m() |
		fb_mmu_fault_status_non_replayable_getptr_corrupted_m());
	gv11b_fb_handle_mmu_fault(g, niso_intr);
	if (!helper_is_intr_cleared(g)) {
		unit_return_fail(m, "unhandled interrupt (6)\n");
	}

	g->ops.fb.fault_buf_set_state_hw(g, 0, NVGPU_MMU_FAULT_BUF_DISABLED);

	return UNIT_SUCCESS;
}

int fb_mmu_fault_gv11b_handle_bar2_fault(struct unit_module *m, struct gk20a *g,
	void *args)
{
	struct mmu_fault_info mmufault;
	struct nvgpu_channel refch;
	u32 fault_status = 0;
	static const char *const error_str = "test error";

	(void) memset(&mmufault, 0, sizeof(mmufault));
	(void) memset(&refch, 0, sizeof(refch));

	gv11b_fb_handle_bar2_fault(g, &mmufault, fault_status);

	/* Set the minimum mmufault struct to handle the fault */
	/* First cover some error cases */
	gv11b_fb_mmu_fault_info_dump(g, NULL);
	gv11b_fb_mmu_fault_info_dump(g, &mmufault);

	/* Now set it up properly */
	mmufault.valid = true;
	mmufault.refch = &refch;
	mmufault.fault_type_desc = error_str;
	mmufault.client_type_desc = error_str;
	mmufault.client_id_desc = error_str;

	gv11b_fb_mmu_fault_info_dump(g, &mmufault);
	fault_status = fb_mmu_fault_status_non_replayable_error_m();
	g->ops.fb.fault_buf_set_state_hw(g, 0, NVGPU_MMU_FAULT_BUF_ENABLED);
	gv11b_fb_handle_bar2_fault(g, &mmufault, fault_status);

	/* Case where g->ops.bus.bar2_bind fails */
	g->ops.bus.bar2_bind = hal_bar2_bind_fail;
	g->ops.fb.fault_buf_set_state_hw(g, 0, NVGPU_MMU_FAULT_BUF_ENABLED);
	gv11b_fb_handle_bar2_fault(g, &mmufault, fault_status);
	g->ops.bus.bar2_bind = hal_bar2_bind_nop;

	/* Case where fault buffer is not enabled */
	g->ops.fb.fault_buf_set_state_hw(g, 0, NVGPU_MMU_FAULT_BUF_DISABLED);
	gv11b_fb_handle_bar2_fault(g, &mmufault, fault_status);

	return UNIT_SUCCESS;
}
