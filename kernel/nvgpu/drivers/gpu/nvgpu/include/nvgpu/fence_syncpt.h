/*
 * Copyright (c) 2014-2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_FENCE_SYNCPT_H
#define NVGPU_FENCE_SYNCPT_H

#ifdef CONFIG_TEGRA_GK20A_NVHOST

#include <nvgpu/types.h>
#include <nvgpu/os_fence.h>

struct nvgpu_fence_type;
struct nvgpu_nvhost_dev;

void nvgpu_fence_from_syncpt(
		struct nvgpu_fence_type *f,
		struct nvgpu_nvhost_dev *nvhost_device,
		u32 id, u32 value,
		struct nvgpu_os_fence os_fence);

#endif /* CONFIG_TEGRA_GK20A_NVHOST */

#endif
