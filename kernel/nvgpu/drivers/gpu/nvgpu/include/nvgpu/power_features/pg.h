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


#ifndef NVGPU_POWER_FEATURES_PG_H
#define NVGPU_POWER_FEATURES_PG_H

#include <nvgpu/types.h>

struct gk20a;

#ifdef CONFIG_NVGPU_POWER_PG
#define nvgpu_pg_elpg_protected_call(g, func) \
	({ \
		int err = 0; \
		err = nvgpu_pg_elpg_disable(g);\
		if (err != 0) {\
			(void)nvgpu_pg_elpg_enable(g);\
		}\
		if (err == 0) { \
			err = (func); \
			(void)nvgpu_pg_elpg_enable(g);\
		} \
		err; \
	})

#define nvgpu_pg_elpg_ms_protected_call(g, func) \
	({ \
		int status = 0; \
		status = nvgpu_pg_elpg_ms_disable(g);\
		if (status != 0) {\
			(void)nvgpu_pg_elpg_ms_enable(g);\
		} \
		if (status == 0) { \
			status = (func); \
			(void)nvgpu_pg_elpg_ms_enable(g);\
		} \
		status; \
	})
#else
#define nvgpu_pg_elpg_protected_call(g, func) func
#define nvgpu_pg_elpg_ms_protected_call(g, func) func
#endif

int nvgpu_pg_elpg_disable(struct gk20a *g);
int nvgpu_pg_elpg_enable(struct gk20a *g);
int nvgpu_pg_elpg_ms_disable(struct gk20a *g);
int nvgpu_pg_elpg_ms_enable(struct gk20a *g);
bool nvgpu_pg_elpg_is_enabled(struct gk20a *g);
int nvgpu_pg_elpg_set_elpg_enabled(struct gk20a *g, bool enable);

#endif /*NVGPU_POWER_FEATURES_PG_H*/
