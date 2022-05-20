/*
 * general perf structures & definitions
 *
 * Copyright (c) 2016-2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_PERF_VFE_EQU_H
#define NVGPU_PERF_VFE_EQU_H

#include "ucode_perf_vfe_inf.h"

struct vfe_equs {
	struct boardobjgrp_e255 super;
};

struct vfe_equ {
	struct pmu_board_obj super;
	u8 var_idx;
	u8 equ_idx_next;
	u8 output_type;
	u32 out_range_min;
	u32 out_range_max;
	struct boardobjgrpmask_e32 mask_depending_vars;
	int (*mask_depending_build)(struct gk20a *g,
			struct boardobjgrp *pboardobjgrp,
			struct vfe_equ *pvfe_equ);
	bool b_is_dynamic_valid;
	bool b_is_dynamic;
};

struct vfe_equ_compare {
	struct vfe_equ super;
	u8 func_id;
	u8 equ_idx_true;
	u8 equ_idx_false;
	u32 criteria;
};

struct vfe_equ_minmax {
	struct vfe_equ super;
	bool b_max;
	u8 equ_idx0;
	u8 equ_idx1;
};

struct vfe_equ_quadratic {
	struct vfe_equ super;
	u32 coeffs[CTRL_PERF_VFE_EQU_QUADRATIC_COEFF_COUNT];
};

struct vfe_equ_scalar {
	struct vfe_equ super;
	u8 equ_idx_to_scale;
};

int perf_vfe_equ_sw_setup(struct gk20a *g);
int perf_vfe_equ_pmu_setup(struct gk20a *g);
#endif /* NVGPU_PERF_VFE_EQU_H */
