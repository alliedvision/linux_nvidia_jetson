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

#ifndef NVGPU_GR_HWPM_MAP_H
#define NVGPU_GR_HWPM_MAP_H

#ifdef CONFIG_NVGPU_DEBUGGER

#include <nvgpu/types.h>

struct gk20a;
struct ctxsw_buf_offset_map_entry;
struct nvgpu_gr_config;

struct ctxsw_buf_offset_map_entry {
	u32 addr;	/* Register address */
	u32 offset;	/* Offset in ctxt switch buffer */
};

struct nvgpu_gr_hwpm_map {
	u32 pm_ctxsw_image_size;

	u32 count;
	struct ctxsw_buf_offset_map_entry *map;

	bool init;
};

int nvgpu_gr_hwpm_map_init(struct gk20a *g, struct nvgpu_gr_hwpm_map **hwpm_map,
	u32 size);
void nvgpu_gr_hwpm_map_deinit(struct gk20a *g,
	struct nvgpu_gr_hwpm_map *hwpm_map);

u32 nvgpu_gr_hwpm_map_get_size(struct nvgpu_gr_hwpm_map *hwpm_map);

int nvgpu_gr_hwmp_map_find_priv_offset(struct gk20a *g,
	struct nvgpu_gr_hwpm_map *hwpm_map,
	u32 addr, u32 *priv_offset, struct nvgpu_gr_config *config);

#endif /* CONFIG_NVGPU_DEBUGGER */
#endif /* NVGPU_GR_HWPM_MAP_H */
