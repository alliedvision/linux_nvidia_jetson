/*
 * GK20A Master Control
 *
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/device.h>
#include <nvgpu/mc.h>

int nvgpu_mc_reset_units(struct gk20a *g, u32 units)
{
	int err;

	err = g->ops.mc.enable_units(g, units, false);
	if (err != 0) {
		nvgpu_log(g, gpu_dbg_info, "Unit disable failed");
		return err;
	}

	err = g->ops.mc.enable_units(g, units, true);
	if (err != 0) {
		nvgpu_log(g, gpu_dbg_info, "Unit disable failed");
		return err;
	}
	return 0;
}

int nvgpu_mc_reset_dev(struct gk20a *g, const struct nvgpu_device *dev)
{
	int err = 0;

	if (g->ops.mc.enable_dev == NULL) {
		goto fail;
	}

	err = g->ops.mc.enable_dev(g, dev, false);
	if (err != 0) {
		nvgpu_device_dump_dev(g, dev);
		goto fail;
	}

	err = g->ops.mc.enable_dev(g, dev, true);
	if (err != 0) {
		nvgpu_device_dump_dev(g, dev);
		goto fail;
	}

fail:
	return err;
}

int nvgpu_mc_reset_devtype(struct gk20a *g, u32 devtype)
{
	int err;

	err = g->ops.mc.enable_devtype(g, devtype, false);
	if (err != 0) {
		nvgpu_log(g, gpu_dbg_info, "Devtype:%u disable failed",
			devtype);
		return err;
	}

	err = g->ops.mc.enable_devtype(g, devtype, true);
	if (err != 0) {
		nvgpu_log(g, gpu_dbg_info, "Devtype:%u enable failed",
			devtype);
		return err;
	}
	return 0;
}

