/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GR_ZCULL_GM20B_H
#define NVGPU_GR_ZCULL_GM20B_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_gr_config;
struct nvgpu_gr_zcull;
struct nvgpu_gr_zcull_info;
struct nvgpu_channel;

int gm20b_gr_init_zcull_hw(struct gk20a *g,
			struct nvgpu_gr_zcull *gr_zcull,
			struct nvgpu_gr_config *gr_config);
int gm20b_gr_get_zcull_info(struct gk20a *g,
			struct nvgpu_gr_config *gr_config,
			struct nvgpu_gr_zcull *zcull,
			struct nvgpu_gr_zcull_info *zcull_params);
void gm20b_gr_program_zcull_mapping(struct gk20a *g, u32 zcull_num_entries,
					u32 *zcull_map_tiles);

#endif /* NVGPU_GR_ZCULL_GM20B_H */
