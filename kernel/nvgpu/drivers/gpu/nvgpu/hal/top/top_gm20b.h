/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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
 *
 * GM20B TOP UNIT
 */

#ifndef TOP_GM20B_H
#define TOP_GM20B_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_device;

void gm20b_device_info_parse_enum(struct gk20a *g, u32 table_entry,
					u32 *engine_id, u32 *runlist_id,
					u32 *intr_id, u32 *reset_id);
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
int gm20b_device_info_parse_data(struct gk20a *g, u32 table_entry, u32 *inst_id,
					u32 *pri_base, u32 *fault_id);

u32 gm20b_get_ce_inst_id(struct gk20a *g, u32 engine_type);
#endif

struct nvgpu_device *gm20b_top_parse_next_dev(struct gk20a *g, u32 *i);

u32 gm20b_top_get_max_gpc_count(struct gk20a *g);
u32 gm20b_top_get_max_tpc_per_gpc_count(struct gk20a *g);

u32 gm20b_top_get_max_fbps_count(struct gk20a *g);
u32 gm20b_top_get_max_ltc_per_fbp(struct gk20a *g);
u32 gm20b_top_get_max_lts_per_ltc(struct gk20a *g);
u32 gm20b_top_get_num_ltcs(struct gk20a *g);

#endif
