/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/engines.h>
#include <nvgpu/runlist.h>

#include "hal/fifo/ramfc_ga10b.h"

#include <nvgpu/hw/ga10b/hw_ram_ga10b.h>

int ga10b_ramfc_setup(struct nvgpu_channel *ch, u64 gpfifo_base,
		u32 gpfifo_entries, u64 pbdma_acquire_timeout, u32 flags)
{
	struct gk20a *g = ch->g;
	struct nvgpu_mem *mem = &ch->inst_block;
	u32 data;
	u32 engine_id = 0U;
	u32 eng_intr_mask = 0U;
	u32 eng_intr_vector = 0U;
	u32 eng_bitmask = 0U;
	bool replayable = false;

	(void)flags;

	nvgpu_log_fn(g, " ");

	/*
	 * ga10b can have max 3 engines on a runlist and only
	 * runlist 0 has more than 1 engine(gr0, grcopy0 and grcopy1).
	 * Since grcopy0 and grcopy1 can't schedule work directly, it
	 * is always safe to assume that first active engine on runlist
	 * will trigger pbdma intr notify.
	 * TODO: Add helper function to get active engine mask for
	 * runlist - NVGPU-5219
	 */
	eng_bitmask = ch->runlist->eng_bitmask;
	engine_id = nvgpu_safe_sub_u32(
		nvgpu_safe_cast_u64_to_u32(nvgpu_ffs(eng_bitmask)), 1U);

	nvgpu_memset(g, mem, 0U, 0U, ram_fc_size_val_v());

#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	if ((flags & NVGPU_SETUP_BIND_FLAGS_REPLAYABLE_FAULTS_ENABLE) != 0U) {
		replayable = true;
	}
#endif

	nvgpu_log_info(g, "%llu %u", pbdma_acquire_timeout,
		g->ops.pbdma.acquire_val(pbdma_acquire_timeout));

	g->ops.ramin.init_subctx_pdb(g, mem, ch->vm->pdb.mem,
		replayable, nvgpu_channel_get_max_subctx_count(ch));

	nvgpu_mem_wr32(g, mem, ram_fc_gp_base_w(),
		g->ops.pbdma.get_gp_base(gpfifo_base));

	nvgpu_mem_wr32(g, mem, ram_fc_gp_base_hi_w(),
		g->ops.pbdma.get_gp_base_hi(gpfifo_base, gpfifo_entries));

	nvgpu_mem_wr32(g, mem, ram_fc_signature_w(),
		ch->g->ops.pbdma.get_signature(ch->g));

	nvgpu_mem_wr32(g, mem, ram_fc_pb_header_w(),
		g->ops.pbdma.get_fc_pb_header());

	nvgpu_mem_wr32(g, mem, ram_fc_subdevice_w(),
		g->ops.pbdma.get_fc_subdevice());

	nvgpu_mem_wr32(g, mem, ram_fc_target_w(),
		g->ops.pbdma.get_fc_target(
			nvgpu_engine_get_active_eng_info(g, engine_id)));

	nvgpu_mem_wr32(g, mem, ram_fc_acquire_w(),
		g->ops.pbdma.acquire_val(pbdma_acquire_timeout));

	data = nvgpu_mem_rd32(g, mem, ram_fc_set_channel_info_w());

	data = data | (g->ops.pbdma.set_channel_info_veid(ch->subctx_id) |
		   g->ops.pbdma.set_channel_info_chid(ch->chid));
	nvgpu_mem_wr32(g, mem, ram_fc_set_channel_info_w(), data);
	nvgpu_mem_wr32(g, mem, ram_in_engine_wfi_veid_w(),
		ram_in_engine_wfi_veid_f(ch->subctx_id));

	/* get engine interrupt vector */
	eng_intr_mask = nvgpu_engine_act_interrupt_mask(g, engine_id);
	eng_intr_vector = nvgpu_safe_sub_u32(
		nvgpu_safe_cast_u64_to_u32(nvgpu_ffs(eng_intr_mask)), 1U);

	/*
	 * engine_intr_vector can be value between 0 and 255.
	 * For example, engine_intr_vector x translates to subtree x/64,
	 * leaf (x % 64)/32 and leaf entry interrupt bit(x % 64)%32.
	 * ga10b engine_intr_vectors are 0,1,2,3,4,5. They map to
	 * subtree_0 and leaf_0(Engine non-stall interrupts) interrupt
	 * bits.
	 */
	data = g->ops.pbdma.set_intr_notify(eng_intr_vector);
	nvgpu_mem_wr32(g, mem, ram_fc_intr_notify_w(), data);

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

	return 0;
}

void ga10b_ramfc_capture_ram_dump(struct gk20a *g, struct nvgpu_channel *ch,
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
	info->inst.pb_header = nvgpu_mem_rd32(g, mem,
			ram_fc_pb_header_w());
	info->inst.pb_count = nvgpu_mem_rd32(g, mem,
			ram_fc_pb_count_w());
	info->inst.sem_addr = nvgpu_mem_rd32_pair(g, mem,
			ram_fc_sem_addr_lo_w(),
			ram_fc_sem_addr_hi_w());
	info->inst.sem_payload = nvgpu_mem_rd32_pair(g, mem,
			ram_fc_sem_payload_lo_w(),
			ram_fc_sem_payload_hi_w());
	info->inst.sem_execute = nvgpu_mem_rd32(g, mem,
			ram_fc_sem_execute_w());
}
