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

#include <nvgpu/gk20a.h>
#include <nvgpu/kmem.h>
#include <nvgpu/log.h>
#include <nvgpu/cic_mon.h>

#include "cic_mon_priv.h"

int nvgpu_cic_mon_setup(struct gk20a *g)
{
	struct nvgpu_cic_mon *cic_mon;
	int err = 0;

	if (g->cic_mon != NULL) {
		cic_dbg(g, "CIC_MON already initialized");
		return 0;
	}

	cic_mon = nvgpu_kzalloc(g, sizeof(*cic_mon));
	if (cic_mon == NULL) {
		nvgpu_err(g, "Failed to allocate memory "
				"for struct nvgpu_cic_mon");
		return -ENOMEM;
	}

	g->cic_mon = cic_mon;
	cic_dbg(g, "CIC_MON unit initialization done.");
	return err;
}

int nvgpu_cic_mon_init_lut(struct gk20a *g)
{
	struct nvgpu_cic_mon *cic_mon;
	int err = 0;

	cic_mon = g->cic_mon;
	if (cic_mon == NULL) {
		nvgpu_err(g, "CIC_MON setup pending");
		return -EINVAL;
	}

	if (g->ops.cic_mon.init != NULL) {
		err = g->ops.cic_mon.init(g, cic_mon);
		if (err != 0) {
			nvgpu_err(g, "CIC MON chip specific "
					"initialization failed.");
			goto cleanup;
		}
	} else {
		cic_mon->err_lut = NULL;
		cic_mon->num_hw_modules = 0;
	}

	return 0;
cleanup:
	if (cic_mon != NULL) {
		nvgpu_kfree(g, cic_mon);
		g->cic_mon = NULL;
	}
	return err;
}

int nvgpu_cic_mon_remove(struct gk20a *g)
{
	struct nvgpu_cic_mon *cic_mon;

	cic_mon = g->cic_mon;

	if (cic_mon == NULL) {
		cic_dbg(g, "CIC_MON already removed");
		return 0;
	}

	nvgpu_kfree(g, cic_mon);
	g->cic_mon = NULL;

	return 0;
}

int nvgpu_cic_mon_deinit_lut(struct gk20a *g)
{
	struct nvgpu_cic_mon *cic_mon;

	cic_mon = g->cic_mon;

	if (cic_mon == NULL) {
		cic_dbg(g, "CIC_MON and LUT removed");
		return 0;
	}

	cic_mon->err_lut = NULL;
	cic_mon->num_hw_modules = 0;

	return 0;
}

int nvgpu_cic_mon_deinit(struct gk20a *g)
{
	/** More deinit calls might get added here as CIC grows. */
	return nvgpu_cic_mon_deinit_lut(g);
}
