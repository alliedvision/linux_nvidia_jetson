/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_GOPS_FLOORSWEEP_H
#define NVGPU_GOPS_FLOORSWEEP_H

#ifdef CONFIG_NVGPU_STATIC_POWERGATE
struct gops_tpc_pg {
	int (*init_tpc_pg)(struct gk20a *g, bool *can_tpc_pg);
	void (*tpc_pg)(struct gk20a *g);
};

struct gops_fbp_pg {
	int (*init_fbp_pg)(struct gk20a *g, bool *can_fbp_pg);
	void (*fbp_pg)(struct gk20a *g);
};

struct gops_gpc_pg {
	int (*init_gpc_pg)(struct gk20a *g, bool *can_gpc_pg);
	void (*gpc_pg)(struct gk20a *g);
};

#endif

#endif /* NVGPU_GOPS_FLOORSWEEP_H */
