/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef LSFM_H
#define LSFM_H

struct gk20a;
struct nvgpu_pmu_lsfm;

struct nvgpu_pmu_lsfm {
	bool is_wpr_init_done;
	u32 loaded_falcon_id;
	int (*init_wpr_region)(struct gk20a *g, struct nvgpu_pmu *pmu);
	int (*bootstrap_ls_falcon)(struct gk20a *g,
		struct nvgpu_pmu *pmu, struct nvgpu_pmu_lsfm *lsfm,
		u32 falcon_id_mask);
	int (*ls_pmu_cmdline_args_copy)(struct gk20a *g, struct nvgpu_pmu *pmu);
};

int nvgpu_pmu_lsfm_bootstrap_ls_falcon(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_lsfm *lsfm, u32 falcon_id_mask);
int nvgpu_pmu_lsfm_ls_pmu_cmdline_args_copy(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_lsfm *lsfm);
void nvgpu_pmu_lsfm_rpc_handler(struct gk20a *g,
	struct rpc_handler_payload *rpc_payload);
int nvgpu_pmu_lsfm_init(struct gk20a *g, struct nvgpu_pmu_lsfm **lsfm);
void nvgpu_pmu_lsfm_clean(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct nvgpu_pmu_lsfm *lsfm);
void nvgpu_pmu_lsfm_deinit(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct nvgpu_pmu_lsfm *lsfm);

#endif /*LSFM_H*/
