/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GR_ZBC_H
#define NVGPU_GR_ZBC_H

#include <nvgpu/types.h>


#define NVGPU_GR_ZBC_COLOR_VALUE_SIZE	4U  /* RGBA */

/* index zero reserved to indicate "not ZBCd" */
#define NVGPU_GR_ZBC_STARTOF_TABLE	1U

#define NVGPU_GR_ZBC_TYPE_INVALID	0U
#define NVGPU_GR_ZBC_TYPE_COLOR		1U
#define NVGPU_GR_ZBC_TYPE_DEPTH		2U
#define NVGPU_GR_ZBC_TYPE_STENCIL	3U

struct gk20a;
struct zbc_color_table;
struct zbc_depth_table;
struct zbc_s_table;
struct nvgpu_gr_zbc_entry;
struct nvgpu_gr_zbc;

struct nvgpu_gr_zbc_table_indices {
	u32 min_color_index;
	u32 min_depth_index;
	u32 min_stencil_index;
	u32 max_color_index;
	u32 max_depth_index;
	u32 max_stencil_index;
};

struct nvgpu_gr_zbc_query_params {
	u32 color_ds[NVGPU_GR_ZBC_COLOR_VALUE_SIZE];
	u32 color_l2[NVGPU_GR_ZBC_COLOR_VALUE_SIZE];
	u32 depth;
	u32 stencil;
	u32 ref_cnt;
	u32 format;
	u32 type;	/* color or depth */
	u32 index_size;	/* [out] size, [in] index */
};

int nvgpu_gr_zbc_init(struct gk20a *g, struct nvgpu_gr_zbc **zbc);
void nvgpu_gr_zbc_deinit(struct gk20a *g, struct nvgpu_gr_zbc *zbc);
void nvgpu_gr_zbc_load_table(struct gk20a *g, struct nvgpu_gr_zbc *zbc);
int nvgpu_gr_zbc_query_table(struct gk20a *g, struct nvgpu_gr_zbc *zbc,
			     struct nvgpu_gr_zbc_query_params *query_params);
int nvgpu_gr_zbc_set_table(struct gk20a *g, struct nvgpu_gr_zbc *zbc,
			   struct nvgpu_gr_zbc_entry *zbc_val);

struct nvgpu_gr_zbc_entry *nvgpu_gr_zbc_entry_alloc(struct gk20a *g);
void nvgpu_gr_zbc_entry_free(struct gk20a *g, struct nvgpu_gr_zbc_entry *entry);
u32 nvgpu_gr_zbc_get_entry_color_ds(struct nvgpu_gr_zbc_entry *entry, int idx);
void nvgpu_gr_zbc_set_entry_color_ds(struct nvgpu_gr_zbc_entry *entry,
		int idx, u32 ds);
u32 nvgpu_gr_zbc_get_entry_color_l2(struct nvgpu_gr_zbc_entry *entry, int idx);
void nvgpu_gr_zbc_set_entry_color_l2(struct nvgpu_gr_zbc_entry *entry,
		int idx, u32 l2);
u32 nvgpu_gr_zbc_get_entry_depth(struct nvgpu_gr_zbc_entry *entry);
void nvgpu_gr_zbc_set_entry_depth(struct nvgpu_gr_zbc_entry *entry, u32 depth);
u32 nvgpu_gr_zbc_get_entry_stencil(struct nvgpu_gr_zbc_entry *entry);
void nvgpu_gr_zbc_set_entry_stencil(struct nvgpu_gr_zbc_entry *entry,
		u32 stencil);
u32 nvgpu_gr_zbc_get_entry_type(struct nvgpu_gr_zbc_entry *entry);
void nvgpu_gr_zbc_set_entry_type(struct nvgpu_gr_zbc_entry *entry, u32 type);
u32 nvgpu_gr_zbc_get_entry_format(struct nvgpu_gr_zbc_entry *entry);
void nvgpu_gr_zbc_set_entry_format(struct nvgpu_gr_zbc_entry *entry,
		u32 format);

#endif /* NVGPU_GR_ZBC_H */
