/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/channel.h>
#include <nvgpu/fifo.h>

#include "hal/fifo/ramfc_gk20a.h"
#include "hal/fifo/ramfc_tu104.h"

#include <nvgpu/hw/tu104/hw_ram_tu104.h>

int tu104_ramfc_setup(struct nvgpu_channel *ch, u64 gpfifo_base,
		u32 gpfifo_entries, u64 pbdma_acquire_timeout, u32 flags)
{
	struct gk20a *g = ch->g;
	struct nvgpu_mem *mem = &ch->inst_block;
	u32 data;

	nvgpu_log_fn(g, " ");

	nvgpu_memset(g, mem, 0, 0, ram_fc_size_val_v());

	nvgpu_mem_wr32(g, mem, ram_fc_gp_base_w(),
		g->ops.pbdma.get_gp_base(gpfifo_base));

	nvgpu_mem_wr32(g, mem, ram_fc_gp_base_hi_w(),
		g->ops.pbdma.get_gp_base_hi(gpfifo_base, gpfifo_entries));

	nvgpu_mem_wr32(g, mem, ram_fc_signature_w(),
		g->ops.pbdma.get_signature(g));

	nvgpu_mem_wr32(g, mem, ram_fc_pb_header_w(),
		g->ops.pbdma.get_fc_pb_header());

	nvgpu_mem_wr32(g, mem, ram_fc_subdevice_w(),
		g->ops.pbdma.get_fc_subdevice());

	nvgpu_mem_wr32(g, mem, ram_fc_target_w(),
		g->ops.pbdma.get_fc_target(NULL));

	nvgpu_mem_wr32(g, mem, ram_fc_acquire_w(),
		g->ops.pbdma.acquire_val(pbdma_acquire_timeout));

	nvgpu_mem_wr32(g, mem, ram_fc_set_channel_info_w(),
		g->ops.pbdma.set_channel_info_veid(ch->subctx_id));

	nvgpu_mem_wr32(g, mem, ram_in_engine_wfi_veid_w(),
		ram_in_engine_wfi_veid_f(ch->subctx_id));

	if (ch->is_privileged_channel) {
		/* Set privilege level for channel */
		nvgpu_mem_wr32(g, mem, ram_fc_config_w(),
			g->ops.pbdma.get_config_auth_level_privileged());

		/* Enable HCE priv mode for phys mode transfer */
		nvgpu_mem_wr32(g, mem, ram_fc_hce_ctrl_w(),
			g->ops.pbdma.get_ctrl_hce_priv_mode_yes());
	}

	/* Enable userd writeback */
	data = nvgpu_mem_rd32(g, mem, ram_fc_config_w());
	data = g->ops.pbdma.config_userd_writeback_enable(data);
	nvgpu_mem_wr32(g, mem, ram_fc_config_w(), data);

	return g->ops.ramfc.commit_userd(ch);
}
