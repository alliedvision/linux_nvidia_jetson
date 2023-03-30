/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/log.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/nvgpu_err_info.h>

#include "cic_mon_priv.h"

int nvgpu_cic_mon_bound_check_hw_unit_id(struct gk20a *g, u32 hw_unit_id)
{
	if (g->cic_mon == NULL) {
		nvgpu_err(g, "CIC is not initialized");
		return -EINVAL;
	}

	if (g->cic_mon->num_hw_modules == 0U) {
		cic_dbg(g, "LUT not initialized.");
		return -EINVAL;
	}

	if (hw_unit_id >= g->cic_mon->num_hw_modules) {
		cic_dbg(g, "Invalid input HW unit ID.");
		return -EINVAL;
	}

	return 0;
}

int nvgpu_cic_mon_bound_check_err_id(struct gk20a *g, u32 hw_unit_id,
		u32 err_id)
{
	int err = 0;

	if ((g->cic_mon == NULL) || (g->cic_mon->err_lut == NULL)) {
		cic_dbg(g, "CIC/LUT not initialized.");
		return -EINVAL;
	}

	err = nvgpu_cic_mon_bound_check_hw_unit_id(g, hw_unit_id);
	if (err != 0) {
		return err;
	}

	if (err_id >= g->cic_mon->err_lut[hw_unit_id].num_errs) {
		err = -EINVAL;
	}

	return err;
}

int nvgpu_cic_mon_get_err_desc(struct gk20a *g, u32 hw_unit_id,
		u32 err_id, struct nvgpu_err_desc **err_desc)
{
	int err = 0;

	/* if (g->cic_mon != NULL) and (g->cic_mon->err_lut != NULL) check
	 * can be skipped here as it checked as part of
	 * nvgpu_cic_mon_bound_check_err_id() called below.
	 */

	err = nvgpu_cic_mon_bound_check_err_id(g, hw_unit_id, err_id);
	if (err != 0) {
		return err;
	}

	*err_desc = &(g->cic_mon->err_lut[hw_unit_id].errs[err_id]);

	return err;
}

int nvgpu_cic_mon_get_num_hw_modules(struct gk20a *g)
{
	if (g->cic_mon == NULL) {
		nvgpu_err(g, "CIC is not initialized");
		return -EINVAL;
	}

	return (int)g->cic_mon->num_hw_modules;
}
