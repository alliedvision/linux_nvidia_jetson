/*
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/nvgpu_err_info.h>
#include <nvgpu/cic_mon.h>

#include "cic_mon_priv.h"

void nvgpu_report_err_to_sdl(struct gk20a *g, u32 hw_unit_id, u32 err_id)
{
	int32_t err = 0;
	u32 ss_err_id = 0U;

	if (g->ops.cic_mon.report_err == NULL) {
		return;
	}

	err = nvgpu_cic_mon_bound_check_err_id(g, hw_unit_id, err_id);
	if (err != 0) {
		nvgpu_err(g, "Invalid hw_unit_id/err_id"
				"hw_unit_id = 0x%x, err_id=0x%x",
				hw_unit_id, err_id);
		goto handle_report_failure;
	}


	/**
	 * Prepare error ID that will be reported to Safety_Services.
	 * Format of the error ID:
	 * - HW_unit_id (4-bits: bit-0 to 3),
	 * - Error_id (5-bits: bit-4 to 8),
	 * - Corrected/Uncorrected error (1-bit: bit-9),
	 * - Remaining 22-bits are unused.
	 */
	hw_unit_id = hw_unit_id & HW_UNIT_ID_MASK;
	err_id = err_id & ERR_ID_MASK;
	ss_err_id =  ((err_id) << (ERR_ID_FIELD_SHIFT)) | hw_unit_id;
	if (g->cic_mon->err_lut[hw_unit_id].errs[err_id].is_critical) {
		ss_err_id =  ss_err_id |
			(U32(1) << U32(CORRECTED_BIT_FIELD_SHIFT));
	}

	if (g->ops.cic_mon.report_err(g, ss_err_id) != 0) {
		nvgpu_err(g, "Failed to report an error: "
				"hw_unit_id = 0x%x, err_id=0x%x, "
				"ss_err_id = 0x%x",
				hw_unit_id, err_id, ss_err_id);
		goto handle_report_failure;
	}

#ifndef CONFIG_NVGPU_RECOVERY
	/*
	 * Trigger SW quiesce, in case of an uncorrected error is reported
	 * to Safety_Services, in safety build.
	 */
	if (g->cic_mon->err_lut[hw_unit_id].errs[err_id].is_critical) {
		nvgpu_sw_quiesce(g);
	}
#endif

	return;

handle_report_failure:
#ifdef CONFIG_NVGPU_BUILD_CONFIGURATION_IS_SAFETY
	/*
	 * Trigger SW quiesce, in case of a SW error is encountered during
	 * error reporting to Safety_Services, in safety build.
	 */
	nvgpu_sw_quiesce(g);
#endif
	return;
}

