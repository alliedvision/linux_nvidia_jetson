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
#ifndef NVGPU_FENCE_H
#define NVGPU_FENCE_H

#include <nvgpu/types.h>
#include <nvgpu/kref.h>
#include <nvgpu/os_fence.h>

struct gk20a;
struct nvgpu_channel;
struct platform_device;
#ifdef CONFIG_NVGPU_SW_SEMAPHORE
struct nvgpu_semaphore;
#endif
struct nvgpu_os_fence;
struct nvgpu_user_fence;
struct nvgpu_fence_ops;

struct nvgpu_fence_type_priv {
	/* Valid for all fence types: */
	struct nvgpu_ref ref;
	const struct nvgpu_fence_ops *ops;

	struct nvgpu_os_fence os_fence;

#ifdef CONFIG_NVGPU_SW_SEMAPHORE
	/* Valid for fences created from semaphores: */
	struct nvgpu_semaphore *semaphore;
	struct nvgpu_cond *semaphore_wq;
#endif

#ifdef CONFIG_TEGRA_GK20A_NVHOST
	/* Valid for fences created from syncpoints: */
	struct nvgpu_nvhost_dev *nvhost_device;
	u32 syncpt_id;
	u32 syncpt_value;
#endif
};

struct nvgpu_fence_type {
	/*
	 * struct nvgpu_fence_type needs to be allocated outside fence code for
	 * performance. It's technically possible to peek inside this priv
	 * data, but it's called priv for a reason. Don't touch it; use the
	 * public API (nvgpu_fence_*()).
	 */
	struct nvgpu_fence_type_priv priv;
};

/* Fence operations */
void nvgpu_fence_put(struct nvgpu_fence_type *f);
struct nvgpu_fence_type *nvgpu_fence_get(struct nvgpu_fence_type *f);
int  nvgpu_fence_wait(struct gk20a *g, struct nvgpu_fence_type *f, u32 timeout);
bool nvgpu_fence_is_expired(struct nvgpu_fence_type *f);
struct nvgpu_user_fence nvgpu_fence_extract_user(struct nvgpu_fence_type *f);

#endif /* NVGPU_FENCE_H */
