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

#ifndef NVGPU_CLK_VIN_H
#define NVGPU_CLK_VIN_H

#include <nvgpu/boardobjgrp.h>
#include <nvgpu/boardobjgrp_e32.h>
#include <common/pmu/boardobj/boardobj.h>

typedef u32 vin_device_state_load(struct gk20a *g,
		struct nvgpu_clk_pmupstate *clk, struct clk_vin_device *pdev);

struct clk_vin_device {
	struct pmu_board_obj super;
	u8 id;
	u8 volt_domain;
	u8 volt_domain_vbios;
	u8 por_override_mode;
	u8 override_mode;
	u32 flls_shared_mask;
	vin_device_state_load  *state_load;
};

struct vin_device_v20 {
	struct clk_vin_device super;
	struct ctrl_clk_vin_device_info_data_v20 data;
};
struct nvgpu_avfsvinobjs {
	struct boardobjgrp_e32 super;
	u8 calibration_rev_vbios;
	u8 calibration_rev_fused;
	u8 version;
	bool vin_is_disable_allowed;
};
int clk_vin_init_pmupstate(struct gk20a *g);
void clk_vin_free_pmupstate(struct gk20a *g);
int clk_pmu_vin_load(struct gk20a *g);
int clk_vin_sw_setup(struct gk20a *g);
int clk_vin_pmu_setup(struct gk20a *g);
#endif /* NVGPU_CLK_VIN_H */
