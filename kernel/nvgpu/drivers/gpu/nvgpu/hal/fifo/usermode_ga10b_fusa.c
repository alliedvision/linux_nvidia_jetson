/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/types.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/io.h>
#include <nvgpu/runlist.h>

#include "usermode_ga10b.h"

#include "hal/fifo/fifo_utils_ga10b.h"

#include <nvgpu/hw/ga10b/hw_runlist_ga10b.h>

#define GFID_INSTANCE_0		0U

/*
 * nvgpu_fifo.max_runlists:
 *      - Maximum runlists supported by hardware.
 * nvgpu_fifo.num_runlists:
 *      - Number of valid runlists detected during device info parsing and
 *        connected to a valid engine.
 * nvgpu_fifo.runlists[]:
 *      - This is an array of pointers to nvgpu_runlist_info structure.
 *      - This is indexed by hardware runlist_id from 0 to max_runlists.
 * nvgpu_fifo.active_runlists[]:
 *      - This is an array of nvgpu_runlist_info structure.
 *      - This is indexed by software [consecutive] runlist_ids from 0 to
 *        num_runlists.
 *
 * runlists[] pointers at valid runlist_id indices contain valid
 * nvgpu_runlist structures. runlist[] pointers at invalid runlist_id
 * indexes point to NULL. This is explained in the example below.
 *
 * for example: max_runlists = 10, num_runlists = 4
 *              say valid runlist_ids are = {0, 2, 3, 7}
 *
 *         runlist_info                           active_runlists
 *      0 ________________                  0 ___________________________
 *       |________________|----------------->|___________________________|
 *       |________________|   |------------->|___________________________|
 *       |________________|---|  |---------->|___________________________|
 *       |________________|------|  |------->|___________________________|
 *       |________________|         |    num_runlists
 *       |________________|         |
 *       |________________|         |
 *       |________________|---------|
 *       |________________|
 *       |________________|
 *  max_runlists
 *
 */

void ga10b_usermode_setup_hw(struct gk20a *g)
{
	struct nvgpu_runlist *runlist = NULL;
	u32 reg_val = 0U;
	u32 i = 0U;
	u32 max_runlist = g->ops.runlist.count_max(g);

	for (i = 0U; i < max_runlist; i++) {
		runlist = g->fifo.runlists[i];
		if (runlist == NULL) {
			continue;
		}

		/*
		 * At this moment, we are not supporting multiple GFIDs.
		 * Only GFID 0 is supported and passed to
		 * runlist_virtual_channel_cfg_r()
		 */
		reg_val = nvgpu_runlist_readl(g, runlist,
				runlist_virtual_channel_cfg_r(GFID_INSTANCE_0));
		reg_val |= runlist_virtual_channel_cfg_mask_hw_mask_hw_init_f();
		reg_val |= runlist_virtual_channel_cfg_pending_enable_true_f();

		nvgpu_runlist_writel(g, runlist,
			runlist_virtual_channel_cfg_r(GFID_INSTANCE_0),
			reg_val);
	}
}
