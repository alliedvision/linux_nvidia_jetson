/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/kmem.h>
#include <nvgpu/device.h>
#include <nvgpu/engines.h>

#include <nvgpu/vgpu/vgpu.h>

#include "top_vgpu.h"

/*
 * Similar to how the real HW version works, just read a device out of the vGPU
 * device list one at a time. The core device management code will manage the
 * actual device lists for us.
 */
struct nvgpu_device *vgpu_top_parse_next_dev(struct gk20a *g, u32 *token)
{
	struct vgpu_priv_data *priv = vgpu_get_priv_data(g);
	struct tegra_vgpu_engines_info *engines = &priv->constants.engines_info;
	struct nvgpu_device *dev;

	/*
	 * Check to see if we are done parsing engines.
	 */
	if (*token >= engines->num_engines) {
		return NULL;
	}

	dev = nvgpu_kzalloc(g, sizeof(*dev));
	if (!dev) {
		return NULL;
	}

	/*
	 * Copy the engine data into the device and return it to our caller.
	 */
	dev->type       = engines->info[*token].engine_enum;
	dev->engine_id  = engines->info[*token].engine_id;
	dev->intr_id    = nvgpu_ffs(engines->info[*token].intr_mask) - 1;
	dev->reset_id   = nvgpu_ffs(engines->info[*token].reset_mask) - 1;
	dev->runlist_id = engines->info[*token].runlist_id;
	dev->pbdma_id   = engines->info[*token].pbdma_id;
	dev->inst_id    = engines->info[*token].inst_id;
	dev->pri_base   = engines->info[*token].pri_base;
	dev->fault_id   = engines->info[*token].fault_id;

	(*token)++;

	return dev;
}
