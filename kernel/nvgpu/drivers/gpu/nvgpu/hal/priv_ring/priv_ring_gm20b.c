/*
 * GM20B priv ring
 *
 * Copyright (c) 2011-2019, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/log.h>
#include <nvgpu/timers.h>
#include <nvgpu/enabled.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/power_features/cg.h>

#include "priv_ring_gm20b.h"

#include <nvgpu/hw/gm20b/hw_pri_ringmaster_gm20b.h>
#include <nvgpu/hw/gm20b/hw_pri_ringstation_sys_gm20b.h>
#include <nvgpu/hw/gm20b/hw_pri_ringstation_gpc_gm20b.h>

void gm20b_priv_ring_isr(struct gk20a *g)
{
	u32 status0, status1;
	u32 cmd;
	s32 retry;
	u32 gpc;
	u32 gpc_priv_stride;
	u32 gpc_offset;

#ifdef CONFIG_NVGPU_SIM
	if (nvgpu_is_enabled(g, NVGPU_IS_FMODEL)) {
		nvgpu_err(g, "unhandled priv ring intr");
		return;
	}
#endif

	status0 = nvgpu_readl(g, pri_ringmaster_intr_status0_r());
	status1 = nvgpu_readl(g, pri_ringmaster_intr_status1_r());

	nvgpu_log(g, gpu_dbg_intr, "ringmaster intr status0: 0x%08x,"
		"status1: 0x%08x", status0, status1);

	if (pri_ringmaster_intr_status0_gbl_write_error_sys_v(status0) != 0U) {
		nvgpu_log(g, gpu_dbg_intr, "SYS write error. ADR %08x "
			"WRDAT %08x INFO %08x, CODE %08x",
			nvgpu_readl(g, pri_ringstation_sys_priv_error_adr_r()),
			nvgpu_readl(g, pri_ringstation_sys_priv_error_wrdat_r()),
			nvgpu_readl(g, pri_ringstation_sys_priv_error_info_r()),
			nvgpu_readl(g, pri_ringstation_sys_priv_error_code_r()));
	}

	gpc_priv_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_PRIV_STRIDE);

	for (gpc = 0; gpc < g->ops.priv_ring.get_gpc_count(g); gpc++) {
		if ((status1 & BIT32(gpc)) == 0U) {
			continue;
		}
		gpc_offset = gpc * gpc_priv_stride;
		nvgpu_log(g, gpu_dbg_intr, "GPC%u write error. ADR %08x "
			"WRDAT %08x INFO %08x, CODE %08x", gpc,
			nvgpu_readl(g, pri_ringstation_gpc_gpc0_priv_error_adr_r() + gpc_offset),
			nvgpu_readl(g, pri_ringstation_gpc_gpc0_priv_error_wrdat_r() + gpc_offset),
			nvgpu_readl(g, pri_ringstation_gpc_gpc0_priv_error_info_r() + gpc_offset),
			nvgpu_readl(g, pri_ringstation_gpc_gpc0_priv_error_code_r() + gpc_offset));
	}
	/* clear interrupt */
	cmd = nvgpu_readl(g, pri_ringmaster_command_r());
	cmd = set_field(cmd, pri_ringmaster_command_cmd_m(),
		pri_ringmaster_command_cmd_ack_interrupt_f());
	nvgpu_writel(g, pri_ringmaster_command_r(), cmd);

	/* poll for clear interrupt done */
	retry = GM20B_PRIV_RING_POLL_CLEAR_INTR_RETRIES;

	cmd = pri_ringmaster_command_cmd_v(
		nvgpu_readl(g, pri_ringmaster_command_r()));
	while ((cmd != pri_ringmaster_command_cmd_no_cmd_v()) && (retry != 0)) {
		nvgpu_udelay(GM20B_PRIV_RING_POLL_CLEAR_INTR_UDELAY);
		retry--;
		cmd = pri_ringmaster_command_cmd_v(
			nvgpu_readl(g, pri_ringmaster_command_r()));
	}
	if (retry == 0 && cmd != pri_ringmaster_command_cmd_no_cmd_v()) {
		nvgpu_warn(g, "priv ringmaster intr ack too many retries");
	}
}
