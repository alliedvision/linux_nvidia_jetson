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

#ifndef NVGPU_PMU_VOLT_H
#define NVGPU_PMU_VOLT_H

#include <nvgpu/types.h>
#include <nvgpu/pmu/msg.h>

struct gk20a;
struct nvgpu_pmu_volt_metadata;

#define CTRL_VOLT_DOMAIN_LOGIC			0x01U
#define CLK_PROG_VFE_ENTRY_LOGIC		0x00U
#define CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS	0x04U

struct nvgpu_pmu_volt {
	struct nvgpu_pmu_volt_metadata *volt_metadata;
	void (*volt_rpc_handler)(struct gk20a *g,
			struct nv_pmu_rpc_header *rpc);
};

u8 nvgpu_pmu_volt_rail_volt_domain_convert_to_idx(struct gk20a *g, u8 volt_domain);
int nvgpu_pmu_volt_get_vmin_vmax_ps35(struct gk20a *g, u32 *vmin_uv, u32 *vmax_uv);
u8 nvgpu_pmu_volt_get_vmargin_ps35(struct gk20a *g);
int nvgpu_pmu_volt_get_curr_volt_ps35(struct gk20a *g, u32 *vcurr_uv);
int nvgpu_pmu_volt_sw_setup(struct gk20a *g);
int nvgpu_pmu_volt_pmu_setup(struct gk20a *g);
void nvgpu_pmu_volt_deinit(struct gk20a *g);
int nvgpu_pmu_volt_init(struct gk20a *g);

#endif /* NVGPU_PMU_VOLT_H */
