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

#ifndef NVGPU_GR_CONFIG_GM20B_H
#define NVGPU_GR_CONFIG_GM20B_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_gr_config;

u32 gm20b_gr_config_get_gpc_tpc_mask(struct gk20a *g,
	struct nvgpu_gr_config *config, u32 gpc_index);
u32 gm20b_gr_config_get_tpc_count_in_gpc(struct gk20a *g,
	struct nvgpu_gr_config *config, u32 gpc_index);
u32 gm20b_gr_config_get_pes_tpc_mask(struct gk20a *g,
	struct nvgpu_gr_config *config, u32 gpc_index, u32 pes_index);
u32 gm20b_gr_config_get_pd_dist_skip_table_size(void);
u32 gm20b_gr_config_get_gpc_mask(struct gk20a *g);
#if defined(CONFIG_NVGPU_HAL_NON_FUSA)
int gm20b_gr_config_init_sm_id_table(struct gk20a *g,
	struct nvgpu_gr_config *gr_config);
#endif /* CONFIG_NVGPU_HAL_NON_FUSA */
#ifdef CONFIG_NVGPU_GRAPHICS
u32 gm20b_gr_config_get_zcull_count_in_gpc(struct gk20a *g,
	struct nvgpu_gr_config *config, u32 gpc_index);
#endif /* CONFIG_NVGPU_GRAPHICS */
#endif /* NVGPU_GR_CONFIG_GM20B_H */
