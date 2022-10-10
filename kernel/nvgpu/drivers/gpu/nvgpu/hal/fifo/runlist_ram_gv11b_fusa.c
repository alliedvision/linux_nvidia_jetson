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

#include <nvgpu/channel.h>
#include <nvgpu/runlist.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/static_analysis.h>

#include "runlist_ram_gv11b.h"

#include <nvgpu/hw/gv11b/hw_ram_gv11b.h>

#define RL_MAX_TIMESLICE_TIMEOUT ram_rl_entry_tsg_timeslice_timeout_v(U32_MAX)
#define RL_MAX_TIMESLICE_SCALE ram_rl_entry_tsg_timeslice_scale_v(U32_MAX)

u32 gv11b_runlist_entry_size(struct gk20a *g)
{
	(void)g;
	return ram_rl_entry_size_v();
}

u32 gv11b_runlist_max_timeslice(void)
{
	return ((RL_MAX_TIMESLICE_TIMEOUT << RL_MAX_TIMESLICE_SCALE) / 1000) * 1024;
}

void gv11b_runlist_get_tsg_entry(struct nvgpu_tsg *tsg,
		u32 *runlist, u32 timeslice)
{
	struct gk20a *g = tsg->g;
	u32 timeout = timeslice;
	u32 scale = 0U;

	WARN_ON(timeslice == 0U);

	while (timeout > RL_MAX_TIMESLICE_TIMEOUT) {
		timeout >>= 1U;
		scale = nvgpu_safe_add_u32(scale, 1U);
	}

	if (scale > RL_MAX_TIMESLICE_SCALE) {
		nvgpu_err(g, "requested timeslice value is clamped");
		timeout = RL_MAX_TIMESLICE_TIMEOUT;
		scale = RL_MAX_TIMESLICE_SCALE;
	}

	runlist[0] = ram_rl_entry_type_tsg_v() |
			ram_rl_entry_tsg_timeslice_scale_f(scale) |
			ram_rl_entry_tsg_timeslice_timeout_f(timeout);
	runlist[1] = ram_rl_entry_tsg_length_f(tsg->num_active_channels);
	runlist[2] = ram_rl_entry_tsg_tsgid_f(tsg->tsgid);
	runlist[3] = 0;

	nvgpu_log_info(g, "gv11b tsg runlist [0] %x [1]  %x [2] %x [3] %x",
		runlist[0], runlist[1], runlist[2], runlist[3]);

}

void gv11b_runlist_get_ch_entry(struct nvgpu_channel *ch, u32 *runlist)
{
	struct gk20a *g = ch->g;
	u32 addr_lo, addr_hi;
	u32 runlist_entry;

	/* Time being use 0 pbdma sequencer */
	runlist_entry = ram_rl_entry_type_channel_v() |
		ram_rl_entry_chan_runqueue_selector_f(ch->runqueue_sel) |
		ram_rl_entry_chan_userd_target_f(
			nvgpu_aperture_mask(g, ch->userd_mem,
				ram_rl_entry_chan_userd_target_sys_mem_ncoh_v(),
				ram_rl_entry_chan_userd_target_sys_mem_coh_v(),
				ram_rl_entry_chan_userd_target_vid_mem_v())) |
		ram_rl_entry_chan_inst_target_f(
			nvgpu_aperture_mask(g, &ch->inst_block,
				ram_rl_entry_chan_inst_target_sys_mem_ncoh_v(),
				ram_rl_entry_chan_inst_target_sys_mem_coh_v(),
				ram_rl_entry_chan_inst_target_vid_mem_v()));

	addr_lo = u64_lo32(ch->userd_iova) >>
			ram_rl_entry_chan_userd_ptr_align_shift_v();
	addr_hi = u64_hi32(ch->userd_iova);
	runlist[0] = runlist_entry | ram_rl_entry_chan_userd_ptr_lo_f(addr_lo);
	runlist[1] = ram_rl_entry_chan_userd_ptr_hi_f(addr_hi);

	addr_lo = u64_lo32(nvgpu_inst_block_addr(g, &ch->inst_block)) >>
			ram_rl_entry_chan_inst_ptr_align_shift_v();
	addr_hi = u64_hi32(nvgpu_inst_block_addr(g, &ch->inst_block));

	runlist[2] = ram_rl_entry_chan_inst_ptr_lo_f(addr_lo) |
				ram_rl_entry_chid_f(ch->chid);
	runlist[3] = ram_rl_entry_chan_inst_ptr_hi_f(addr_hi);

	nvgpu_log_info(g, "gv11b channel runlist [0] %x [1]  %x [2] %x [3] %x",
			runlist[0], runlist[1], runlist[2], runlist[3]);
}

u32 gv11b_runlist_get_max_channels_per_tsg(void)
{
	return ram_rl_entry_tsg_length_max_v();
}
