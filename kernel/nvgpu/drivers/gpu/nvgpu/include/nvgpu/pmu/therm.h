/*
 * general thermal pmu control structures & definitions
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
#ifndef NVGPU_PMU_THERM_H
#define NVGPU_PMU_THERM_H

struct gk20a;
struct nvgpu_pmu;

int nvgpu_pmu_therm_sw_setup(struct gk20a *g, struct nvgpu_pmu *pmu);
int nvgpu_pmu_therm_pmu_setup(struct gk20a *g, struct nvgpu_pmu *pmu);
int nvgpu_pmu_therm_init(struct gk20a *g, struct nvgpu_pmu *pmu);
void nvgpu_pmu_therm_deinit(struct gk20a *g, struct nvgpu_pmu *pmu);
int nvgpu_pmu_therm_channel_get_curr_temp(struct gk20a *g, u32 *temp);

#endif /* NVGPU_PMU_THREM_H */
