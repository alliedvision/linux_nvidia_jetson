/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_MC_GA10B_H
#define NVGPU_MC_GA10B_H

#include <nvgpu/types.h>

#define MC_UNIT_RESET_DELAY_US		20U
#define MC_ENGINE_RESET_DELAY_US	500U

struct gk20a;

int ga10b_mc_enable_units(struct gk20a *g, u32 units, bool enable);
int ga10b_mc_enable_dev(struct gk20a *g, const struct nvgpu_device *dev,
			bool enable);
int ga10b_mc_enable_devtype(struct gk20a *g, u32 devtype, bool enable);
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
void ga10b_mc_elpg_enable(struct gk20a *g);
#endif

#endif /* NVGPU_MC_GA10B_H */
