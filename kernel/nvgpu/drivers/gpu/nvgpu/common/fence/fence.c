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

#include <nvgpu/soc.h>
#include <nvgpu/os_fence.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/fence.h>
#include <nvgpu/user_fence.h>
#include "fence_priv.h"

static struct nvgpu_fence_type *nvgpu_fence_from_ref(struct nvgpu_ref *ref)
{
	return (struct nvgpu_fence_type *)((uintptr_t)ref -
				offsetof(struct nvgpu_fence_type, priv.ref));
}

static void nvgpu_fence_release(struct nvgpu_ref *ref)
{
	struct nvgpu_fence_type *f = nvgpu_fence_from_ref(ref);
	struct nvgpu_fence_type_priv *pf = &f->priv;

	if (nvgpu_os_fence_is_initialized(&pf->os_fence)) {
		pf->os_fence.ops->drop_ref(&pf->os_fence);
	}

	pf->ops->release(f);
}

void nvgpu_fence_put(struct nvgpu_fence_type *f)
{
	struct nvgpu_fence_type_priv *pf = &f->priv;

	nvgpu_ref_put(&pf->ref, nvgpu_fence_release);
}

struct nvgpu_fence_type *nvgpu_fence_get(struct nvgpu_fence_type *f)
{
	struct nvgpu_fence_type_priv *pf = &f->priv;

	nvgpu_ref_get(&pf->ref);
	return f;
}

/*
 * Extract an object to be passed to the userspace as a result of a submitted
 * job. This must be balanced with a call to nvgpu_user_fence_release().
 */
struct nvgpu_user_fence nvgpu_fence_extract_user(struct nvgpu_fence_type *f)
{
	struct nvgpu_fence_type_priv *pf = &f->priv;

	struct nvgpu_user_fence uf = (struct nvgpu_user_fence) {
#ifdef CONFIG_TEGRA_GK20A_NVHOST
		.syncpt_id = pf->syncpt_id,
		.syncpt_value = pf->syncpt_value,
#endif
		.os_fence = pf->os_fence,
	};

	/*
	 * The os fence member has to live so it can be signaled when the job
	 * completes. The returned user fence may live longer than that before
	 * being safely attached to an fd if the job completes before a
	 * submission ioctl finishes, or if it's stored for cde job state
	 * tracking.
	 */
	if (nvgpu_os_fence_is_initialized(&pf->os_fence)) {
		pf->os_fence.ops->dup(&pf->os_fence);
	}

	return uf;
}

int nvgpu_fence_wait(struct gk20a *g, struct nvgpu_fence_type *f,
							u32 timeout)
{
	struct nvgpu_fence_type_priv *pf = &f->priv;

	if (!nvgpu_platform_is_silicon(g)) {
		timeout = U32_MAX;
	}
	return pf->ops->wait(f, timeout);
}

bool nvgpu_fence_is_expired(struct nvgpu_fence_type *f)
{
	struct nvgpu_fence_type_priv *pf = &f->priv;

	return pf->ops->is_expired(f);
}

void nvgpu_fence_init(struct nvgpu_fence_type *f,
		const struct nvgpu_fence_ops *ops,
		struct nvgpu_os_fence os_fence)
{
	struct nvgpu_fence_type_priv *pf = &f->priv;

	nvgpu_ref_init(&pf->ref);
	pf->ops = ops;
	pf->os_fence = os_fence;
}
