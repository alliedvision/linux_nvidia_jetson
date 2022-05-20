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

#include <nvgpu/device.h>
#include <nvgpu/engines.h>
#include <nvgpu/log.h>
#include <nvgpu/errno.h>
#include <nvgpu/gk20a.h>

#include <nvgpu/hw/gm20b/hw_fifo_gm20b.h>

#include "engines_gm20b.h"

bool gm20b_is_fault_engine_subid_gpc(struct gk20a *g, u32 engine_subid)
{
	return (engine_subid == fifo_intr_mmu_fault_info_engine_subid_gpc_v());
}

int gm20b_engine_init_ce_info(struct nvgpu_fifo *f)
{
	struct gk20a *g = f->g;
	u32 i;
	bool found;

	for (i = NVGPU_DEVTYPE_COPY0;  i <= NVGPU_DEVTYPE_COPY2; i++) {
		const struct nvgpu_device *dev;
		struct nvgpu_device *dev_rw;

		dev = (struct nvgpu_device *)nvgpu_device_get(g, i,
							      i - NVGPU_DEVTYPE_COPY0);
		if (dev == NULL) {
			/*
			 * Not an error condition; gm20b has only 1 CE.
			 */
			continue;
		}

		/*
		 * Cast to a non-const version since we have to hack up a few fields for
		 * SW to work.
		 */
		dev_rw = (struct nvgpu_device *)dev;

		found = g->ops.fifo.find_pbdma_for_runlist(g,
							   dev->runlist_id,
							   &dev_rw->pbdma_id);
		if (!found) {
			nvgpu_err(g, "busted pbdma map");
			return -EINVAL;
		}

		f->host_engines[dev->engine_id] = dev;
		f->active_engines[f->num_engines] = dev;
		++f->num_engines;
	}

	return 0;
}
