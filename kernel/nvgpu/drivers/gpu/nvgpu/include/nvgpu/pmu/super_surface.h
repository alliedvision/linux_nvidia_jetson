/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef SUPER_SURFACE_H
#define SUPER_SURFACE_H

#include <nvgpu/types.h>

struct nvgpu_mem;
struct nvgpu_pmu;
struct nvgpu_pmu_super_surface;

/*
 * Super surface member BIT identification used in member_mask indicating
 * which members in the super surface are valid.
 *
 * The ordering here is very important because it defines the order of
 * processing in the PMU and takes dependencies into consideration.
 */
#define NV_PMU_SUPER_SURFACE_MEMBER_THERM_DEVICE_GRP          0x00U
#define NV_PMU_SUPER_SURFACE_MEMBER_THERM_CHANNEL_GRP         0x01U
#define NV_PMU_SUPER_SURFACE_MEMBER_VFE_VAR_GRP               0x03U
#define NV_PMU_SUPER_SURFACE_MEMBER_VFE_EQU_GRP               0x04U
#define NV_PMU_SUPER_SURFACE_MEMBER_VOLT_DEVICE_GRP           0x0BU
#define NV_PMU_SUPER_SURFACE_MEMBER_VOLT_RAIL_GRP             0x0CU
#define NV_PMU_SUPER_SURFACE_MEMBER_VOLT_POLICY_GRP           0x0DU
#define NV_PMU_SUPER_SURFACE_MEMBER_CLK_DOMAIN_GRP            0x12U
#define NV_PMU_SUPER_SURFACE_MEMBER_CLK_PROG_GRP              0x13U
#define NV_PMU_SUPER_SURFACE_MEMBER_CLK_VIN_DEVICE_GRP        0x15U
#define NV_PMU_SUPER_SURFACE_MEMBER_CLK_FLL_DEVICE_GRP        0x16U
#define NV_PMU_SUPER_SURFACE_MEMBER_CLK_VF_POINT_GRP          0x17U
#define NV_PMU_SUPER_SURFACE_MEMBER_CLK_FREQ_CONTROLLER_GRP   0x18U
#define NV_PMU_SUPER_SURFACE_MEMBER_CLK_FREQ_DOMAIN_GRP       0x19U
#define NV_PMU_SUPER_SURFACE_MEMBER_CHANGE_SEQ_GRP            0x1EU
#define NV_PMU_SUPER_SURFACE_MEMBER_PSTATE_GRP                0x20U

#define NV_PMU_SUPER_SURFACE_MEMBER_COUNT                     0x21U

u32 nvgpu_pmu_get_ss_member_set_offset(struct gk20a *g,
	struct nvgpu_pmu *pmu, u32 member_id);
u32 nvgpu_pmu_get_ss_member_set_size(struct gk20a *g,
	struct nvgpu_pmu *pmu, u32 member_id);
u32 nvgpu_pmu_get_ss_member_get_status_offset(struct gk20a *g,
	struct nvgpu_pmu *pmu, u32 member_id);
u32 nvgpu_pmu_get_ss_member_get_status_size(struct gk20a *g,
	struct nvgpu_pmu *pmu, u32 member_id);
int nvgpu_pmu_ss_create_ssmd_lookup_table(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_super_surface *ss);
struct nvgpu_mem *nvgpu_pmu_super_surface_mem(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_super_surface *ss);
int nvgpu_pmu_super_surface_buf_alloc(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct nvgpu_pmu_super_surface *ss);

u32 nvgpu_pmu_get_ss_cmd_fbq_offset(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_super_surface *ss, u32 id);
u32 nvgpu_pmu_get_ss_msg_fbq_offset(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_super_surface *ss);
u32 nvgpu_pmu_get_ss_msg_fbq_element_offset(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_super_surface *ss, u32 idx);
void nvgpu_pmu_ss_fbq_flush(struct gk20a *g, struct nvgpu_pmu *pmu);

void nvgpu_pmu_super_surface_deinit(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct nvgpu_pmu_super_surface *ss);

int nvgpu_pmu_super_surface_init(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct nvgpu_pmu_super_surface **super_suface);

#endif /* SUPER_SURFACE_H */
