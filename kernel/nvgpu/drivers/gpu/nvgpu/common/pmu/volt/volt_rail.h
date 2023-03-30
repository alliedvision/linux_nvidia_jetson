/*
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


#ifndef NVGPU_VOLT_RAIL_H
#define NVGPU_VOLT_RAIL_H

#include <nvgpu/boardobjgrp.h>
#include <common/pmu/boardobj/boardobj.h>

#define CTRL_PMGR_PWR_EQUATION_INDEX_INVALID	0xFFU

struct voltage_rail {
	struct pmu_board_obj super;
	u32 boot_voltage_uv;
	u8 rel_limit_vfe_equ_idx;
	u8 alt_rel_limit_vfe_equ_idx;
	u8 ov_limit_vfe_equ_idx;
	u8 pwr_equ_idx;
	u8 volt_scale_exp_pwr_equ_idx;
	u8 volt_dev_idx_default;
	u8 volt_dev_idx_ipc_vmin;
	u8 boot_volt_vfe_equ_idx;
	u8 vmin_limit_vfe_equ_idx;
	u8 volt_margin_limit_vfe_equ_idx;
	u32 volt_margin_limit_vfe_equ_mon_handle;
	u32 rel_limit_vfe_equ_mon_handle;
	u32 alt_rel_limit_vfe_equ_mon_handle;
	u32 ov_limit_vfe_equ_mon_handle;
	struct boardobjgrpmask_e32 volt_dev_mask;
	s32  volt_delta_uv[CTRL_VOLT_RAIL_VOLT_DELTA_MAX_ENTRIES];
	u32 vmin_limitu_v;
	u32 max_limitu_v;
	u32 current_volt_uv;
};

int volt_rail_volt_dev_register(struct gk20a *g, struct voltage_rail
	*pvolt_rail, u8 volt_dev_idx, u8 operation_type);
int volt_rail_sw_setup(struct gk20a *g);
int volt_rail_pmu_setup(struct gk20a *g);

#endif /* NVGPU_VOLT_RAIL_H */
