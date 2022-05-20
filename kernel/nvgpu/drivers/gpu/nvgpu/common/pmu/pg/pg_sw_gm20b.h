/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_PMU_PG_SW_GM20B_H
#define NVGPU_PMU_PG_SW_GM20B_H

#include <nvgpu/types.h>

struct gk20a;
struct pmu_pg_stats_data;

#define ZBC_MASK(i)			U16(~(~(0U) << ((i)+1U)) & 0xfffeU)

u32 gm20b_pmu_pg_engines_list(struct gk20a *g);
u32 gm20b_pmu_pg_feature_list(struct gk20a *g, u32 pg_engine_id);
void gm20b_pmu_save_zbc(struct gk20a *g, u32 entries);
int gm20b_pmu_elpg_statistics(struct gk20a *g, u32 pg_engine_id,
	struct pmu_pg_stats_data *pg_stat_data);
void nvgpu_gm20b_pg_sw_init(struct gk20a *g,
		struct nvgpu_pmu_pg *pg);
int gm20b_pmu_pg_elpg_allow(struct gk20a *g, struct nvgpu_pmu *pmu,
		u8 pg_engine_id);
int gm20b_pmu_pg_elpg_disallow(struct gk20a *g, struct nvgpu_pmu *pmu,
		u8 pg_engine_id);
int gm20b_pmu_pg_elpg_init(struct gk20a *g, struct nvgpu_pmu *pmu,
		u8 pg_engine_id);
int gm20b_pmu_pg_elpg_alloc_dmem(struct gk20a *g, struct nvgpu_pmu *pmu,
		u8 pg_engine_id);
int gm20b_pmu_pg_elpg_load_buff(struct gk20a *g, struct nvgpu_pmu *pmu);
int gm20b_pmu_pg_elpg_hw_load_zbc(struct gk20a *g, struct nvgpu_pmu *pmu);
int gm20b_pmu_pg_init_send(struct gk20a *g, struct nvgpu_pmu *pmu,
		u8 pg_engine_id);

#endif /* NVGPU_PMU_PG_SW_GM20B_H */
