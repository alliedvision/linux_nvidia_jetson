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

#ifndef NVGPU_CLK_H
#define NVGPU_CLK_H

#include <nvgpu/boardobjgrp_e255.h>
#include "ucode_clk_inf.h"

#define CTRL_CLK_FLL_REGIME_ID_INVALID			((u8)0x00000000)
#define CTRL_CLK_FLL_REGIME_ID_FFR			((u8)0x00000001)
#define CTRL_CLK_FLL_REGIME_ID_FR			((u8)0x00000002)

#define CTRL_CLK_FLL_LUT_VSELECT_LOGIC			(0x00000000U)
#define CTRL_CLK_FLL_LUT_VSELECT_MIN			(0x00000001U)
#define CTRL_CLK_FLL_LUT_VSELECT_SRAM			(0x00000002U)

#define CTRL_CLK_VIN_SW_OVERRIDE_VIN_USE_HW_REQ		(0x00000000U)
#define CTRL_CLK_VIN_SW_OVERRIDE_VIN_USE_MIN		(0x00000001U)
#define CTRL_CLK_VIN_SW_OVERRIDE_VIN_USE_SW_REQ		(0x00000003U)

#define CTRL_CLK_VIN_STEP_SIZE_UV			(6250U)
#define CTRL_CLK_LUT_MIN_VOLTAGE_UV			(450000U)
#define CTRL_CLK_FLL_TYPE_DISABLED			(0U)

struct nvgpu_clk_pmupstate {
	struct nvgpu_avfsvinobjs *avfs_vinobjs;
	struct clk_avfs_fll_objs *avfs_fllobjs;
	struct nvgpu_clk_domains *clk_domainobjs;
	struct nvgpu_clk_progs *clk_progobjs;
	struct nvgpu_clk_vf_points *clk_vf_pointobjs;
};

struct clk_vf_point {
	struct pmu_board_obj super;
	u8  vfe_equ_idx;
	u8  volt_rail_idx;
	struct ctrl_clk_vf_pair pair;
};

struct clk_vf_point_volt {
	struct clk_vf_point super;
	u32 source_voltage_uv;
	struct ctrl_clk_freq_delta freq_delta;
};

struct clk_vf_point_freq {
	struct clk_vf_point super;
	int volt_delta_uv;
};

struct nvgpu_clk_vf_points {
	struct boardobjgrp_e255 super;
};

struct clk_vf_point *nvgpu_construct_clk_vf_point(struct gk20a *g,
	void *pargs);

u32 nvgpu_pmu_clk_fll_get_lut_min_volt(struct nvgpu_clk_pmupstate *pclk);
u8 clk_get_fll_lut_vf_num_entries(struct nvgpu_clk_pmupstate *pclk);
struct clk_vin_device *clk_get_vin_from_index(
		struct nvgpu_avfsvinobjs *pvinobjs, u8 idx);
int clk_domain_clk_prog_link(struct gk20a *g,
		struct nvgpu_clk_pmupstate *pclk);
#endif /* NVGPU_CLK_VIN_H */
