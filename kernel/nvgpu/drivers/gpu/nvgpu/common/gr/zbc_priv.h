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

#ifndef NVGPU_GR_ZBC_PRIV_H
#define NVGPU_GR_ZBC_PRIV_H

#include <nvgpu/gr/zbc.h>

/* Opaque black (i.e. solid black, fmt 0x28 = A8B8G8R8) */
#define GR_ZBC_SOLID_BLACK_COLOR_FMT		0x28
/* Transparent black = (fmt 1 = zero) */
#define GR_ZBC_TRANSPARENT_BLACK_COLOR_FMT	0x1
/* Opaque white (i.e. solid white) = (fmt 2 = uniform 1) */
#define GR_ZBC_SOLID_WHITE_COLOR_FMT		0x2
/* z format with fp32 */
#define GR_ZBC_Z_FMT_VAL_FP32			0x1

#define GR_ZBC_STENCIL_CLEAR_FMT_INVAILD	0U
#define GR_ZBC_STENCIL_CLEAR_FMT_U8		1U

struct zbc_color_table {
	u32 color_ds[NVGPU_GR_ZBC_COLOR_VALUE_SIZE];
	u32 color_l2[NVGPU_GR_ZBC_COLOR_VALUE_SIZE];
	u32 format;
	u32 ref_cnt;
};

struct zbc_depth_table {
	u32 depth;
	u32 format;
	u32 ref_cnt;
};

struct zbc_stencil_table {
	u32 stencil;
	u32 format;
	u32 ref_cnt;
};

struct nvgpu_gr_zbc_entry {
	u32 color_ds[NVGPU_GR_ZBC_COLOR_VALUE_SIZE];
	u32 color_l2[NVGPU_GR_ZBC_COLOR_VALUE_SIZE];
	u32 depth;
	u32 stencil;
	u32 type;
	u32 format;
};

/*
 * HW ZBC table valid entries start at index 1.
 * Entry 0 is reserved to mean "no matching entry found, do not use ZBC"
 */
struct nvgpu_gr_zbc {
	struct nvgpu_mutex zbc_lock;	/* Lock to access zbc table */
	struct zbc_color_table *zbc_col_tbl; /* SW zbc color table pointer */
	struct zbc_depth_table *zbc_dep_tbl; /* SW zbc depth table pointer */
	struct zbc_stencil_table *zbc_s_tbl; /* SW zbc stencil table pointer */
	u32 min_color_index;	/* Minimum valid color table index */
	u32 min_depth_index;	/* Minimum valid depth table index */
	u32 min_stencil_index;	/* Minimum valid stencil table index */
	u32 max_color_index;	/* Maximum valid color table index */
	u32 max_depth_index;	/* Maximum valid depth table index */
	u32 max_stencil_index;	/* Maximum valid stencil table index */
	u32 max_used_color_index; /* Max used color table index */
	u32 max_used_depth_index; /* Max used depth table index */
	u32 max_used_stencil_index; /* Max used stencil table index */
};

#endif /* NVGPU_GR_ZBC_PRIV_H */

