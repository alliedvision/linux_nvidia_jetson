/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_GOPS_CBC_H
#define NVGPU_GOPS_CBC_H

#ifdef CONFIG_NVGPU_COMPRESSION
struct gops_cbc {
	int (*cbc_init_support)(struct gk20a *g);
	void (*cbc_remove_support)(struct gk20a *g);
	void (*init)(struct gk20a *g, struct nvgpu_cbc *cbc, bool is_resume);
	int (*alloc_comptags)(struct gk20a *g,
				struct nvgpu_cbc *cbc);
	int (*ctrl)(struct gk20a *g, enum nvgpu_cbc_op op,
			u32 min, u32 max);
	u32 (*fix_config)(struct gk20a *g, int base);
	bool (*use_contig_pool)(struct gk20a *g);
};
#endif

#endif /* NVGPU_GOPS_CBC_H */
