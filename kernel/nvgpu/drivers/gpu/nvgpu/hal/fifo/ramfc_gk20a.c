/*
 * Copyright (c) 2011-2020, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/log2.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/channel.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/io.h>

#include "ramfc_gk20a.h"

#include <nvgpu/hw/gk20a/hw_ram_gk20a.h>

int gk20a_ramfc_commit_userd(struct nvgpu_channel *ch)
{
	u32 addr_lo;
	u32 addr_hi;
	struct gk20a *g = ch->g;

	nvgpu_log_fn(g, " ");

	addr_lo = u64_lo32(ch->userd_iova >> ram_userd_base_shift_v());
	addr_hi = u64_hi32(ch->userd_iova);

	nvgpu_log_info(g, "channel %d : set ramfc userd 0x%16llx",
		ch->chid, (u64)ch->userd_iova);

	nvgpu_mem_wr32(g, &ch->inst_block,
		ram_in_ramfc_w() + ram_fc_userd_w(),
		g->ops.pbdma.get_userd_aperture_mask(g, ch->userd_mem) |
		g->ops.pbdma.get_userd_addr(addr_lo));

	nvgpu_mem_wr32(g, &ch->inst_block,
		ram_in_ramfc_w() + ram_fc_userd_hi_w(),
		g->ops.pbdma.get_userd_hi_addr(addr_hi));

	return 0;
}

int gk20a_ramfc_setup(struct nvgpu_channel *ch, u64 gpfifo_base,
		u32 gpfifo_entries, u64 pbdma_acquire_timeout, u32 flags)
{
	struct gk20a *g = ch->g;
	struct nvgpu_mem *mem = &ch->inst_block;

	nvgpu_log_fn(g, " ");

	nvgpu_memset(g, mem, 0, 0, ram_fc_size_val_v());

	nvgpu_mem_wr32(g, mem, ram_fc_gp_base_w(),
		g->ops.pbdma.get_gp_base(gpfifo_base));

	nvgpu_mem_wr32(g, mem, ram_fc_gp_base_hi_w(),
		g->ops.pbdma.get_gp_base_hi(gpfifo_base, gpfifo_entries));

	nvgpu_mem_wr32(g, mem, ram_fc_signature_w(),
		 ch->g->ops.pbdma.get_signature(ch->g));

	nvgpu_mem_wr32(g, mem, ram_fc_formats_w(),
		g->ops.pbdma.get_fc_formats());

	nvgpu_mem_wr32(g, mem, ram_fc_pb_header_w(),
		g->ops.pbdma.get_fc_pb_header());

	nvgpu_mem_wr32(g, mem, ram_fc_subdevice_w(),
		g->ops.pbdma.get_fc_subdevice());

	nvgpu_mem_wr32(g, mem, ram_fc_target_w(),
		g->ops.pbdma.get_fc_target(NULL));

	nvgpu_mem_wr32(g, mem, ram_fc_acquire_w(),
		g->ops.pbdma.acquire_val(pbdma_acquire_timeout));

	nvgpu_mem_wr32(g, mem, ram_fc_runlist_timeslice_w(),
		g->ops.fifo.get_runlist_timeslice(g));

	nvgpu_mem_wr32(g, mem, ram_fc_pb_timeslice_w(),
		g->ops.fifo.get_pb_timeslice(g));

	nvgpu_mem_wr32(g, mem, ram_fc_chid_w(), ram_fc_chid_id_f(ch->chid));

	if (ch->is_privileged_channel) {
		/* Enable HCE priv mode for phys mode transfer */
		nvgpu_mem_wr32(g, mem, ram_fc_hce_ctrl_w(),
		g->ops.pbdma.get_ctrl_hce_priv_mode_yes());
	}

	return g->ops.ramfc.commit_userd(ch);
}

void gk20a_ramfc_capture_ram_dump(struct gk20a *g, struct nvgpu_channel *ch,
		struct nvgpu_channel_dump_info *info)
{
	struct nvgpu_mem *mem = &ch->inst_block;

	info->inst.pb_top_level_get = nvgpu_mem_rd32_pair(g, mem,
			ram_fc_pb_top_level_get_w(),
			ram_fc_pb_top_level_get_hi_w());
	info->inst.pb_put = nvgpu_mem_rd32_pair(g, mem,
			ram_fc_pb_put_w(),
			ram_fc_pb_put_hi_w());
	info->inst.pb_get = nvgpu_mem_rd32_pair(g, mem,
			ram_fc_pb_get_w(),
			ram_fc_pb_get_hi_w());
	info->inst.pb_fetch = nvgpu_mem_rd32_pair(g, mem,
			ram_fc_pb_fetch_w(),
			ram_fc_pb_fetch_hi_w());
	info->inst.pb_header = nvgpu_mem_rd32(g, mem,
			ram_fc_pb_header_w());
	info->inst.pb_count = nvgpu_mem_rd32(g, mem,
			ram_fc_pb_count_w());
	info->inst.syncpointa = nvgpu_mem_rd32(g, mem,
			ram_fc_syncpointa_w());
	info->inst.syncpointb = nvgpu_mem_rd32(g, mem,
			ram_fc_syncpointb_w());
	info->inst.semaphorea = nvgpu_mem_rd32(g, mem,
			ram_fc_semaphorea_w());
	info->inst.semaphoreb = nvgpu_mem_rd32(g, mem,
			ram_fc_semaphoreb_w());
	info->inst.semaphorec = nvgpu_mem_rd32(g, mem,
			ram_fc_semaphorec_w());
	info->inst.semaphored = nvgpu_mem_rd32(g, mem,
			ram_fc_semaphored_w());
}
