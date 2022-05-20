/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/engines.h>
#include <nvgpu/runlist.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/tsg.h>
#include <nvgpu/dma.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/static_analysis.h>

#include "hal/fifo/tsg_gv11b.h"


/* can be removed after runque support is added */
#define GR_RUNQUE			0U	/* pbdma 0 */
#define ASYNC_CE_RUNQUE			2U	/* pbdma 2 */

/* TSG enable sequence applicable for Volta and onwards */
void gv11b_tsg_enable(struct nvgpu_tsg *tsg)
{
	struct gk20a *g = tsg->g;
	struct nvgpu_channel *ch;
	struct nvgpu_channel *last_ch = NULL;

	nvgpu_rwsem_down_read(&tsg->ch_list_lock);
	nvgpu_list_for_each_entry(ch, &tsg->ch_list, nvgpu_channel, ch_entry) {
		g->ops.channel.enable(ch);
		last_ch = ch;
	}
	nvgpu_rwsem_up_read(&tsg->ch_list_lock);

	if (last_ch != NULL) {
		g->ops.usermode.ring_doorbell(last_ch);
	}
}

void gv11b_tsg_unbind_channel_check_eng_faulted(struct nvgpu_tsg *tsg,
		struct nvgpu_channel *ch,
		struct nvgpu_channel_hw_state *hw_state)
{
	struct gk20a *g = tsg->g;
	struct nvgpu_mem *mem;

	/*
	 * If channel has FAULTED set, clear the CE method buffer
	 * if saved out channel is same as faulted channel
	 */
	if (!hw_state->eng_faulted || (tsg->eng_method_buffers == NULL)) {
		return;
	}

	/*
	 * CE method buffer format :
	 * DWord0 = method count
	 * DWord1 = channel id
	 *
	 * It is sufficient to write 0 to method count to invalidate
	 */
	mem = &tsg->eng_method_buffers[ASYNC_CE_RUNQUE];
	if (ch->chid == nvgpu_mem_rd32(g, mem, 1)) {
		nvgpu_mem_wr32(g, mem, 0, 0);
	}
}

void gv11b_tsg_bind_channel_eng_method_buffers(struct nvgpu_tsg *tsg,
		struct nvgpu_channel *ch)
{
	struct gk20a *g = tsg->g;
	u64 gpu_va;

	nvgpu_assert(tsg->runlist != NULL);

	if (tsg->eng_method_buffers == NULL) {
		nvgpu_log_info(g, "eng method buffer NULL");
		return;
	}

	if (tsg->runlist->id == nvgpu_engine_get_fast_ce_runlist_id(g)) {
		gpu_va = tsg->eng_method_buffers[ASYNC_CE_RUNQUE].gpu_va;
	} else {
		gpu_va = tsg->eng_method_buffers[GR_RUNQUE].gpu_va;
	}

	g->ops.ramin.set_eng_method_buffer(g, &ch->inst_block, gpu_va);
}

int gv11b_tsg_init_eng_method_buffers(struct gk20a *g, struct nvgpu_tsg *tsg)
{
	struct vm_gk20a *vm = g->mm.bar2.vm;
	int err = 0;
	int i;
	unsigned int runque, buffer_size;
	u32 page_size = U32(NVGPU_CPU_PAGE_SIZE);
	unsigned int num_pbdma = nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_PBDMA);

	if (tsg->eng_method_buffers != NULL) {
		nvgpu_warn(g, "eng method buffers already allocated");
		return 0;
	}

	buffer_size =  nvgpu_safe_add_u32(nvgpu_safe_mult_u32((9U + 1U + 3U),
				g->ops.ce.get_num_pce(g)), 2U);
	buffer_size = nvgpu_safe_mult_u32((27U * 5U), buffer_size);
	buffer_size = round_up(buffer_size, page_size);
	nvgpu_log_info(g, "method buffer size in bytes %d", buffer_size);

	tsg->eng_method_buffers = nvgpu_kzalloc(g,
			num_pbdma * sizeof(struct nvgpu_mem));
	if (tsg->eng_method_buffers == NULL) {
		nvgpu_err(g, "could not alloc eng method buffers");
		return -ENOMEM;
	}

	for (runque = 0; runque < num_pbdma; runque++) {
		err = nvgpu_dma_alloc_map_sys(vm, buffer_size,
					&tsg->eng_method_buffers[runque]);
		if (err != 0) {
			nvgpu_err(g, "alloc eng method buffers, runque=%d",
					runque);
			goto clean_up;
		}
	}

	nvgpu_log_info(g, "eng method buffers allocated");
	return 0;

clean_up:
	for (i = ((int)runque - 1); i >= 0; i--) {
		nvgpu_dma_unmap_free(vm, &tsg->eng_method_buffers[i]);
	}
	nvgpu_kfree(g, tsg->eng_method_buffers);
	tsg->eng_method_buffers = NULL;
	return -ENOMEM;
}

void gv11b_tsg_deinit_eng_method_buffers(struct gk20a *g,
		struct nvgpu_tsg *tsg)
{
	struct vm_gk20a *vm = g->mm.bar2.vm;
	unsigned int runque;

	if (tsg->eng_method_buffers == NULL) {
		return;
	}

	for (runque = 0;
	     runque < nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_PBDMA);
	     runque++) {
		nvgpu_dma_unmap_free(vm, &tsg->eng_method_buffers[runque]);
	}

	nvgpu_kfree(g, tsg->eng_method_buffers);
	tsg->eng_method_buffers = NULL;

	nvgpu_log_info(g, "eng method buffers de-allocated");
}
