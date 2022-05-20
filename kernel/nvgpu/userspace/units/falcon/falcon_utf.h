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

#ifndef __FALCON_UTF_H__
#define __FALCON_UTF_H__

#include <nvgpu/posix/types.h>
#include <nvgpu/posix/io.h>

#define UTF_FALCON_MAX_REG_OFFSET	0x400
#define UTF_FALCON_IMEM_DMEM_SIZE	(127 * 1024)

struct gk20a;
struct nvgpu_falcon;

struct utf_falcon {
	struct nvgpu_falcon *flcn;
	u32 *imem;
	u32 *dmem;
};

struct nvgpu_posix_fault_inj *nvgpu_utf_falcon_memcpy_get_fault_injection(void);

void nvgpu_utf_falcon_writel_access_reg_fn(struct gk20a *g,
					   struct utf_falcon *flcn,
					   struct nvgpu_reg_access *access);
void nvgpu_utf_falcon_readl_access_reg_fn(struct gk20a *g,
					  struct utf_falcon *flcn,
					  struct nvgpu_reg_access *access);
struct utf_falcon *nvgpu_utf_falcon_init(struct unit_module *m,
					 struct gk20a *g, u32 flcn_id);
void nvgpu_utf_falcon_free(struct gk20a *g, struct utf_falcon *utf_flcn);
void nvgpu_utf_falcon_set_dmactl(struct gk20a *g, struct utf_falcon *utf_flcn,
				 u32 reg_data);

#endif
