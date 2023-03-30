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
#include <nvgpu/channel.h>

#include "hal/fifo/tsg_gk20a.h"

int gk20a_tsg_unbind_channel_check_hw_next(struct nvgpu_channel *ch,
		struct nvgpu_channel_hw_state *hw_state)
{
	if (hw_state->next) {
		/*
		 * There is a possibility that the user sees the channel
		 * has finished all the work and invokes channel removal
		 * before the scheduler marks it idle (clears NEXT bit).
		 * Scheduler can miss marking the channel idle if the
		 * timeslice expires just after the work finishes.
		 *
		 * nvgpu will then see NEXT bit set even though the
		 * channel has no work left. To catch this case,
		 * reenable the tsg and check the hw state again
		 * to see if the channel is truly idle.
		 */
		nvgpu_log_info(ch->g, "Channel %d to be removed "
			"from TSG %d has NEXT set!",
			ch->chid, ch->tsgid);
		return -EAGAIN;
	}

	return 0;
}
