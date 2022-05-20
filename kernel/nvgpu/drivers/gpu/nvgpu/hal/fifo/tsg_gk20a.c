/*
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

#include <nvgpu/log.h>
#include <nvgpu/channel.h>
#include <nvgpu/tsg.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/runlist.h>

#include "hal/fifo/tsg_gk20a.h"

void gk20a_tsg_enable(struct nvgpu_tsg *tsg)
{
	struct gk20a *g = tsg->g;
	struct nvgpu_channel *ch;

	if (tsg->runlist == NULL) {
		/*
		 * Enabling a TSG that has no runlist (implies no channels)
		 * is just a noop.
		 */
		return;
	}

	nvgpu_runlist_set_state(g, BIT32(tsg->runlist->id),
				RUNLIST_DISABLED);

	/*
	 * Due to h/w bug that exists in Maxwell and Pascal,
	 * we first need to enable all channels with NEXT and CTX_RELOAD set,
	 * and then rest of the channels should be enabled
	 */
	nvgpu_rwsem_down_read(&tsg->ch_list_lock);
	nvgpu_list_for_each_entry(ch, &tsg->ch_list, nvgpu_channel, ch_entry) {
		struct nvgpu_channel_hw_state hw_state;

		g->ops.channel.read_state(g, ch, &hw_state);

		if (hw_state.next || hw_state.ctx_reload) {
			g->ops.channel.enable(ch);
		}
	}

	nvgpu_list_for_each_entry(ch, &tsg->ch_list, nvgpu_channel, ch_entry) {
		struct nvgpu_channel_hw_state hw_state;

		g->ops.channel.read_state(g, ch, &hw_state);

		if (hw_state.next || hw_state.ctx_reload) {
			continue;
		}

		g->ops.channel.enable(ch);
	}
	nvgpu_rwsem_up_read(&tsg->ch_list_lock);

	nvgpu_runlist_set_state(g, BIT32(tsg->runlist->id),
				RUNLIST_ENABLED);
}
